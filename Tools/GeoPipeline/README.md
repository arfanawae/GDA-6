# Geo Pipeline — real Thailand → game coordinates

Converts OpenStreetMap-derived data into Unreal-ready JSON for the 1:20 Thailand map.
Background and design decisions: [`Docs/ThailandMap.md`](../../Docs/ThailandMap.md).

## Setup
```bash
cd Tools/GeoPipeline
python -m venv .venv && . .venv/bin/activate      # Windows: .venv\Scripts\activate
pip install -r requirements.txt      # Windows: see 'Windows (PowerShell)' below
mkdir -p raw
```

## Step 0 — clean & split raw data: `prepare_geodata.py`

Reads every `.geojson/.json/.shp/.csv` in a folder, keeps only the fields the game needs, repairs/drops
bad geometry (nulls, NaN, swapped lat/lon, out-of-Thailand, invalid polygons), reprojects UTM data,
and writes one GeoJSON per zone with a before/after size report. Its output feeds the build scripts below.
Data assumptions are listed at the top of the script.

### Windows (PowerShell) — quick start
```powershell
# 1) once: install Python 3.11+ from python.org (tick "Add python.exe to PATH"), then in the repo folder:
cd Tools\GeoPipeline
py -m venv .venv
.\.venv\Scripts\Activate.ps1          # if blocked: Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
pip install shapely numpy pyshp pyproj

# 2) put source files in Tools\GeoPipeline\raw\  (.geojson / .shp+.shx+.dbf(+.prj/.cpg) / .csv)

# 3) run
python prepare_geodata.py --list-zones
python prepare_geodata.py --input raw --output prepared --zone bangkok-metro
python prepare_geodata.py --input raw --output prepared --zone TH-50 --clip
python prepare_geodata.py --input raw\tha_adm1.geojson --output prepared --zone each
```
(cmd.exe: use `.venv\Scripts\activate.bat` instead of `Activate.ps1`.)

| Option | Meaning |
|---|---|
| `--zone` | `all`, `each` (77 files), `bangkok-metro`, `test-area`, `north` … `south`, or codes/names: `TH-50`, `เชียงใหม่,Lamphun` |
| `--classes` | road classes to keep (default `motorway,trunk,primary`, links added); `all` keeps everything |
| `--boundaries` | province polygons for assigning/clipping roads & points (default: a province file found in `--input`) |
| `--clip` | cut roads/points exactly at the zone border |
| `--simplify-m` | simplify tolerance in real metres (0 = off) |
| `--precision` | decimals kept in coordinates (6 ≈ 0.1 m) |

Output: `prepared/<provinces|roads|points>/<zone>.geojson` + `prepared/_summary.csv`.
Tested on the full HOT/HDX roads shapefile (2.94 M lines, 6.9 GB → 13,840 lines / 5 MB for `bangkok-metro`, 15 s, ~0.4 GB RAM).

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

# Terrain: Copernicus DEM -> 16-bit Landscape heightmap (+ road gradient report)
python build_heightmap.py --area TH-50,TH-51,TH-52 --name North3 --download --roads ../../Content/_Downtown/Data/Roads/Roads_North3.json

# Road beds: grade-limited, smoothed terrain along every road (prints gradients before/after)
python build_heightmap.py --area TH-50,TH-51,TH-52 --name North3 --roads ../../Content/_Downtown/Data/Roads/Roads_North3.json --flatten-roads

# Sea + coastline: seabed below Z 0, Content/_Downtown/Data/Coast/Coast_<name>.json for DTGeoImporter.CoastFile
python build_heightmap.py --area TH-82,TH-83 --name Phuket --download --sea

# National highways per region (--area accepts north/northeast/central/east/west/south, presets, or all)
python build_roads.py --input raw/roads.parquet --area south --name Highways_South --classes motorway,trunk

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
| `terrain_ops.py` | land/sea mask, seabed, coastline extraction, road beds, gradient stats (used by build_heightmap.py) |
| `build_heightmap.py` | Copernicus GLO-30 DEM → `.r16`/16-bit PNG heightmap + exact Landscape import values (sea level = Z 0) + road gradient table per vertical scale |
| `prepare_geodata.py` | step 0: clean/filter/split raw GeoJSON / Shapefile / CSV per province or zone |

## Changing the scale
1. Edit `geo_config.json` **and** `Config/DefaultGame.ini`.
2. Re-run both build scripts, re-import `DT_Provinces.json` (Reimport on the DataTable) and press **Import** on the `DTGeoImporter` again.
The importer refuses files built with a different scale/origin.

## Other road sources
- Geofabrik: `osmium tags-filter thailand-latest.osm.pbf w/highway=motorway,trunk,primary,motorway_link,trunk_link,primary_link -o main.osm.pbf && osmium export main.osm.pbf -o roads.geojson`
- Overpass: `way["highway"~"^(motorway|trunk|primary)(_link)?$"](S,W,N,E); out geom tags;` saved as JSON

Data is © OpenStreetMap contributors, ODbL — see `Content/_Downtown/Data/ATTRIBUTION.md`.
