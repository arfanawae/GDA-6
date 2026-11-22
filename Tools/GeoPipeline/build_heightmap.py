"""
Real elevation (Copernicus GLO-30 DEM) -> Unreal Landscape heightmap for a zone of the 1:20 Thailand map.

Usage:
  python build_heightmap.py --area TH-50,TH-51,TH-52 --name North3 --download
  python build_heightmap.py --area test-area --name TestArea --download --max-quad-m 2
  python build_heightmap.py --area TH-50,TH-51,TH-52 --name North3 --roads ../../Content/_Downtown/Data/Roads/Roads_North3.json

Outputs (default raw/heightmaps/, keep out of git):
  <name>_heightmap.r16 / .png   16-bit heightmap for Landscape Mode > Import from File
  <name>_landscape.json         every number to type into the Landscape import dialog
  <name>_preview.png            north-up hillshade + province borders (+ roads) to eyeball the result

Height convention (same as geo_projection.elevation_to_ue_z): world Z = metres / vertical_scale * 100,
sea level = Z 0. The landscape Location.Z / Scale.Z are chosen so that this holds exactly.

Note: Copernicus GLO-30 is a SURFACE model (DSM): cities include building heights (+10..40 m) and forests their
canopy. Fine for mountains; for dense flat cities (Bangkok) a flat landscape is usually better.

DEM: Copernicus DEM GLO-30, (c) DLR e.V. 2010-2014 and (c) Airbus Defence and Space GmbH 2014-2018,
provided under COPERNICUS by the European Union and ESA; free to use with attribution.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import urllib.error
import urllib.request
from pathlib import Path

try:
    import numpy as np
except ImportError:
    sys.exit("[ERROR] Missing package numpy:  pip install numpy")
try:
    import rasterio
    from rasterio.enums import Resampling
    from rasterio.merge import merge
except ImportError:
    sys.exit("[ERROR] Missing package rasterio:  pip install rasterio   (Windows wheels include GDAL)")
try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("[ERROR] Missing package Pillow:  pip install Pillow")

from geo_projection import EARTH_RADIUS_M, GeoConfig

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
PROVINCES = PROJECT / "Content" / "_Downtown" / "Data" / "Provinces" / "DT_Provinces.json"
DEM_URL = "https://copernicus-dem-30m.s3.amazonaws.com/{name}/{name}.tif"

# Epic's recommended landscape sizes: total vertices -> (quads per section, sections per component, components)
LANDSCAPE_SIZES = [
    (127, (63, 2, 1)), (253, (63, 2, 2)), (505, (63, 2, 4)), (1009, (63, 2, 8)),
    (2017, (63, 2, 16)), (4033, (63, 2, 32)), (8129, (127, 2, 32)),
]
ZONE_PRESETS = {"bangkok-metro": "TH-10,TH-11,TH-12,TH-13,TH-73,TH-74", "test-area": "TH-10,TH-12,TH-13"}


# ----------------------------------------------------------------------------- projection (vectorised)
def ue_to_latlon_np(x_cm: np.ndarray, y_cm: np.ndarray, cfg: GeoConfig) -> tuple[np.ndarray, np.ndarray]:
    """Vectorised inverse of geo_projection.latlon_to_ue (spherical Transverse Mercator)."""
    s = cfg.horizontal_scale / 100.0
    easting, northing = y_cm * s, x_cm * s
    d = northing / EARTH_RADIUS_M + math.radians(cfg.origin_lat)
    xr = easting / EARTH_RADIUS_M
    lat = np.degrees(np.arcsin(np.sin(d) / np.cosh(xr)))
    lon = cfg.origin_lon + np.degrees(np.arctan2(np.sinh(xr), np.cos(d)))
    return lat, lon


# ----------------------------------------------------------------------------- DEM tiles
def tile_name(lat: int, lon: int) -> str:
    ns = "N" if lat >= 0 else "S"
    ew = "E" if lon >= 0 else "W"
    return f"Copernicus_DSM_COG_10_{ns}{abs(lat):02d}_00_{ew}{abs(lon):03d}_00_DEM"


def ensure_tiles(lat0: float, lon0: float, lat1: float, lon1: float, dem_dir: Path, download: bool) -> list[Path]:
    dem_dir.mkdir(parents=True, exist_ok=True)
    paths, missing = [], []
    for lat in range(math.floor(lat0), math.floor(lat1) + 1):
        for lon in range(math.floor(lon0), math.floor(lon1) + 1):
            name = tile_name(lat, lon)
            path = dem_dir / f"{name}.tif"
            nodata_marker = dem_dir / f"{name}.none"  # remembered: no tile exists (open sea)
            if path.is_file():
                paths.append(path)
                continue
            if nodata_marker.is_file():
                continue
            if not download:
                missing.append(name)
                continue
            url = DEM_URL.format(name=name)
            print(f"  downloading {name} ...", flush=True)
            try:
                tmp = path.with_suffix(".part")
                urllib.request.urlretrieve(url, tmp)
                tmp.replace(path)
                paths.append(path)
            except urllib.error.HTTPError as exc:
                if exc.code in (403, 404):  # no land in this tile
                    nodata_marker.write_text("no tile")
                    print(f"  {name}: no tile (sea) - height 0")
                else:
                    sys.exit(f"[ERROR] download failed for {url}: HTTP {exc.code}")
            except urllib.error.URLError as exc:
                sys.exit(f"[ERROR] cannot reach {url}: {exc.reason}")
    if missing:
        sys.exit("[ERROR] Missing DEM tiles in {}:\n  {}\nRun again with --download, or fetch them from\n  {}".format(
            dem_dir, "\n  ".join(missing), DEM_URL.format(name="<tile>")))
    return paths


def load_dem(paths: list[Path], bounds_ll, res_deg: float):
    """Mosaic + resample the tiles to res_deg. Returns (array[rows, cols] metres, affine transform)."""
    if not paths:
        return None, None
    sources = [rasterio.open(p) for p in paths]
    try:
        lon0, lat0, lon1, lat1 = bounds_ll
        data, transform = merge(sources, bounds=(lon0, lat0, lon1, lat1), res=(res_deg, res_deg),
                                resampling=Resampling.bilinear, nodata=-32767.0)
    finally:
        for s in sources:
            s.close()
    dem = data[0].astype(np.float32)
    dem[(dem <= -1000) | ~np.isfinite(dem)] = 0.0  # nodata / sea
    return dem, transform


def sample_bilinear(dem: np.ndarray, transform, lat: np.ndarray, lon: np.ndarray) -> np.ndarray:
    inv = ~transform
    col, row = inv * (lon, lat)
    col = np.clip(col - 0.5, 0, dem.shape[1] - 1.001)
    row = np.clip(row - 0.5, 0, dem.shape[0] - 1.001)
    c0, r0 = np.floor(col).astype(np.int64), np.floor(row).astype(np.int64)
    fc, fr = col - c0, row - r0
    top = dem[r0, c0] * (1 - fc) + dem[r0, c0 + 1] * fc
    bottom = dem[r0 + 1, c0] * (1 - fc) + dem[r0 + 1, c0 + 1] * fc
    return top * (1 - fr) + bottom * fr


# ----------------------------------------------------------------------------- main
def resolve_area(area: str, rows: list[dict]) -> list[str]:
    area = ZONE_PRESETS.get(area.strip().lower(), area)
    regions = {r["Region"].lower(): r["Region"] for r in rows}
    if area.strip().lower() in regions:
        return [r["Code"] for r in rows if r["Region"] == regions[area.strip().lower()]]
    by_name = {}
    for r in rows:
        by_name[r["Code"].upper()] = r["Code"]
        by_name[r["NameEN"].lower().replace(" ", "")] = r["Code"]
        by_name[r["NameTH"]] = r["Code"]
    codes = []
    for token in area.split(","):
        key = token.strip()
        code = by_name.get(key.upper()) or by_name.get(key.lower().replace(" ", "")) or by_name.get(key)
        if not code:
            sys.exit(f"[ERROR] unknown province/zone '{key}'")
        codes.append(code)
    return codes


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--area", required=True, help="codes / names / region / preset, e.g. TH-50,TH-51,TH-52")
    parser.add_argument("--name", required=True)
    parser.add_argument("--dem-dir", type=Path, default=HERE / "raw" / "dem")
    parser.add_argument("--out-dir", type=Path, default=HERE / "raw" / "heightmaps")
    parser.add_argument("--download", action="store_true", help="download missing Copernicus tiles (~45 MB each)")
    parser.add_argument("--max-quad-m", type=float, default=5.0, help="largest landscape quad size in GAME metres")
    parser.add_argument("--margin-m", type=float, default=300.0, help="extra landscape around the zone (game metres)")
    parser.add_argument("--vertical-scale", type=float, help="override geo_config vertical_scale for this run")
    parser.add_argument("--roads", type=Path, help="Roads_<name>.json: report road gradients in game")
    parser.add_argument("--min-height-m", type=float, default=0.0,
                        help="clamp heights below this (real m). Copernicus is a surface model: water edges can be negative")
    args = parser.parse_args()

    cfg = GeoConfig.load()
    if args.vertical_scale:
        cfg = GeoConfig(cfg.origin_lat, cfg.origin_lon, cfg.horizontal_scale, args.vertical_scale)
    if not PROVINCES.is_file():
        sys.exit(f"[ERROR] {PROVINCES} not found - run build_provinces.py first")
    rows = json.loads(PROVINCES.read_text(encoding="utf-8"))
    codes = resolve_area(args.area, rows)
    sel = [r for r in rows if r["Code"] in codes]

    # --- landscape grid --------------------------------------------------------------------------
    xs = [p["X"] for r in sel for ring in r["Rings"] for p in ring["Points"]]
    ys = [p["Y"] for r in sel for ring in r["Rings"] for p in ring["Points"]]
    margin = args.margin_m * 100.0
    min_x, max_x, min_y, max_y = min(xs) - margin, max(xs) + margin, min(ys) - margin, max(ys) + margin
    extent = max(max_x - min_x, max_y - min_y)
    for verts, layout in LANDSCAPE_SIZES:
        if extent / (verts - 1) <= args.max_quad_m * 100.0:
            break
    quad_cm = math.ceil(extent / (verts - 1))
    size_cm = quad_cm * (verts - 1)
    cx, cy = (min_x + max_x) / 2, (min_y + max_y) / 2
    loc_x, loc_y = cx - size_cm / 2, cy - size_cm / 2
    print(f"zone {', '.join(codes)}: {(max_x-min_x)/1e5:.2f} x {(max_y-min_y)/1e5:.2f} km in game (incl. margin)")
    print(f"landscape {verts} x {verts} vertices, quad {quad_cm/100:.2f} m game ({quad_cm/100*cfg.horizontal_scale:.0f} m real), size {size_cm/1e5:.2f} km")

    # vertex world coordinates: image column = landscape local X (north), image row = local Y (east)
    gx = loc_x + np.arange(verts, dtype=np.float64) * quad_cm
    gy = loc_y + np.arange(verts, dtype=np.float64) * quad_cm
    wx, wy = np.meshgrid(gx, gy)  # shape [row=Y, col=X]
    lat, lon = ue_to_latlon_np(wx, wy, cfg)

    # --- DEM -------------------------------------------------------------------------------------
    real_quad_m = quad_cm / 100.0 * cfg.horizontal_scale
    res_deg = max(1.0 / 3600.0, real_quad_m / 2.0 / 111_320.0)  # sample at ~half a quad, never finer than the DEM
    bounds_ll = (float(lon.min()) - 0.01, float(lat.min()) - 0.01, float(lon.max()) + 0.01, float(lat.max()) + 0.01)
    tiles = ensure_tiles(bounds_ll[1], bounds_ll[0], bounds_ll[3], bounds_ll[2], args.dem_dir, args.download)
    print(f"DEM: {len(tiles)} tile(s), resampled to {res_deg*111_320:.0f} m real")
    dem, transform = load_dem(tiles, bounds_ll, res_deg)
    height_m = sample_bilinear(dem, transform, lat, lon) if dem is not None else np.zeros_like(lat, dtype=np.float32)
    height_m = np.maximum(height_m, args.min_height_m)

    # --- encode for Unreal -------------------------------------------------------------------------
    z_cm = height_m.astype(np.float64) * 100.0 / cfg.vertical_scale
    z_min, z_max = float(z_cm.min()), float(z_cm.max())
    # 16-bit range: keep ~1000 steps of headroom below/above for sculpting
    z_scale = max(1.0, math.ceil((z_max - z_min + 1.0) * 128.0 / 63535.0 * 100.0) / 100.0)
    loc_z = z_min + (32768.0 - 1000.0) * z_scale / 128.0
    h16 = np.clip(np.round((z_cm - loc_z) * 128.0 / z_scale + 32768.0), 0, 65535).astype(np.uint16)
    check = loc_z + (h16.astype(np.float64) - 32768.0) * z_scale / 128.0
    max_err_cm = float(np.abs(check - z_cm).max())

    args.out_dir.mkdir(parents=True, exist_ok=True)
    r16 = args.out_dir / f"{args.name}_heightmap.r16"
    png = args.out_dir / f"{args.name}_heightmap.png"
    h16.astype("<u2").tofile(r16)
    Image.fromarray(h16).save(png)  # uint16 array -> 16-bit grayscale PNG

    qps, spc, comps = layout
    settings = {
        "zone": codes,
        "config": cfg.__dict__,
        "heightmap": {"r16": r16.name, "png": png.name, "resolution": verts},
        "landscape": {
            "Location": {"X": round(loc_x), "Y": round(loc_y), "Z": round(loc_z, 2)},
            "Scale": {"X": quad_cm, "Y": quad_cm, "Z": z_scale},
            "SectionSize": f"{qps}x{qps} quads", "SectionsPerComponent": f"{spc}x{spc}",
            "NumberOfComponents": f"{comps}x{comps}", "OverallResolution": f"{verts}x{verts}",
        },
        "heights": {"real_min_m": round(float(height_m.min()), 1), "real_max_m": round(float(height_m.max()), 1),
                    "game_min_cm": round(z_min), "game_max_cm": round(z_max), "max_encoding_error_cm": round(max_err_cm, 2)},
    }

    # --- road gradients ---------------------------------------------------------------------------
    if args.roads:
        settings["road_gradients"] = road_gradients(args.roads, dem, transform, cfg)

    (args.out_dir / f"{args.name}_landscape.json").write_text(json.dumps(settings, indent=2, ensure_ascii=False), encoding="utf-8")
    preview(args.out_dir / f"{args.name}_preview.png", height_m, sel, (loc_x, loc_y, quad_cm), args.roads)

    L = settings["landscape"]
    print(f"real height {settings['heights']['real_min_m']} .. {settings['heights']['real_max_m']} m "
          f"-> game Z {z_min:.0f} .. {z_max:.0f} cm (1:{cfg.vertical_scale:g}), encoding error <= {max_err_cm:.2f} cm")
    print("\nLandscape Mode > New > Import from File:")
    print(f"  Heightmap File       {r16}")
    print(f"  Location             X={L['Location']['X']}  Y={L['Location']['Y']}  Z={L['Location']['Z']}")
    print(f"  Scale                X={quad_cm}  Y={quad_cm}  Z={z_scale}")
    print(f"  Section Size         {L['SectionSize']}   Sections Per Component {L['SectionsPerComponent']}")
    print(f"  Number of Components {L['NumberOfComponents']}   Overall Resolution {L['OverallResolution']}")
    print(f"\nwrote {r16.name}, {png.name}, {args.name}_landscape.json, {args.name}_preview.png in {args.out_dir}")


def road_gradients(roads_path: Path, dem, transform, cfg: GeoConfig) -> dict:
    """Real gradients along the roads; game gradient = real * horizontal_scale / vertical_scale."""
    data = json.loads(roads_path.read_text(encoding="utf-8"))
    step_real_m = 200.0  # DEM noise is too large below ~200 m
    grades, lengths = [], []
    for road in data.get("roads", []):
        if max(p[2] for p in road["points"]) > 0:  # elevated structures do not follow the terrain
            continue
        pts = np.array([p[:2] for p in road["points"]], dtype=np.float64)
        seg = np.hypot(*np.diff(pts, axis=0).T)
        total = seg.sum()
        if total * cfg.horizontal_scale / 100.0 < step_real_m * 2:
            continue
        dist = np.concatenate([[0.0], np.cumsum(seg)])
        n = int(total * cfg.horizontal_scale / 100.0 // step_real_m)
        d = np.linspace(0.0, total, n + 1)
        x, y = np.interp(d, dist, pts[:, 0]), np.interp(d, dist, pts[:, 1])
        lat, lon = ue_to_latlon_np(x, y, cfg)
        h = sample_bilinear(dem, transform, lat, lon)
        step = total / n * cfg.horizontal_scale / 100.0
        grades.extend(np.abs(np.diff(h)) / step)
        lengths.extend([step] * n)
    if not grades:
        return {}
    g, w = np.array(grades), np.array(lengths)
    order = np.argsort(g)
    cum = np.cumsum(w[order]) / w.sum()

    def pct(p):
        return float(g[order][np.searchsorted(cum, p)])

    report = {"sampled_real_km": round(float(w.sum()) / 1000.0, 1),
              "real_grade_pct": {"p50": round(pct(0.5) * 100, 1), "p90": round(pct(0.9) * 100, 1), "p99": round(pct(0.99) * 100, 1)},
              "game_by_vertical_scale": {}}
    print(f"\nroad gradients over {report['sampled_real_km']} km of ground-level road (sampled every {step_real_m:.0f} m real):")
    print(f"  real          p50 {report['real_grade_pct']['p50']:5.1f}%  p90 {report['real_grade_pct']['p90']:5.1f}%  p99 {report['real_grade_pct']['p99']:5.1f}%")
    for vs in sorted({5.0, 10.0, 15.0, 20.0, cfg.vertical_scale}):
        f = cfg.horizontal_scale / vs
        steep = float(w[g * f > 0.15].sum() / w.sum() * 100)
        row = {"p50": round(pct(0.5) * f * 100, 1), "p90": round(pct(0.9) * f * 100, 1), "p99": round(pct(0.99) * f * 100, 1),
               "share_steeper_than_15pct": round(steep, 1)}
        report["game_by_vertical_scale"][f"1:{vs:g}"] = row
        mark = "  <- current" if vs == cfg.vertical_scale else ""
        print(f"  game 1:{vs:<4g}  p50 {row['p50']:5.1f}%  p90 {row['p90']:5.1f}%  p99 {row['p99']:5.1f}%  "
              f"road >15%: {steep:4.1f}%{mark}")
    return report


def preview(path: Path, height_m: np.ndarray, provinces: list[dict], grid, roads_path: Path | None) -> None:
    """North-up hillshade with province outlines (image up = +X north, right = +Y east)."""
    loc_x, loc_y, quad = grid
    h = height_m.T[::-1]  # [row=Y,col=X] -> north up
    dy, dx = np.gradient(h.astype(np.float64))
    shade = np.clip(0.6 + (dx - dy) / (np.percentile(np.abs(dx) + np.abs(dy), 99) + 1e-6) * 0.4, 0, 1)
    hn = (h - h.min()) / (np.ptp(h) + 1e-6)
    rgb = np.stack([0.35 + 0.5 * hn, 0.55 + 0.25 * hn, 0.35 + 0.35 * hn], axis=-1) * shade[..., None]
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8))
    n = h.shape[0]
    scale = 1024 / n
    img = img.resize((max(1, round(n * scale)), max(1, round(n * scale))), Image.BILINEAR)
    draw = ImageDraw.Draw(img)

    def px(x, y):
        return ((y - loc_y) / quad * scale, (n - 1 - (x - loc_x) / quad) * scale)

    for r in provinces:
        for ring in r["Rings"]:
            draw.line([px(p["X"], p["Y"]) for p in ring["Points"]] + [px(ring["Points"][0]["X"], ring["Points"][0]["Y"])],
                      fill=(255, 255, 255), width=2)
    if roads_path:
        for road in json.loads(roads_path.read_text(encoding="utf-8")).get("roads", []):
            color = (230, 60, 50) if road["highway"].startswith(("motorway", "trunk")) else (250, 210, 70)
            draw.line([px(p[0], p[1]) for p in road["points"]], fill=color, width=1)
    img.save(path)


if __name__ == "__main__":
    main()
