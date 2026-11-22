"""
prepare_geodata.py - clean, filter and split raw Thai map data before it goes into the
Downtown (UE 5.5, 1:20) pipeline (build_provinces.py / build_roads.py).

Reads every supported file in a folder (or a single file):
    .geojson / .json   GeoJSON FeatureCollection
    .shp               ESRI Shapefile (needs: pip install pyshp)
    .csv               points: lat/lon columns, or a WKT geometry column

Detects what each file contains (province polygons, road lines or points), keeps only the
fields the game needs, repairs or drops bad geometry, and writes one GeoJSON per zone
(a single province, Bangkok + vicinity, a region, or all 77 provinces separately), then
prints a before/after summary.

----------------------------------------------------------------------------------------
ASSUMPTIONS ABOUT THE INPUT DATA (read this first)
----------------------------------------------------------------------------------------
1. Coordinates are WGS84 lon/lat (EPSG:4326), as in OSM, geoBoundaries and HDX exports.
   Projected data (e.g. Thai government shapefiles in UTM 47N/48N, EPSG:32647/32648) is
   detected by its coordinate range and reprojected IF the .prj (or GeoJSON "crs") is present
   and pyproj is installed; otherwise the file is skipped with an explanation.
2. Valid area is Thailand plus a margin: lon 97.0..106.0, lat 5.0..21.0. Geometry entirely
   outside is dropped. If most points have lat/lon swapped (lat ~100, lon ~13) the file is
   auto-corrected and a warning is printed.
3. Province polygons carry a province code or name in one of these fields (first match wins):
       code : shapeISO, ISO, iso_3166_2, ADM1_PCODE, adm1_pcode, PROV_CODE, pro_code, P_CODE, CODE
       name : ADM1_TH, ADM1_EN, adm1_name, shapeName, PROV_NAMT, PROV_NAME, PROV_NAMEE,
              name_th, name_en, NAME_TH, NAME_EN, name, NAME
   Codes may look like "TH-10", "TH10", "10" or 10 (ISO 3166-2:TH numbers = DOPA province
   numbers). Names are matched in Thai or English, ignoring spaces, case, "จังหวัด"/"Province"
   and common spelling variants (Lopburi / Lop Buri, Phang Nga / Phangnga, ...).
4. Road lines carry an OSM class in "highway" (OSM / HOT / HDX exports) or "fclass"
   (Geofabrik shapefiles). Kept fields: id, highway, name_th, name_en, ref, oneway, lanes,
   maxspeed, bridge, layer. Geofabrik's oneway codes F/T/B are converted to yes/-1/no.
5. Points (CSV or point layers) keep their name fields plus a category
   (amenity / tourism / shop / fclass / type / category).
6. Shapefile text encoding: taken from the .cpg file (also understands GDAL's
   "UTF-8,2GB_LIMIT=NO"); without a .cpg, UTF-8 is tried first, then Thai cp874 (TIS-620).
7. A road/point gets its province from an attribute (adm1_pcode / adm1_name ...) or, when a
   province boundary file is available (--boundaries, or found in the input folder), from a
   spatial test. Without --clip a road that crosses a border is written to both provinces.

----------------------------------------------------------------------------------------
EXAMPLES
----------------------------------------------------------------------------------------
  python prepare_geodata.py --input raw --output prepared --zone bangkok-metro
  python prepare_geodata.py --input raw/tha_adm1.geojson --output prepared --zone each
  python prepare_geodata.py --input raw/roads_lines.shp --output prepared --zone TH-50 --clip
  python prepare_geodata.py --input raw --output prepared --zone north --classes motorway,trunk
  python prepare_geodata.py --list-zones
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
import time
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

# ----------------------------------------------------------------------------- dependencies
MISSING_HELP = {
    "shapely": "pip install shapely",
    "numpy": "pip install numpy",
    "shapefile": "pip install pyshp        (only needed for .shp files)",
    "pyproj": "pip install pyproj       (only needed for projected data, e.g. UTM 47N)",
}


def require(module: str):
    """Import a module or exit with an install hint instead of a traceback."""
    try:
        return __import__(module)
    except ImportError:
        package = {"shapefile": "pyshp"}.get(module, module)
        sys.exit(f"\n[ERROR] Missing Python package '{package}'.\n        Install it with:  {MISSING_HELP.get(module, 'pip install ' + package)}\n")


np = require("numpy")
shapely = require("shapely")
if tuple(int(p) for p in shapely.__version__.split(".")[:2]) < (2, 0):
    sys.exit(f"\n[ERROR] shapely {shapely.__version__} is too old; need 2.0+.  pip install -U shapely\n")
from shapely.geometry import LineString, MultiLineString, MultiPoint, MultiPolygon, Point, Polygon, mapping, shape  # noqa: E402
from shapely.ops import unary_union  # noqa: E402
from shapely.strtree import STRtree  # noqa: E402

HERE = Path(__file__).resolve().parent
PROVINCE_TABLE = HERE / "thai_provinces.csv"

# Thailand + margin (lon_min, lat_min, lon_max, lat_max)
THAI_BBOX = (97.0, 5.0, 106.0, 21.0)
SUPPORTED = {".geojson", ".json", ".shp", ".csv"}

CODE_FIELDS = ["shapeISO", "ISO", "iso_3166_2", "ADM1_PCODE", "adm1_pcode", "PROV_CODE", "pro_code", "P_CODE", "CODE"]
NAME_FIELDS = ["ADM1_TH", "ADM1_EN", "adm1_name", "shapeName", "PROV_NAMT", "PROV_NAME", "PROV_NAMEE",
               "name_th", "name_en", "NAME_TH", "NAME_EN", "name", "NAME"]
ROAD_CLASS_FIELDS = ["highway", "fclass"]
POINT_CATEGORY_FIELDS = ["amenity", "tourism", "shop", "fclass", "type", "category"]

ZONES: dict[str, list[str]] = {
    # Bangkok and vicinity (กรุงเทพมหานครและปริมณฑล)
    "bangkok-metro": ["TH-10", "TH-11", "TH-12", "TH-13", "TH-73", "TH-74"],
    # first test scene used in Docs/ThailandMap.md
    "test-area": ["TH-10", "TH-12", "TH-13"],
}
REGION_ZONES = {"north": "North", "northeast": "Northeast", "central": "Central",
                "east": "East", "west": "West", "south": "South"}

# Spelling variants seen in Thai datasets (normalised key -> ISO code)
NAME_ALIASES = {
    "กรุงเทพ": "TH-10", "กรุงเทพฯ": "TH-10", "กทม": "TH-10", "bangkokmetropolis": "TH-10",
    "krungthepmahanakhon": "TH-10", "krungthep": "TH-10", "bangkokcity": "TH-10",
    "อยุธยา": "TH-14", "ayutthaya": "TH-14", "phranakhonsiayudhya": "TH-14",
    "sakaew": "TH-27", "sakeao": "TH-27", "bungkan": "TH-38", "buengkarn": "TH-38",
    "phetburi": "TH-76", "phetchaburee": "TH-76", "rajaburi": "TH-70", "ratburi": "TH-70",
    "phattalung": "TH-93", "patthalung": "TH-93", "chiengmai": "TH-50", "chiangrai": "TH-57",
    "korat": "TH-30", "khorat": "TH-30", "nakhonratchasrima": "TH-30", "ubonratchatani": "TH-34",
    "sisaket": "TH-33", "srisaket": "TH-33", "nongbualamphu": "TH-39", "nongbualumphu": "TH-39",
    "kanchanaburee": "TH-71", "prachuapkhirikhan": "TH-77", "prachuabkirikhan": "TH-77",
    "nakhonsithammarat": "TH-80", "nakornsithammarat": "TH-80", "suratthani": "TH-84",
    "phangnga": "TH-82", "phuket": "TH-83", "samutprakarn": "TH-11", "pathumthanee": "TH-13",
}


# ----------------------------------------------------------------------------- small helpers
def is_null(value) -> bool:
    if value is None:
        return True
    if isinstance(value, float) and (math.isnan(value) or math.isinf(value)):
        return True
    if isinstance(value, bytes):
        value = value.decode("utf-8", "ignore")
    return isinstance(value, str) and value.strip().lower() in ("", "null", "none", "nan", "n/a", "-", "<null>")


def clean(value):
    """Null-safe value cleaning: strips strings, turns null-likes into None."""
    if is_null(value):
        return None
    if isinstance(value, bytes):
        value = value.decode("utf-8", "replace")
    if isinstance(value, str):
        value = value.strip()
    return value


def human_size(num_bytes: float) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if num_bytes < 1024 or unit == "GB":
            return f"{num_bytes:.1f} {unit}" if unit != "B" else f"{int(num_bytes)} B"
        num_bytes /= 1024
    return f"{num_bytes:.1f} GB"


def input_size(path: Path) -> int:
    """Size of a file, including the shapefile sidecars (.dbf/.shx/.prj/.cpg)."""
    if path.suffix.lower() == ".shp":
        return sum(p.stat().st_size for p in path.parent.glob(path.stem + ".*") if p.is_file())
    return path.stat().st_size


def first_field(props: dict, names: list[str]):
    """First non-null value among candidate field names (case-insensitive)."""
    lower = {k.lower(): k for k in props}
    for name in names:
        key = name if name in props else lower.get(name.lower())
        if key is not None:
            value = clean(props.get(key))
            if value is not None:
                return value
    return None


# ----------------------------------------------------------------------------- province lookup
@dataclass
class ProvinceInfo:
    code: str
    name_th: str
    name_en: str
    region: str


class ProvinceIndex:
    def __init__(self, table: Path):
        if not table.is_file():
            sys.exit(f"\n[ERROR] Province table not found: {table}\n        It ships with the repo in Tools/GeoPipeline/.\n")
        self.by_code: dict[str, ProvinceInfo] = {}
        self.by_name: dict[str, str] = dict(NAME_ALIASES)
        with open(table, encoding="utf-8-sig") as f:
            for row in csv.DictReader(f):
                info = ProvinceInfo(row["code"], row["name_th"], row["name_en"], row["region"])
                self.by_code[info.code] = info
                self.by_name[self.norm(info.name_th)] = info.code
                self.by_name[self.norm(info.name_en)] = info.code

    @staticmethod
    def norm(text: str) -> str:
        t = str(text).strip().lower()
        t = re.sub(r"^(จังหวัด|จ\.|changwat)\s*", "", t)
        t = re.sub(r"\s*(province|provinces|metropolis)$", "", t) if not t.startswith("bangkok") else t
        return re.sub(r"[\s\-\.'’_,()]", "", t)

    def code_from_value(self, value) -> str | None:
        value = clean(value)
        if value is None:
            return None
        if isinstance(value, (int, float)):
            if isinstance(value, float) and not value.is_integer():
                return None
            value = str(int(value))
        text = str(value).strip().upper().replace(" ", "")
        m = re.fullmatch(r"(?:TH-?)?(\d{2})(?:\.0)?", text)
        if m:
            code = f"TH-{m.group(1)}"
            return code if code in self.by_code else None
        return None

    def code_from_name(self, value) -> str | None:
        value = clean(value)
        return self.by_name.get(self.norm(value)) if isinstance(value, str) else None

    def resolve(self, props: dict) -> str | None:
        for name in CODE_FIELDS:
            code = self.code_from_value(first_field(props, [name]))
            if code:
                return code
        for name in NAME_FIELDS:
            code = self.code_from_name(first_field(props, [name]))
            if code:
                return code
        return None

    def zone_codes(self, zone: str) -> list[str] | None:
        """Codes for a zone name. None = 'all' (single file), [] = 'each' (one file per province)."""
        key = zone.strip().lower()
        if key == "all":
            return None
        if key == "each":
            return []
        if key in ZONES:
            return ZONES[key]
        if key in REGION_ZONES:
            return [c for c, p in self.by_code.items() if p.region == REGION_ZONES[key]]
        codes = []
        for part in zone.split(","):
            code = self.code_from_value(part) or self.code_from_name(part)
            if not code:
                sys.exit(f"\n[ERROR] Unknown zone or province '{part.strip()}'. Run with --list-zones.\n")
            codes.append(code)
        return codes


# ----------------------------------------------------------------------------- reading
@dataclass
class RawFeature:
    props: dict
    geom: object  # shapely geometry or None


@dataclass
class FileReport:
    path: Path
    kind: str = "?"
    size_in: int = 0
    read: int = 0
    kept: int = 0
    dropped: Counter = field(default_factory=Counter)
    notes: list[str] = field(default_factory=list)


def parse_cpg(path: Path) -> str | None:
    """Encoding from a .cpg sidecar. Handles GDAL's 'UTF-8,2GB_LIMIT=NO' and code-page numbers."""
    cpg = path.with_suffix(".cpg")
    if not cpg.is_file():
        return None
    raw = cpg.read_text(encoding="ascii", errors="ignore").strip().split(",")[0].strip().upper()
    mapping_ = {"UTF-8": "utf-8", "UTF8": "utf-8", "65001": "utf-8", "874": "cp874", "CP874": "cp874",
                "TIS-620": "cp874", "TIS620": "cp874", "WINDOWS-874": "cp874", "1252": "cp1252",
                "LATIN1": "latin-1", "ISO-8859-1": "latin-1", "88591": "latin-1"}
    return mapping_.get(raw, raw.lower() or None)


