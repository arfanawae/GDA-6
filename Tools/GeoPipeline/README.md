# Geo Pipeline — real Thailand → game coordinates

Converts OpenStreetMap-derived data into Unreal-ready JSON for the 1:20 Thailand map.
Background and design decisions: [`Docs/ThailandMap.md`](../../Docs/ThailandMap.md).

## Setup
```bash
cd Tools/GeoPipeline
python -m venv .venv && . .venv/bin/activate      # Windows: .venv\Scripts\activate
pip install -r requirements.txt
mkdir -p raw
```

## Download source data (one time, kept out of git)
```bash
# 77 province boundaries (geoBoundaries THA ADM1, ODbL, ~4 MB)
curl -L -o raw/tha_adm1.geojson https://media.githubusercontent.com/media/wmgeolab/geoBoundaries/9469f09/releaseData/gbOpen/THA/ADM1/geoBoundaries-THA-ADM1.geojson

# All Thai roads (HOT export via HDX, GeoParquet, ODbL, ~520 MB)
curl -L -o raw/roads.parquet https://production-raw-data-api.s3.amazonaws.com/ISO3/THA/_layers/osm/roads.parquet
```

## Build
```bash
# Provinces: DataTable JSON + summary + bounds (+ optional minimap PNG)
python build_provinces.py --boundaries raw/tha_adm1.geojson --area TH-10,TH-12,TH-13 --minimap raw/T_Minimap_TestArea.png

# Roads + border-crossing signs for the test area
python build_roads.py --input raw/roads.parquet --area TH-10,TH-12,TH-13 --name TestArea

# Visual check
python preview_area.py --name TestArea --out raw/preview_TestArea.png

# Projection self-test (Bangkok round trip, 1° latitude ≈ 5.56 km in game)
python geo_projection.py
```

## Files
| File | Purpose |
|---|---|
| `geo_config.json` | projection origin + scales. **Keep in sync with `[/Script/Downtown.DTGeoSettings]` in `Config/DefaultGame.ini`.** |
| `geo_projection.py` | lat/lon ↔ UE cm (spherical Transverse Mercator), mirrored by `UDTGeoLibrary` in C++ |
| `thai_provinces.csv` | 77 provinces: ISO 3166-2 code, Thai/English name, region (6-region system) |
| `build_provinces.py` | → `Content/_Downtown/Data/Provinces/DT_Provinces.json`, `provinces_summary.csv`, `map_bounds.json` |
| `build_roads.py` | → `Content/_Downtown/Data/Roads/Roads_<name>.json` (roads + crossings). Also accepts GeoJSON or Overpass JSON |
| `preview_area.py` | renders provinces + roads + crossings to a PNG |

## Changing the scale
1. Edit `geo_config.json` **and** `Config/DefaultGame.ini`.
2. Re-run both build scripts, re-import `DT_Provinces.json` (Reimport on the DataTable) and press **Import** on the `DTGeoImporter` again.
The importer refuses files built with a different scale/origin.

## Other road sources
- Geofabrik: `osmium tags-filter thailand-latest.osm.pbf w/highway=motorway,trunk,primary,motorway_link,trunk_link,primary_link -o main.osm.pbf && osmium export main.osm.pbf -o roads.geojson`
- Overpass: `way["highway"~"^(motorway|trunk|primary)(_link)?$"](S,W,N,E); out geom tags;` saved as JSON

Data is © OpenStreetMap contributors, ODbL — see `Content/_Downtown/Data/ATTRIBUTION.md`.
