"""
OpenStreetMap roads -> game-space road polylines + province border crossings.

Input (any of):
  * HOT/HDX GeoParquet  (hotosm THA roads.parquet, whole country, ODbL)
  * GeoJSON LineStrings  (e.g. `osmium export` of a Geofabrik extract)
  * Overpass JSON        (`out geom;`)

Usage (test area = the provinces of DT_Provinces.json you list):
  python build_roads.py --input roads.parquet --area TH-10,TH-12,TH-13 --name TestArea

Output: Content/_Downtown/Data/Roads/Roads_<name>.json
  {
    "config":    projection constants (must match UDTGeoSettings),
    "roads":     [{id, name_th, name_en, highway, oneway, lanes, speed_kmh, points: [[X,Y,level],...]}],
    "crossings": [{x, y, heading, from, to, road}]     # where a road leaves one province for another
  }
Coordinates are Unreal cm (X = north, Y = east). Thailand drives on the LEFT; the importer
offsets lanes accordingly.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import shapely
from shapely.geometry import LineString, MultiLineString, Point, Polygon, shape
from shapely.ops import unary_union
from shapely.strtree import STRtree

from geo_projection import GeoConfig, latlon_to_ue, ue_to_latlon

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
PROVINCES = PROJECT / "Content" / "_Downtown" / "Data" / "Provinces" / "DT_Provinces.json"
OUT_DIR = PROJECT / "Content" / "_Downtown" / "Data" / "Roads"

# Real-world default speeds (km/h). Vehicles keep real speeds; only distances are scaled.
DEFAULT_SPEED = {"motorway": 120, "trunk": 90, "primary": 80, "secondary": 60, "tertiary": 50}
DEFAULT_LANES = {"motorway": 2, "trunk": 2, "primary": 1, "secondary": 1, "tertiary": 1}


def base_class(highway: str) -> str:
    return highway.replace("_link", "")


# ---------------------------------------------------------------- input readers
def read_parquet(path: Path, classes: set[str], bbox_ll) -> list[dict]:
    import pyarrow.compute as pc
    import pyarrow.parquet as pq

    cols = ["id", "name", "name_th", "name_en", "highway", "lanes", "oneway", "bridge", "layer", "bbox", "geom"]
    table = pq.read_table(path, columns=cols, filters=[("highway", "in", sorted(classes))])
    if bbox_ll:
        lon0, lat0, lon1, lat1 = bbox_ll
        b = table.column("bbox")
        mask = pc.and_(pc.and_(pc.less_equal(pc.struct_field(b, "xmin"), lon1), pc.greater_equal(pc.struct_field(b, "xmax"), lon0)),
                       pc.and_(pc.less_equal(pc.struct_field(b, "ymin"), lat1), pc.greater_equal(pc.struct_field(b, "ymax"), lat0)))
        table = table.filter(mask)
    rows = table.to_pylist()
    for r in rows:
        r["geometry"] = shapely.from_wkb(r.pop("geom"))
    return rows


def read_geojson(path: Path, classes: set[str]) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = []
    for f in data["features"]:
        p = f.get("properties") or {}
        if p.get("highway") in classes:
            rows.append({**p, "id": p.get("id") or p.get("@id"), "geometry": shape(f["geometry"])})
    return rows


def read_overpass(path: Path, classes: set[str]) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = []
    for e in data.get("elements", []):
        tags = e.get("tags", {})
        if e.get("type") == "way" and tags.get("highway") in classes and e.get("geometry"):
            rows.append({**tags, "id": str(e["id"]), "geometry": LineString([(g["lon"], g["lat"]) for g in e["geometry"]])})
    return rows


def read_roads(path: Path, classes: set[str], bbox_ll) -> list[dict]:
    if path.suffix == ".parquet":
        return read_parquet(path, classes, bbox_ll)
    data_head = path.read_text(encoding="utf-8")[:200]
    return read_overpass(path, classes) if '"elements"' in data_head or "osm3s" in data_head else read_geojson(path, classes)


# ---------------------------------------------------------------- helpers
def to_game(geom, cfg: GeoConfig):
    import numpy as np
    return shapely.transform(geom, lambda c: np.array([latlon_to_ue(lat, lon, cfg) for lon, lat in c[:, :2]]))


def lines_of(geom) -> list[LineString]:
    if isinstance(geom, LineString):
        return [geom] if geom.length > 0 else []
    if isinstance(geom, MultiLineString):
        return [g for g in geom.geoms if g.length > 0]
    if hasattr(geom, "geoms"):
        return [g for sub in geom.geoms for g in lines_of(sub)]
    return []


def parse_int(value, default: int) -> int:
    try:
        return max(1, int(str(value).split(";")[0]))
    except (TypeError, ValueError):
        return default


def load_province_polygons() -> tuple[dict[str, Polygon], dict]:
    rows = json.loads(PROVINCES.read_text(encoding="utf-8"))
    polys = {}
    for r in rows:
        rings = [Polygon([(p["X"], p["Y"]) for p in ring["Points"]]) for ring in r["Rings"]]
        polys[r["Code"]] = unary_union([g if g.is_valid else g.buffer(0) for g in rings])
    return polys, {r["Code"]: r for r in rows}


def heading_deg(line: LineString, d: float) -> float:
    a = line.interpolate(max(0.0, d - 500.0))
    b = line.interpolate(min(line.length, d + 500.0))
    return math.degrees(math.atan2(b.y - a.y, b.x - a.x))  # UE yaw: 0 = north (+X), 90 = east (+Y)


# ---------------------------------------------------------------- main
def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--area", required=True, help="province codes, e.g. TH-10,TH-12,TH-13")
    parser.add_argument("--name", default="TestArea")
    parser.add_argument("--classes", default="motorway,trunk,primary",
                        help="OSM highway classes (links are added automatically)")
    parser.add_argument("--margin-m", type=float, default=150.0, help="keep roads this far (game m) outside the area")
    parser.add_argument("--simplify-m", type=float, default=1.5, help="game metres (1.5 m game = 30 m real)")
    parser.add_argument("--min-length-m", type=float, default=15.0, help="drop fragments shorter than this (game m)")
    args = parser.parse_args()

    cfg = GeoConfig.load()
    classes = {c.strip() for c in args.classes.split(",") if c.strip()}
    classes |= {f"{c}_link" for c in classes}

    polys, rows = load_province_polygons()
    area_codes = [c.strip().upper() for c in args.area.split(",")]
    area = unary_union([polys[c] for c in area_codes])
    clip = area.buffer(args.margin_m * 100.0)

    # lat/lon bbox of the clip region for a fast pre-filter
    min_x, min_y, max_x, max_y = clip.bounds
    corners = [ue_to_latlon(x, y, cfg) for x in (min_x, max_x) for y in (min_y, max_y)]
    bbox_ll = (min(c[1] for c in corners) - 0.01, min(c[0] for c in corners) - 0.01,
               max(c[1] for c in corners) + 0.01, max(c[0] for c in corners) + 0.01)

    raw = read_roads(args.input, classes, bbox_ll)
    print(f"read {len(raw)} OSM ways ({', '.join(sorted(classes))})")

    # 1) project + clip + normalise attributes, 2) merge pieces that OSM split at every tag change
    groups: dict[tuple, list[LineString]] = {}
    lanes_of: dict[tuple, int] = {}
    ids: dict[tuple, str] = {}
    for r in raw:
        highway = r.get("highway") or ""
        cls = base_class(highway)
        oneway_tag = str(r.get("oneway") or "").lower()
        oneway = oneway_tag in ("yes", "true", "1", "-1") or highway in ("motorway", "motorway_link")
        # Elevation level, stored as Z of every vertex: only explicitly layered motorways/trunks
        # (Bangkok's elevated expressways). Small canal bridges stay on the ground.
        layer_tag = str(r.get("layer") or "0")
        level = int(layer_tag) if layer_tag.lstrip("-").isdigit() and cls in ("motorway", "trunk") else 0
        level = max(0, min(level, 4))
        key = (highway, r.get("name_th") or r.get("name") or "", r.get("name_en") or "", oneway)
        game = to_game(r["geometry"], cfg)
        if oneway_tag == "-1":
            game = shapely.reverse(game)
        game = shapely.force_3d(game, level)
        for line in lines_of(game.intersection(clip)):
            groups.setdefault(key, []).append(line)
            ids.setdefault(key, str(r.get("id")))
            lanes_of[key] = max(lanes_of.get(key, 1), parse_int(r.get("lanes"), DEFAULT_LANES.get(cls, 1) * (1 if oneway else 2)))

    roads = []
    for key, pieces in groups.items():
        highway, name_th, name_en, oneway = key
        merged = shapely.line_merge(MultiLineString(pieces), directed=oneway)
        for line in lines_of(merged):
            line = line.simplify(args.simplify_m * 100.0)
            if line.length < args.min_length_m * 100.0:
                continue
            coords = [(c[0], c[1], c[2] if len(c) > 2 and c[2] == c[2] else 0.0) for c in line.coords]
            roads.append({
                "id": ids[key],
                "name_th": name_th,
                "name_en": name_en,
                "highway": highway,
                "oneway": oneway,
                "lanes": lanes_of[key],
                "speed_kmh": DEFAULT_SPEED.get(base_class(highway), 50),
                # [X, Y, level]; level > 0 = elevated expressway, importer raises Z per level
                "points": [[round(x), round(y), int(round(z))] for x, y, z in coords],
            })

    # Border crossings: where each road passes from one province polygon into another.
    nearby = {c: p for c, p in polys.items() if p.intersects(clip)}
    tree_codes = list(nearby)
    tree = STRtree([nearby[c] for c in tree_codes])
    borders = unary_union([p.boundary for p in nearby.values()])

    def province_at(pt) -> str | None:
        for i in tree.query(pt, predicate="intersects"):
            return tree_codes[i]
        return None

    crossings, seen = [], set()
    for road in roads:
        line = LineString([p[:2] for p in road["points"]])
        hits = line.intersection(borders)
        points = [hits] if hits.geom_type == "Point" else [g for g in getattr(hits, "geoms", []) if g.geom_type == "Point"]
        for pt in points:
            d = line.project(pt)
            before = province_at(line.interpolate(max(0.0, d - 3000.0)))
            after = province_at(line.interpolate(min(line.length, d + 3000.0)))
            if not before or not after or before == after:
                continue
            directions = [(before, after, 0.0)] if road["oneway"] else [(before, after, 0.0), (after, before, 180.0)]
            for src, dst, flip in directions:
                key = (src, dst, round(pt.x / 5000.0), round(pt.y / 5000.0))  # dedupe within ~50 m game
                if key in seen:
                    continue
                heading = (heading_deg(line, d) + flip + 180.0) % 360.0 - 180.0
                # Skip roads that run along the border (sign would be ambiguous): straight ahead/behind
                # must really be the two provinces, as the runtime point-in-polygon check will see it.
                fx, fy = math.cos(math.radians(heading)), math.sin(math.radians(heading))
                if (province_at(Point(pt.x - fx * 3000.0, pt.y - fy * 3000.0)) != src or
                        province_at(Point(pt.x + fx * 3000.0, pt.y + fy * 3000.0)) != dst):
                    continue
                seen.add(key)
                crossings.append({
                    "x": round(pt.x), "y": round(pt.y),
                    "heading": round(heading, 1),
                    "from": src, "to": dst,
                    "road": road["name_th"] or road["name_en"] or road["highway"],
                })

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out = OUT_DIR / f"Roads_{args.name}.json"
    out.write_text(json.dumps({
        "config": cfg.__dict__,
        "area": area_codes,
        "bounds": {"MinX": round(min_x), "MinY": round(min_y), "MaxX": round(max_x), "MaxY": round(max_y)},
        "roads": roads,
        "crossings": crossings,
    }, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")

    total_km = sum(LineString([p[:2] for p in r["points"]]).length for r in roads) / 1e5
    by_class: dict[str, int] = {}
    for r in roads:
        by_class[r["highway"]] = by_class.get(r["highway"], 0) + 1
    print(f"{len(roads)} road pieces, {total_km:.1f} km in game ({total_km * cfg.horizontal_scale:.0f} km real): {by_class}")
    print(f"{len(crossings)} border-crossing signs -> {out} ({out.stat().st_size / 1024:.0f} KB)")


if __name__ == "__main__":
    main()