def make_reprojector(crs_text: str | None, report: FileReport):
    """Returns f(x, y) -> (lon, lat) for projected data, or exits the file with a clear reason."""
    if not crs_text:
        report.notes.append("coordinates look projected (metres) but no .prj / crs was found")
        return None
    try:
        from pyproj import CRS, Transformer
    except ImportError:
        report.notes.append("coordinates are projected; install pyproj to reproject:  pip install pyproj")
        return None
    try:
        src = CRS.from_user_input(crs_text)
        transformer = Transformer.from_crs(src, "EPSG:4326", always_xy=True)
        report.notes.append(f"reprojected from {src.name}")
        return transformer.transform
    except Exception as exc:  # pyproj raises many types
        report.notes.append(f"cannot understand CRS ({exc.__class__.__name__}): {str(crs_text)[:60]}")
        return None


def safe_shape(geojson_geom):
    if not isinstance(geojson_geom, dict) or not geojson_geom.get("type"):
        return None
    try:
        g = shape(geojson_geom)
        return None if g.is_empty else g
    except Exception:
        return None


def read_geojson(path: Path, report: FileReport):
    try:
        with open(path, encoding="utf-8-sig") as f:
            data = json.load(f)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        report.notes.append(f"not valid JSON: {exc}")
        return [], None
    if isinstance(data, dict) and data.get("type") == "Feature":
        data = {"type": "FeatureCollection", "features": [data]}
    if not isinstance(data, dict) or not isinstance(data.get("features"), list):
        report.notes.append("not a GeoJSON FeatureCollection (no 'features' list)")
        return [], None
    crs = None
    crs_name = (((data.get("crs") or {}).get("properties") or {}).get("name"))
    if crs_name and not re.search(r"CRS84|4326", str(crs_name)):
        crs = crs_name
    feats = []
    for f in data["features"]:
        if not isinstance(f, dict):
            report.dropped["not a feature"] += 1
            continue
        props = f.get("properties") if isinstance(f.get("properties"), dict) else {}
        feats.append(RawFeature(props, safe_shape(f.get("geometry"))))
    return feats, crs


