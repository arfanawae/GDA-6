"""
Province boundaries (GeoJSON, WGS84) -> Unreal DataTable JSON (row struct FDTProvinceRow).

Usage:
  python build_provinces.py --boundaries geoBoundaries-THA-ADM1.geojson
  python build_provinces.py --boundaries ... --area TH-10,TH-12,TH-13 --minimap minimap_test.png

Outputs (default: Content/_Downtown/Data/Provinces/):
  DT_Provinces.json      import into the editor as a DataTable with row struct DTProvinceRow
  provinces_summary.csv  human-readable check (areas, reference points, sizes in game)
  map_bounds.json        game-space bounds of all provinces and of --area (for landscape/minimap)
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from shapely.geometry import MultiPolygon, Polygon, shape
import numpy as np
import shapely
from shapely.ops import unary_union

from geo_projection import GeoConfig, latlon_to_ue, ue_to_latlon

HERE = Path(__file__).resolve().parent
DEFAULT_OUT = HERE.parents[1] / "Content" / "_Downtown" / "Data" / "Provinces"


def load_meta() -> dict[str, dict]:
    with open(HERE / "thai_provinces.csv", encoding="utf-8") as f:
        return {row["code"]: row for row in csv.DictReader(f)}


def find_code(props: dict) -> str | None:
    """Accepts geoBoundaries (shapeISO 'TH-10') and OCHA/HDX (ADM1_PCODE 'TH10')."""
    for key in ("shapeISO", "ISO", "iso_3166_2", "ADM1_PCODE", "ADM1_PCODE_"):
        value = props.get(key)
        if value:
            value = str(value).upper()
            return value if "-" in value else f"{value[:2]}-{value[2:]}"
    return None


def to_game(geom, cfg: GeoConfig):
    # GeoJSON order is (lon, lat); output (X=north cm, Y=east cm)
    def project(coords: np.ndarray) -> np.ndarray:
        return np.array([latlon_to_ue(lat, lon, cfg) for lon, lat in coords[:, :2]])
    return shapely.transform(geom, project)


def real_km2(game_area_cm2: float, cfg: GeoConfig) -> float:
    return game_area_cm2 / 1e4 * cfg.horizontal_scale ** 2 / 1e6


def polygons(geom) -> list[Polygon]:
    if isinstance(geom, Polygon):
        return [geom]
    if isinstance(geom, MultiPolygon):
        return list(geom.geoms)
    return []


def ring_points(poly: Polygon) -> list[dict]:
    coords = list(poly.exterior.coords)[:-1]  # drop closing duplicate
    return [{"X": round(x, 1), "Y": round(y, 1)} for x, y in coords]


def bounds_dict(geom) -> dict:
    min_x, min_y, max_x, max_y = geom.bounds
    return {"MinX": round(min_x), "MinY": round(min_y), "MaxX": round(max_x), "MaxY": round(max_y),
            "SizeXKm": round((max_x - min_x) / 1e5, 2), "SizeYKm": round((max_y - min_y) / 1e5, 2)}


REGION_COLORS = {
    "North": (120, 170, 110), "Northeast": (205, 180, 110), "Central": (200, 140, 120),
    "East": (120, 160, 200), "West": (160, 130, 190), "South": (110, 180, 170),
}


def render_minimap(provinces: list[tuple[dict, object]], bounds, path: Path, px: int) -> None:
    from PIL import Image, ImageDraw

    min_x, min_y, max_x, max_y = bounds
    width = px
    height = max(1, round(px * (max_x - min_x) / (max_y - min_y)))  # image rows = north axis

    def to_px(x, y):
        return ((y - min_y) / (max_y - min_y) * (width - 1), (1.0 - (x - min_x) / (max_x - min_x)) * (height - 1))

    image = Image.new("RGB", (width, height), (40, 70, 110))  # sea
    draw = ImageDraw.Draw(image)
    for meta, geom in provinces:
        for poly in polygons(geom):
            pts = [to_px(x, y) for x, y in poly.exterior.coords]
            draw.polygon(pts, fill=REGION_COLORS.get(meta["region"], (180, 180, 180)), outline=(250, 250, 250))
    image.save(path)
    print(f"minimap: {path} ({width}x{height}px)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--boundaries", required=True, type=Path, help="ADM1 GeoJSON (WGS84)")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--simplify-m", type=float, default=10.0,
                        help="simplification tolerance in GAME metres (10 m game = 200 m real at 1:20)")
    parser.add_argument("--min-island-km2", type=float, default=5.0, help="drop islands smaller than this (real km2)")
    parser.add_argument("--area", default="", help="comma separated codes for a test area, e.g. TH-10,TH-12,TH-13")
    parser.add_argument("--minimap", type=Path, help="also render a PNG minimap of --area (or the whole country)")
    parser.add_argument("--minimap-px", type=int, default=2048)
    args = parser.parse_args()

    cfg = GeoConfig.load()
    meta = load_meta()
    data = json.loads(args.boundaries.read_text(encoding="utf-8"))

    projected: dict[str, object] = {}
    for feature in data["features"]:
        code = find_code(feature["properties"])
        if code not in meta:
            print(f"skip feature with unknown code {code!r}: {feature['properties']}")
            continue
        geom = to_game(shape(feature["geometry"]), cfg)
        projected[code] = geom.buffer(0) if not geom.is_valid else geom

    missing = sorted(set(meta) - set(projected))
    if missing:
        print("WARNING missing provinces:", missing)

    # Adjacency on unsimplified shapes (a small buffer closes slivers between datasets).
    touch_buffer = 20.0 * 100.0  # 20 m game
    buffered = {c: g.buffer(touch_buffer) for c, g in projected.items()}
    neighbors = {c: sorted(o for o in projected if o != c and buffered[c].intersects(projected[o])) for c in projected}

    tolerance = args.simplify_m * 100.0
    rows, summary, simplified_all = [], [], []
    for code in sorted(projected):
        geom = projected[code]
        info = meta[code]
        simple = geom.simplify(tolerance, preserve_topology=True)
        kept = [p for p in polygons(simple) if real_km2(p.area, cfg) >= args.min_island_km2]
        if not kept:  # never drop a whole province
            kept = [max(polygons(simple), key=lambda p: p.area)]
        holes = sum(len(p.interiors) for p in kept)
        if holes:
            print(f"note: {code} has {holes} hole(s); only outer rings are exported")

        mainland = max(kept, key=lambda p: p.area)
        ref = mainland.centroid if mainland.contains(mainland.centroid) else mainland.representative_point()

        ref_lat, ref_lon = ue_to_latlon(ref.x, ref.y, cfg)

        rows.append({
            "Name": code,
            "Code": code,
            "NameTH": info["name_th"],
            "NameEN": info["name_en"],
            "Region": info["region"],
            "ReferencePoint": {"X": round(ref.x, 1), "Y": round(ref.y, 1)},
            "ReferenceLatLon": {"X": round(ref_lat, 6), "Y": round(ref_lon, 6)},
            "RealAreaKm2": round(real_km2(geom.area, cfg), 1),
            "Neighbors": neighbors[code],
            "Rings": [{"Points": ring_points(p)} for p in kept],
        })
        b = bounds_dict(geom)
        summary.append([code, info["name_th"], info["name_en"], info["region"], round(real_km2(geom.area, cfg), 1),
                        round(ref_lat, 5), round(ref_lon, 5), b["SizeXKm"], b["SizeYKm"],
                        sum(len(p.exterior.coords) for p in kept), " ".join(neighbors[code])])
        simplified_all.append((info, unary_union(kept)))

    args.out_dir.mkdir(parents=True, exist_ok=True)
    table_path = args.out_dir / "DT_Provinces.json"
    table_path.write_text(json.dumps(rows, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")

    with open(args.out_dir / "provinces_summary.csv", "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["code", "name_th", "name_en", "region", "real_area_km2", "ref_lat", "ref_lon",
                         "game_size_north_km", "game_size_east_km", "vertices", "neighbors"])
        writer.writerows(summary)

    country = unary_union(list(projected.values()))
    bounds = {"config": cfg.__dict__, "country": bounds_dict(country)}
    area_codes = [c.strip().upper() for c in args.area.split(",") if c.strip()]
    if area_codes:
        area_geom = unary_union([projected[c] for c in area_codes])
        bounds["area"] = {"codes": area_codes, **bounds_dict(area_geom)}
    (args.out_dir / "map_bounds.json").write_text(json.dumps(bounds, indent=2), encoding="utf-8")

    print(f"{len(rows)} provinces -> {table_path} ({table_path.stat().st_size / 1024:.0f} KB)")
    print("country in game:", bounds["country"])
    if "area" in bounds:
        print("test area in game:", bounds["area"])

    if args.minimap:
        selection = [(i, g) for (i, g) in simplified_all if not area_codes or i["code"] in area_codes]
        box = unary_union([g for _, g in selection]).bounds
        render_minimap(selection, box, args.minimap, args.minimap_px)
        # Copy these into BP_GameMode.MinimapWorldBounds so the texture lines up with the world.
        bounds["minimap"] = {"image": args.minimap.name, "MinX": round(box[0]), "MinY": round(box[1]),
                             "MaxX": round(box[2]), "MaxY": round(box[3])}
        (args.out_dir / "map_bounds.json").write_text(json.dumps(bounds, indent=2), encoding="utf-8")
        print("MinimapWorldBounds:", bounds["minimap"])


if __name__ == "__main__":
    main()