def read_csv(path: Path, report: FileReport):
    feats = []
    lat_names = ("lat", "latitude", "y", "lat_dd", "ละติจูด")
    lon_names = ("lon", "lng", "long", "longitude", "x", "lon_dd", "ลองจิจูด")
    wkt_names = ("wkt", "geometry", "geom", "the_geom")
    for encoding in ("utf-8-sig", "cp874"):
        try:
            with open(path, encoding=encoding, newline="") as f:
                rows = list(csv.DictReader(f))
            if encoding != "utf-8-sig":
                report.notes.append(f"read as {encoding}")
            break
        except UnicodeDecodeError:
            continue
    else:
        report.notes.append("cannot decode CSV as UTF-8 or cp874")
        return [], None
    if not rows:
        return [], None
    header = {h.lower().strip(): h for h in rows[0].keys() if h}
    lat_col = next((header[n] for n in lat_names if n in header), None)
    lon_col = next((header[n] for n in lon_names if n in header), None)
    wkt_col = next((header[n] for n in wkt_names if n in header), None)
    if not ((lat_col and lon_col) or wkt_col):
        report.notes.append(f"no lat/lon or WKT column (columns: {', '.join(list(header)[:12])})")
        return [], None
    from shapely import wkt as shapely_wkt
    for row in rows:
        geom = None
        if lat_col and lon_col:
            try:
                lat = float(str(row.get(lat_col)).replace(",", ".").strip())
                lon = float(str(row.get(lon_col)).replace(",", ".").strip())
                geom = Point(lon, lat)
            except (TypeError, ValueError):
                geom = None
        elif wkt_col:
            try:
                geom = shapely_wkt.loads(str(row.get(wkt_col)))
            except Exception:
                geom = None
        props = {k: v for k, v in row.items() if k and k not in (lat_col, lon_col, wkt_col)}
        feats.append(RawFeature(props, geom))
    return feats, None


def read_shapefile(path: Path, report: FileReport, road_classes: set[str]):
    shapefile_mod = require("shapefile")
    for side in (".shx", ".dbf"):
        if not path.with_suffix(side).is_file():
            report.notes.append(f"missing sidecar file {path.with_suffix(side).name} (a shapefile needs .shp + .shx + .dbf)")
            return [], None

    encoding = parse_cpg(path)
    candidates = [encoding] if encoding else ["utf-8", "cp874"]
    reader = None
    for enc in candidates:
        try:
            import warnings
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")  # pyshp warns when we override a malformed .cpg
                reader = shapefile_mod.Reader(str(path), encoding=enc, encodingErrors="strict")
                # probe a few records so a wrong guess fails here, not halfway
                for i in range(min(len(reader), 200)):
                    reader.record(i)
            if enc != "utf-8":
                report.notes.append(f"text encoding {enc}")
            break
        except Exception:  # UnicodeDecodeError, LookupError, or pyshp 3.x dbfFileException
            reader = None
    if reader is None:
        reader = shapefile_mod.Reader(str(path), encoding="utf-8", encodingErrors="replace")
        report.notes.append("unknown text encoding, undecodable characters replaced")

    field_names = [f[0] for f in reader.fields[1:]]
    prj = path.with_suffix(".prj")
    crs = prj.read_text(encoding="ascii", errors="ignore") if prj.is_file() else None

    # Big road layers (millions of lines): filter on the attribute table first,
    # then read geometry only for the rows we keep.
    class_field = next((f for f in field_names if f.lower() in ROAD_CLASS_FIELDS), None)
    is_line_layer = reader.shapeType in (shapefile_mod.POLYLINE, shapefile_mod.POLYLINEZ, shapefile_mod.POLYLINEM)
    feats = []
    if is_line_layer and class_field and road_classes:
        report.notes.append(f"pre-filtered {len(reader):,} rows on '{class_field}'")
        for i, rec in enumerate(reader.iterRecords(fields=[class_field])):
            if clean(rec[0]) not in road_classes:
                continue
            feats.append(RawFeature(reader.record(i).as_dict(), _shp_geom(reader, i)))
        report.read = len(reader)
        report.dropped["road class not wanted"] += len(reader) - len(feats)
        return feats, crs

    for i in range(len(reader)):
        try:
            props = reader.record(i).as_dict()
        except Exception:
            report.dropped["unreadable record"] += 1
            continue
        feats.append(RawFeature(props, _shp_geom(reader, i)))
    return feats, crs


def _shp_geom(reader, index: int):
    try:
        return safe_shape(reader.shape(index).__geo_interface__)
    except Exception:
        return None


# ----------------------------------------------------------------------------- geometry checks
def coords_of(geom) -> "np.ndarray":
    return shapely.get_coordinates(geom)


def looks_projected(feats: list[RawFeature]) -> bool:
    sample = [f.geom for f in feats[:500] if f.geom is not None]
    if not sample:
        return False
    xy = np.vstack([coords_of(g) for g in sample])
    xy = xy[np.isfinite(xy).all(axis=1)]
    return len(xy) > 0 and float(np.abs(xy).max()) > 360.0


def looks_swapped(feats: list[RawFeature]) -> bool:
    """True if most coordinates are (lat, lon) instead of (lon, lat)."""
    sample = [f.geom for f in feats[:500] if f.geom is not None]
    if not sample:
        return False
    xy = np.vstack([coords_of(g) for g in sample])
    xy = xy[np.isfinite(xy).all(axis=1)]
    if len(xy) == 0:
        return False
    lon0, lat0, lon1, lat1 = THAI_BBOX
    normal = ((xy[:, 0] >= lon0) & (xy[:, 0] <= lon1) & (xy[:, 1] >= lat0) & (xy[:, 1] <= lat1)).mean()
    swapped = ((xy[:, 1] >= lon0) & (xy[:, 1] <= lon1) & (xy[:, 0] >= lat0) & (xy[:, 0] <= lat1)).mean()
    return swapped > 0.5 and swapped > normal


def apply_xy(geom, func):
    """Apply func(x_array, y_array) -> (x, y) to every coordinate; None if it fails."""
    try:
        return shapely.transform(geom, lambda c: np.column_stack(func(c[:, 0], c[:, 1])))
    except Exception:
        return None


def polygonal_part(geom):
    if geom is None or geom.is_empty:
        return None
    if isinstance(geom, (Polygon, MultiPolygon)):
        return geom
    parts = [g for g in getattr(geom, "geoms", []) if isinstance(g, (Polygon, MultiPolygon))]
    return unary_union(parts) if parts else None


def lineal_part(geom):
    if geom is None or geom.is_empty:
        return None
    if isinstance(geom, (LineString, MultiLineString)):
        return geom
    parts = [g for g in getattr(geom, "geoms", []) if isinstance(g, (LineString, MultiLineString))]
    return shapely.line_merge(MultiLineString([p for g in parts for p in getattr(g, "geoms", [g])])) if parts else None


def check_geometry(geom, kind: str, report: FileReport):
    """Returns a clean geometry of the right type, or None (and counts why)."""
    if geom is None:
        report.dropped["null / unreadable geometry"] += 1
        return None
    xy = coords_of(geom)
    if len(xy) == 0:
        report.dropped["empty geometry"] += 1
        return None
    if not np.isfinite(xy).all():
        report.dropped["NaN/infinite coordinates"] += 1
        return None
    lon0, lat0, lon1, lat1 = THAI_BBOX
    min_x, min_y, max_x, max_y = geom.bounds
    if max_x < lon0 or min_x > lon1 or max_y < lat0 or min_y > lat1:
        report.dropped["outside Thailand"] += 1
        return None
    if abs(min_x) > 180 or abs(max_x) > 180 or abs(min_y) > 90 or abs(max_y) > 90:
        report.dropped["coordinates out of lon/lat range"] += 1
        return None

    if kind == "provinces":
        if not geom.is_valid:
            geom = shapely.make_valid(geom)
            report.dropped["(repaired invalid polygon)"] += 1
        geom = polygonal_part(geom)
    elif kind == "roads":
        geom = lineal_part(geom)
        if geom is not None and geom.length <= 0:
            geom = None
    else:
        if not isinstance(geom, (Point, MultiPoint)):
            geom = geom.representative_point() if not geom.is_empty else None

    if geom is None or geom.is_empty:
        report.dropped[f"wrong geometry type for {kind}"] += 1
        return None
    return geom


def detect_kind(feats: list[RawFeature], forced: str) -> str:
    if forced != "auto":
        return forced
    types = Counter(f.geom.geom_type for f in feats[:2000] if f.geom is not None)
    if not types:
        return "points"
    top = types.most_common(1)[0][0]
    if "Polygon" in top:
        return "provinces"
    if "LineString" in top:
        return "roads"
    return "points"


# ----------------------------------------------------------------------------- field filtering
def oneway_value(props: dict, geofabrik: bool) -> str | None:
    raw = first_field(props, ["oneway"])
    if raw is None:
        return None
    v = str(raw).strip().lower()
    if geofabrik and v in ("f", "t", "b"):
        return {"f": "yes", "t": "-1", "b": "no"}[v]
    if v in ("yes", "true", "1"):
        return "yes"
    if v in ("-1", "reverse"):
        return "-1"
    if v in ("no", "false", "0"):
        return "no"
    return None


def int_or_none(value) -> int | None:
    value = clean(value)
    if value is None:
        return None
    try:
        return int(float(str(value).split(";")[0].replace(",", ".")))
    except ValueError:
        return None


def slim_props(kind: str, props: dict, provinces: ProvinceIndex, province_code: str | None, geofabrik: bool) -> dict:
    if kind == "provinces":
        info = provinces.by_code[province_code]
        return {"code": info.code, "name_th": info.name_th, "name_en": info.name_en, "region": info.region}
    out = {
        "id": first_field(props, ["id", "osm_id", "@id", "fid"]),
        "name_th": first_field(props, ["name_th", "name:th", "NAME_TH"]) or first_field(props, ["name", "NAME"]),
        "name_en": first_field(props, ["name_en", "name:en", "NAME_EN", "name_latin"]),
    }
    if kind == "roads":
        out.update({
            "highway": first_field(props, ROAD_CLASS_FIELDS),
            "ref": first_field(props, ["ref", "REF"]),
            "oneway": oneway_value(props, geofabrik),
            "lanes": int_or_none(first_field(props, ["lanes"])),
            "maxspeed": int_or_none(first_field(props, ["maxspeed"])),
            "bridge": None if clean(first_field(props, ["bridge"])) in (None, "no", "F", "f") else "yes",
            "layer": int_or_none(first_field(props, ["layer"])),
        })
    else:
        out["category"] = first_field(props, POINT_CATEGORY_FIELDS)
    if province_code:
        out["province"] = province_code
    return {k: (str(v) if k == "id" and v is not None else v) for k, v in out.items() if v is not None}


# ----------------------------------------------------------------------------- output
def rounded(geom, precision: int):
    return shapely.transform(geom, lambda c: np.round(c, precision))


def write_geojson(path: Path, features: list[dict]) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": features}, f, ensure_ascii=False, separators=(",", ":"))
    return path.stat().st_size


def to_feature(geom, props: dict, precision: int) -> dict | None:
    g = rounded(geom, precision)
    if g.is_empty:
        return None
    return {"type": "Feature", "properties": props, "geometry": mapping(g)}


# ----------------------------------------------------------------------------- main
@dataclass
class CleanFeature:
    geom: object
    props: dict
    provinces: set[str]


def process_file(path: Path, args, provinces: ProvinceIndex, road_classes: set[str]):
    report = FileReport(path, size_in=input_size(path))
    suffix = path.suffix.lower()
    if suffix in (".geojson", ".json"):
        raw, crs = read_geojson(path, report)
    elif suffix == ".csv":
        raw, crs = read_csv(path, report)
    else:
        raw, crs = read_shapefile(path, report, road_classes if args.dataset in ("auto", "roads") else set())
    report.read = report.read or len(raw)
    if not raw:
        report.kind = "skipped"
        return report, "skipped", []

    if looks_projected(raw):
        func = make_reprojector(crs, report)
        if func is None:
            report.kind = "skipped"
            report.dropped["projected data, not reprojected"] += len(raw)
            return report, "skipped", []
        for f in raw:
            if f.geom is not None:
                f.geom = apply_xy(f.geom, func)
    elif looks_swapped(raw):
        report.notes.append("lat/lon were swapped - corrected")
        for f in raw:
            if f.geom is not None:
                f.geom = apply_xy(f.geom, lambda x, y: (y, x))

    kind = detect_kind(raw, args.dataset)
    report.kind = kind
    geofabrik = any("fclass" in f.props for f in raw[:5])
    simplify_deg = args.simplify_m / 111_320.0 if args.simplify_m > 0 else 0.0

    cleaned: list[CleanFeature] = []
    for f in raw:
        if kind == "roads":
            cls = clean(first_field(f.props, ROAD_CLASS_FIELDS))
            if road_classes and cls not in road_classes:
                report.dropped["road class not wanted"] += 1
                continue
        geom = check_geometry(f.geom, kind, report)
        if geom is None:
            continue
        code = provinces.resolve(f.props)
        if kind == "provinces" and code is None:
            report.dropped["province not recognised"] += 1
            continue
        if simplify_deg:
            geom = geom.simplify(simplify_deg, preserve_topology=True)
        cleaned.append(CleanFeature(geom, slim_props(kind, f.props, provinces, code, geofabrik), {code} if code else set()))

    if kind == "provinces":
        # merge duplicates (some datasets split a province into several rows)
        by_code: dict[str, list[CleanFeature]] = defaultdict(list)
        for c in cleaned:
            by_code[next(iter(c.provinces))].append(c)
        merged = []
        for code, items in by_code.items():
            geom = items[0].geom if len(items) == 1 else unary_union([i.geom for i in items])
            if len(items) > 1:
                report.notes.append(f"{code}: merged {len(items)} rows")
            merged.append(CleanFeature(geom, items[0].props, {code}))
        cleaned = merged
        missing = sorted(set(provinces.by_code) - set(by_code))
        if missing and len(missing) < 77:
            report.notes.append(f"{len(missing)} province(s) not in this file: {', '.join(missing[:10])}{' ...' if len(missing) > 10 else ''}")

    report.kept = len(cleaned)
    return report, kind, cleaned


def assign_spatial(features: list[CleanFeature], boundaries: dict[str, object]) -> int:
    """Fill in provinces for features without a province attribute. Returns how many were assigned."""
    codes = list(boundaries)
    tree = STRtree([boundaries[c] for c in codes])
    assigned = 0
    for f in features:
        if f.provinces:
            continue
        hits = {codes[i] for i in tree.query(f.geom, predicate="intersects")}
        if hits:
            f.provinces = hits
            if len(hits) == 1:
                f.props["province"] = next(iter(hits))
            assigned += 1
    return assigned


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", type=Path, help="folder with source files, or one file")
    parser.add_argument("--output", type=Path, default=Path("prepared"), help="output folder (default: prepared)")
    parser.add_argument("--zone", default="test-area",
                        help="all | each | bangkok-metro | test-area | north/northeast/central/east/west/south | "
                             "province code/name list, e.g. 'TH-50' or 'เชียงใหม่,Lamphun'")
    parser.add_argument("--dataset", choices=["auto", "provinces", "roads", "points"], default="auto")
    parser.add_argument("--classes", default="motorway,trunk,primary",
                        help="road classes to keep ('_link' added automatically); 'all' keeps every class")
    parser.add_argument("--boundaries", type=Path, help="province polygons for spatial assignment/clipping "
                        "(default: a province file found in --input)")
    parser.add_argument("--clip", action="store_true", help="cut roads/points exactly at the zone border")
    parser.add_argument("--precision", type=int, default=6, help="decimal places to keep (6 = ~0.1 m)")
    parser.add_argument("--simplify-m", type=float, default=0.0, help="simplify tolerance in real metres (0 = off)")
    parser.add_argument("--list-zones", action="store_true")
    args = parser.parse_args()

    provinces = ProvinceIndex(PROVINCE_TABLE)

    if args.list_zones:
        print("Zones:")
        print("  all            everything in one file per dataset")
        print("  each           one file per province (77 files)")
        for name, codes in ZONES.items():
            print(f"  {name:<14} {', '.join(provinces.by_code[c].name_th for c in codes)}")
        for name, region in REGION_ZONES.items():
            n = sum(1 for p in provinces.by_code.values() if p.region == region)
            print(f"  {name:<14} {region} region ({n} provinces)")
        print("  <code/name>    e.g. TH-50 | เชียงใหม่ | 'Chiang Mai,Lamphun'")
        return

    if args.input is None:
        parser.error("--input is required")
    if not args.input.exists():
        sys.exit(f"\n[ERROR] Input not found: {args.input.resolve()}\n")
    if args.input.is_dir():
        files = sorted(p for p in args.input.rglob("*") if p.suffix.lower() in SUPPORTED and p.is_file())
        files = [p for p in files if args.output.resolve() not in p.resolve().parents]  # never re-read our output
    else:
        files = [args.input]
    if not files:
        sys.exit(f"\n[ERROR] No .geojson / .json / .shp / .csv files in {args.input.resolve()}\n")

    zone_codes = provinces.zone_codes(args.zone)
    road_classes: set[str] = set()
    if args.classes.strip().lower() != "all":
        road_classes = {c.strip() for c in args.classes.split(",") if c.strip()}
        road_classes |= {f"{c}_link" for c in road_classes}

    started = time.time()
    reports: list[FileReport] = []
    datasets: dict[str, list[CleanFeature]] = defaultdict(list)
    for path in files:
        print(f"reading {path.name} ({human_size(input_size(path))}) ...", flush=True)
        try:
            report, kind, feats = process_file(path, args, provinces, road_classes)
        except MemoryError:
            sys.exit(f"\n[ERROR] Out of memory reading {path.name}. Split it first or use a smaller --zone.\n")
        except Exception as exc:  # one bad file must not stop the whole run
            report, kind, feats = FileReport(path, kind="skipped", size_in=input_size(path)), "skipped", []
            report.notes.append(f"failed: {exc.__class__.__name__}: {str(exc)[:120]}")
        reports.append(report)
        if kind != "skipped":
            datasets[kind].extend(feats)

    # Province polygons for spatial assignment / clipping.
    boundaries: dict[str, object] = {}
    if args.boundaries:
        if not args.boundaries.is_file():
            sys.exit(f"\n[ERROR] --boundaries file not found: {args.boundaries.resolve()}\n")
        b_report, b_kind, b_feats = process_file(args.boundaries, argparse.Namespace(**{**vars(args), "dataset": "provinces"}), provinces, set())
        boundaries = {next(iter(f.provinces)): f.geom for f in b_feats}
    elif datasets.get("provinces"):
        boundaries = {next(iter(f.provinces)): f.geom for f in datasets["provinces"]}

    for kind in ("roads", "points"):
        if datasets.get(kind) and boundaries:
            n = assign_spatial(datasets[kind], boundaries)
            if n:
                print(f"  {kind}: {n:,} features got their province from the boundary polygons")
        unassigned = sum(1 for f in datasets.get(kind, []) if not f.provinces)
        if unassigned and zone_codes is not None:
            why = ("lie outside every province polygon (sea / neighbouring country)" if boundaries
                   else "have no province attribute and no boundary file was given (use --boundaries)")
            print(f"  [note] {kind}: {unassigned:,} features {why}; they are left out of zone files")

    # Write outputs.
    outputs: list[tuple[Path, int, int]] = []
    for kind, feats in datasets.items():
        if zone_codes is None:
            groups = {"all": (None, feats)}
        elif zone_codes == []:
            groups = {}
            for code in provinces.by_code:
                sel = [f for f in feats if code in f.provinces]
                if sel:
                    info = provinces.by_code[code]
                    groups[f"{code}_{info.name_en.replace(' ', '')}"] = ([code], sel)
        else:
            label = args.zone.lower() if args.zone.lower() in ZONES or args.zone.lower() in REGION_ZONES else "_".join(zone_codes)
            groups = {label: (zone_codes, [f for f in feats if f.provinces & set(zone_codes)])}

        for label, (codes, sel) in groups.items():
            clip_geom = None
            if args.clip and codes and kind != "provinces":
                if all(c in boundaries for c in codes):
                    clip_geom = unary_union([boundaries[c] for c in codes])
                else:
                    print(f"  [warn] --clip ignored for {label}: no boundary polygons loaded")
            out_feats = []
            for f in sel:
                geom = f.geom
                if clip_geom is not None:
                    geom = geom.intersection(clip_geom)
                    geom = lineal_part(geom) if kind == "roads" else geom
                    if geom is None or geom.is_empty:
                        continue
                feat = to_feature(geom, f.props, args.precision)
                if feat:
                    out_feats.append(feat)
            if not out_feats:
                continue
            path = args.output / kind / f"{label}.geojson"
            size = write_geojson(path, out_feats)
            outputs.append((path, len(out_feats), size))

    # ------------------------------------------------------------------ summary
    print("\n" + "=" * 78)
    print("INPUT FILES")
    print(f"{'file':<34}{'type':<11}{'size':>10}{'features':>11}{'kept':>10}")
    for r in reports:
        print(f"{r.path.name[:33]:<34}{r.kind:<11}{human_size(r.size_in):>10}{r.read:>11,}{r.kept:>10,}")
        for reason, n in r.dropped.most_common():
            print(f"{'':<36}- {reason}: {n:,}")
        for note in r.notes:
            print(f"{'':<36}! {note}")

    print("\nOUTPUT FILES")
    print(f"{'file':<52}{'features':>10}{'size':>12}")
    for path, n, size in outputs:
        print(f"{str(path.relative_to(args.output))[:51]:<52}{n:>10,}{human_size(size):>12}")

    total_in = sum(r.size_in for r in reports)
    total_out = sum(s for _, _, s in outputs)
    total_read = sum(r.read for r in reports)
    total_written = sum(n for _, n, _ in outputs)
    print("-" * 78)
    print(f"before: {total_read:,} features, {human_size(total_in)}")
    print(f"after : {total_written:,} features, {human_size(total_out)}"
          + (f"  ({100 * (1 - total_out / total_in):.1f}% smaller)" if total_in else ""))
    print(f"zone  : {args.zone}  |  output: {args.output.resolve()}  |  {time.time() - started:.1f} s")

    if outputs:
        args.output.mkdir(parents=True, exist_ok=True)
        with open(args.output / "_summary.csv", "w", encoding="utf-8-sig", newline="") as f:
            w = csv.writer(f)
            w.writerow(["file", "features", "bytes"])
            for path, n, size in outputs:
                w.writerow([str(path.relative_to(args.output)), n, size])
    else:
        print("\n[warn] nothing was written - check the zone, --classes and the notes above")
        sys.exit(1)


if __name__ == "__main__":
    main()
