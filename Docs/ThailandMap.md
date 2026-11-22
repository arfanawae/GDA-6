# แผนที่ประเทศไทยย่อสเกล (77 จังหวัด) — คู่มือเริ่มต้น

**Engine:** Unreal Engine 5 (ใช้ต่อกับระบบเดิมของโปรเจกต์ เช่น Traffic, HUD, Minimap)
**สเกล:** ระยะทางแนวราบ 1:20 (ระยะจริง 100 กม. = 5 กม. ในเกม), ความสูงภูมิประเทศ 1:5
**ฉากทดสอบแรก:** กรุงเทพฯ (TH-10), นนทบุรี (TH-12), ปทุมธานี (TH-13) ซึ่งติดกันครบทั้ง 3 จังหวัด

> ส่วนที่เป็น Python (`Tools/GeoPipeline`) ให้ผลลัพธ์เป็น JSON ที่ใช้กับ Engine ไหนก็ได้
> ถ้าจะใช้กับ Unity ให้สลับแกนเป็น Unity X = ตะวันออก, Z = เหนือ และใช้หน่วยเมตร (หาร 100) ส่วนอื่นเหมือนกันทั้งหมด

---

## 1. ตัวเลขที่ควรรู้ก่อนเริ่ม

| รายการ | ของจริง | ในเกม (1:20) |
|---|---|---|
| ประเทศไทยทั้งประเทศ (กรอบ เหนือ-ใต้ × ตะวันออก-ตะวันตก) | ~1,650 × 880 กม. | **82.6 × 44.1 กม.** |
| พื้นที่บก | 513,000 กม.² | ~1,280 กม.² |
| กรุงเทพฯ → เชียงใหม่ (เส้นตรง) | 583 กม. | 29.1 กม. ≈ ขับ **22 นาที** ที่ 100 กม./ชม. |
| กรุงเทพฯ → หาดใหญ่ (เส้นตรง) | 750 กม. | 37.5 กม. ≈ ขับ **28 นาที** |
| ฉากทดสอบ 3 จังหวัด | ~87 × 74 กม. | **4.35 × 3.72 กม.** |
| ถนนสายหลักในฉากทดสอบ (motorway/trunk/primary) | ~3,265 กม. | 163 กม. |

**กฎสำคัญ: ย่อ "ระยะทาง" แต่ไม่ย่อ "วัตถุ"**
รถ คน อาคาร และความกว้างถนนต้องเป็นขนาดจริง (เลนกว้าง 3.5 ม.) ไม่อย่างนั้นจะขับไม่ได้ ผลที่ตามมาคือ:
- ความเร็วรถใช้ค่าจริงได้เลย (100 กม./ชม.) โลกจึงดู "เล็กลง 20 เท่า" ในแง่เวลาเดินทาง
- เมืองที่ถนนหนาแน่นจะเบียดกัน จากการวัดในฉากทดสอบ **ถนนคนละสายที่อยู่ห่างกันไม่ถึง 10 ม. (ในเกม) มีประมาณ 19% ของความยาวถนน** (ดูข้อ 7)
- ถนนที่มีเกาะกลาง OSM วาดไว้เป็น 2 เส้นห่างกัน ~20 ม. พอย่อแล้วเหลือ ~1 ม. และทับกัน ตัว Importer แก้ให้อัตโนมัติ (`bSeparateDualCarriageways`)

---

## 2. การแปลงพิกัด (Projection)

ใช้ **Transverse Mercator แบบทรงกลม** โดยให้จุดศูนย์กลางของโลกเกมอยู่ที่ **13.0°N 101.0°E** (กลางประเทศ) เพื่อให้พิกัดในเกมมีค่าไม่เกิน ±42 กม.

```
B        = cos(φ) · sin(λ − λ₀)
Easting  = ½ · R · ln((1 + B) / (1 − B))
Northing = R · (atan2(tan φ, cos(λ − λ₀)) − φ₀)          R = 6,371,008.8 ม.

UE X (เหนือ) = Northing × 100 / HorizontalScale    (cm)
UE Y (ตะวันออก) = Easting × 100 / HorizontalScale   (cm)
UE Z = ความสูง(ม.) × 100 / VerticalScale
```

เหตุผลที่เลือกวิธีนี้:
- เป็น **Conformal** คือรักษารูปทรงและมุม จังหวัดและทางโค้งจึงไม่บิดเบี้ยว (ต่างจากการเอา lat/lon คูณค่าคงที่ตรงๆ ซึ่งยืดแกนตะวันออก-ตะวันตกผิดไป 2–4%)
- ใช้ระบบเดียวครอบคลุมทั้งประเทศ ไม่ต้องแบ่งโซนเหมือน UTM 47/48
- คลาดเคลื่อนจากทรงรี WGS84 ประมาณ 0.3% ซึ่งมองไม่เห็นหลังย่อ 1:20
- สูตรเดียวกันอยู่ทั้งใน Python (`geo_projection.py`) และ C++ (`UDTGeoLibrary`) ข้อมูลที่ import กับพิกัดที่คำนวณตอนเล่นจึงตรงกันเสมอ

ค่าคงที่ทั้งหมดอยู่ 2 ที่ และ **ต้องตรงกัน**: `Tools/GeoPipeline/geo_config.json` กับ `Config/DefaultGame.ini` (`[/Script/Downtown.DTGeoSettings]`)
ถ้าไม่ตรง ตัว Importer จะปฏิเสธไฟล์และบอกค่าที่ต่างกันใน Output Log

> **Large World Coordinates (UE5):** โลกขนาด 83 × 44 กม. ยังอยู่ในขอบเขตที่ double precision ของ UE5 รองรับได้สบาย ไม่ต้องทำ Floating Origin

---

## 3. แหล่งข้อมูล

| ข้อมูล | แหล่ง | License |
|---|---|---|
| ขอบเขต 77 จังหวัด | geoBoundaries `THA ADM1` (gbOpen, มาจาก OSM) | ODbL 1.0 |
| ถนน | HOT/HDX "Thailand Roads (OpenStreetMap Export)" GeoParquet ทั้งประเทศ | ODbL 1.0 |
| ถนน (ทางเลือก) | Geofabrik `thailand-latest.osm.pbf` → `osmium export` เป็น GeoJSON, หรือ Overpass API | ODbL 1.0 |
| แนวชายฝั่ง | OSM land polygons (osmdata.openstreetmap.de) | ODbL 1.0 |
| ความสูง (เฟสถัดไป) | Copernicus DEM GLO-30 / SRTM | ใช้ได้ฟรี ต้องให้เครดิต |

ข้อมูลที่ใช้มาจาก OSM ทั้งหมด จึงต้องใส่เครดิต **"© OpenStreetMap contributors"** ในเกม (ดู `Content/_Downtown/Data/ATTRIBUTION.md`)

---

## 4. Pipeline: จากแผนที่จริงสู่เกม

```
geoBoundaries ADM1 (GeoJSON) ─► build_provinces.py ─► DT_Provinces.json ──► DataTable (FDTProvinceRow)
                                        │                                     └► UDTProvinceSubsystem (เช็กจังหวัด)
                                        ├► map_bounds.json (ขนาดพื้นที่, Minimap bounds)
                                        └► T_Minimap_*.png (แผนที่ระบายสีตามภาค)

HDX roads (GeoParquet) ─► build_roads.py ─► Roads_TestArea.json ─► ADTGeoImporter (ปุ่ม Import ใน Editor)
                                │                                     ├► ADTGeoRoad (ถนน, แบ่งชิ้นตาม World Partition)
                                │                                     ├► ADTTrafficLane (กราฟเลนให้ Traffic AI, ชิดซ้าย)
                                └► จุดที่ถนนข้ามเขตจังหวัด ───────────► └► ADTProvinceSign (ป้าย "ยินดีต้อนรับสู่...")
```

คำสั่งทั้งหมดอยู่ใน `Tools/GeoPipeline/README.md`

### สิ่งที่สคริปต์ทำให้
- **build_provinces.py:** จับคู่รหัส ISO 3166-2 (`TH-10` … `TH-96`) กับตาราง `thai_provinces.csv` (ชื่อไทย, อังกฤษ, ภาคตามระบบ 6 ภาค) → แปลงพิกัด → ลดจำนวนจุด (Simplify 10 ม. ในเกม) → ตัดเกาะที่เล็กกว่า 5 กม.² ทิ้ง → คำนวณจุดอ้างอิง (centroid ที่รับประกันว่าอยู่ในจังหวัด) และรายชื่อจังหวัดที่ติดกัน
- **build_roads.py:** กรองเฉพาะ motorway/trunk/primary (+ ทางเชื่อม) → ตัดให้อยู่ในพื้นที่ทดสอบ → รวมชิ้นถนนที่ OSM แบ่งไว้ → ทางด่วนยกระดับเก็บค่า `level` ไว้ในพิกัด Z ของแต่ละจุด → หาจุดที่ถนนตัดเส้นเขตจังหวัด

### ผลที่ตรวจแล้วกับข้อมูลจริง
- ได้ครบ 77 จังหวัด แบ่งตามภาคได้ เหนือ 9 / อีสาน 20 / กลาง 22 / ตะวันออก 7 / ตะวันตก 5 / ใต้ 14
- พื้นที่กรุงเทพฯ ที่คำนวณได้ 1,576.7 กม.² (ค่าทางการ 1,568.7) รายชื่อจังหวัดที่ติดกรุงเทพฯ ตรงกับความจริงทั้ง 6 จังหวัด
- ฉากทดสอบมีถนน 1,481 ชิ้น และจุดป้ายข้ามเขต 97 จุด (ตัดจุดที่ถนนวิ่งเลียบเส้นเขตออกแล้ว)

---

## 5. โครงสร้างข้อมูลจังหวัด

```cpp
UENUM() enum class EDTThaiRegion { North, Northeast, Central, East, West, South };

USTRUCT() struct FDTProvinceRow : FTableRowBase    // Row Name = "TH-10"
{
    FName      Code;             // "TH-10"
    FText      NameTH;           // "กรุงเทพมหานคร"
    FText      NameEN;           // "Bangkok"
    EDTThaiRegion Region;
    FVector2D  ReferencePoint;   // พิกัดในเกม (cm) ใช้เป็นจุดเกิด/วาร์ป
    FVector2D  ReferenceLatLon;  // lat/lon ของจุดเดียวกัน
    float      RealAreaKm2;
    TArray<FName> Neighbors;     // จังหวัดที่ติดกัน → เช็กเฉพาะกลุ่มนี้
    TArray<FDTProvinceRing> Rings;  // เส้นขอบเขต (แผ่นดินหลัก + เกาะ)
};
```

นำเข้าโดยลาก `DT_Provinces.json` เข้า Content Browser แล้วเลือก **DataTable** → Row Struct = `DTProvinceRow` จากนั้นตั้งค่าใน `BP_GameMode.ProvinceTable`

---

## 6. ระบบเช็กพอยต์และแจ้งชื่อจังหวัด

ใช้ **Point-in-Polygon บนข้อมูล ไม่ต้องวาง Trigger Volume เอง** (77 จังหวัดวางเองไม่ไหว และเส้นเขตจริงไม่เป็นกล่อง)

`UDTProvinceSubsystem` (World Subsystem):
1. เช็กตำแหน่งผู้เล่นทุก 0.2 วินาที (ถ้าอยู่ในรถจะใช้ตำแหน่งรถ)
2. ลำดับการเช็ก: **จังหวัดปัจจุบัน → จังหวัดที่ติดกัน → ทั้งหมด** โดยกรองด้วย Bounding Box ก่อน ปกติจึงเช็กแค่ 1–7 จังหวัด
3. **Hysteresis:** ต้องอยู่ในจังหวัดใหม่ต่อเนื่อง 0.6 วินาทีก่อนประกาศ จะได้ไม่เด้งรัวๆ ตอนขับเลียบเส้นเขต ส่วนการออกทะเลจะไม่ล้างชื่อจังหวัดเดิม
4. Broadcast `OnProvinceChanged(New, Old, bFirstVisit)` และเก็บรายการจังหวัดที่เคยไป (`GetVisitedCount() / 77`) ใช้ทำความสำเร็จ "เที่ยวครบ 77 จังหวัด" และบันทึกลง Save ได้ด้วย `GetVisited`/`SetVisited`

`UDTHUDWidget::OnProvinceEntered(NameTH, NameEN, Region, bFirstVisit, Visited, Total)` เป็น Event ที่ `WBP_HUD` นำไปแสดง Banner เช่น:

```
┌───────────────────────────────┐
│  ยินดีต้อนรับสู่                │
│  นนทบุรี   NONTHABURI           │
│  ภาคกลาง · จังหวัดใหม่! 2/77     │
└───────────────────────────────┘
```

`ADTProvinceSign`: ป้ายข้างทางที่ Importer วางให้ทุกจุดที่ถนนข้ามเขต วางห่างจากเส้นเขตมา 15 ม. ทางซ้ายของถนน (ไทยขับชิดซ้าย) และหันหน้าเข้าหารถที่วิ่งมา

> **ฟอนต์ภาษาไทย:** ฟอนต์ Roboto ที่ติดมากับ UE ไม่มีตัวอักษรไทย ให้ import **Noto Sans Thai** หรือ **Sarabun** (Google Fonts, OFL) เป็น Font Asset แล้วใช้ใน UMG และ TextRender ไม่อย่างนั้นจะขึ้นเป็นกล่องสี่เหลี่ยม

---

## 7. เรื่องความหนาแน่นของเมือง (ต้องตัดสินใจ)

ถ้าย่อทุกอย่าง 1:20 แบบเท่ากัน กรุงเทพฯ จะเหลือ 2.6 × 3.3 กม. แต่ถนนยังกว้างเท่าเดิม ทางเลือกมีดังนี้:

| ทางเลือก | ข้อดี | ข้อเสีย |
|---|---|---|
| **A. ใช้ OSM เป็นโครงร่าง แล้วออกแบบเมืองเอง** *(แนะนำ)* | ทางหลวงระหว่างจังหวัดตรงตามจริง ส่วนในเมืองจัดวางให้เล่นสนุกได้ | ต้องใช้แรงคนออกแบบเมือง |
| B. นำเข้าเฉพาะ motorway/trunk ในเขตเมือง | อัตโนมัติ ไม่มีถนนทับกัน | ในเมืองถนนน้อยกว่าความจริงมาก |
| C. ย่อไม่เท่ากัน (ในเมือง 1:8, ชนบท 1:25) | เมืองโปร่งขึ้น | ต้องปรับ Projection แบบ piecewise และ lat/lon ในเมืองจะไม่ตรง |

แนะนำเริ่มจาก **A + B** คือนำเข้าทั้งหมดไปดูภาพรวมก่อน แล้วค่อยลบหรือย้ายถนนในเมืองด้วยมือ ส่วนทางหลวงระหว่างเมืองใช้ข้อมูลจริงได้เกือบทั้งหมด

---

## 8. ตั้งค่าฉากทดสอบ 3 จังหวัด (ทีละขั้น)

### 8.1 เตรียมข้อมูล (ทำแล้ว ไฟล์อยู่ใน repo)
- `Content/_Downtown/Data/Provinces/DT_Provinces.json` (ครบ 77 จังหวัด)
- `Content/_Downtown/Data/Roads/Roads_TestArea.json` (TH-10, TH-12, TH-13)
- `Content/_Downtown/Data/Provinces/map_bounds.json`

### 8.2 Project Settings
1. Project Settings → Game → **Downtown Geo**: ตรวจว่า OriginLat 13.0, OriginLon 101.0, HorizontalScale 20 (ตั้งไว้แล้วใน `DefaultGame.ini`)
2. ติดตั้งฟอนต์ภาษาไทย (ข้อ 6)

### 8.3 สร้าง Level
1. File → New Level → **Open World** แล้ว Save เป็น `Content/_Downtown/Maps/L_Test_Bangkok`
2. ลบ Landscape ที่ติดมากับ Template แล้วสร้างใหม่ให้พอดีพื้นที่ทดสอบ (4.35 × 3.72 กม.):
   - Landscape Mode → New, Section Size 127, Sections/Component 2×2, **Number of Components 10 × 10** (2,541 quads), **Scale X/Y = 200** (2 ม./quad) → ได้ **5.08 กม.**
   - Location: **X = 238,090  Y = −465,862  Z = 0** (มุมของ Landscape อยู่ที่มุมพื้นที่ทดสอบ เผื่อขอบไว้ ~350 ม.)
   - กรุงเทพฯ แทบไม่มีความสูง พื้นราบจึงใช้ได้ ส่วน Heightmap จริงค่อยใส่ในเฟสภาคเหนือ (ข้อ 10)
3. World Settings → World Partition → Runtime Settings → Grid "MainGrid": **Cell Size 25,600** (256 ม.), **Loading Range 76,800** (768 ม.) ซึ่งเหมาะกับรถที่วิ่ง 100–140 กม./ชม.
4. วาง `PlayerStart` ที่จุดอ้างอิงของนนทบุรี: **X = 514,666  Y = −327,855** (Z ให้อยู่เหนือพื้น) แล้วขับไปทางตะวันออกเฉียงใต้เข้ากรุงเทพฯ (X 429,475  Y −203,123) หรือไปทางตะวันออกเฉียงเหนือเข้าปทุมธานี (X 592,339  Y −170,989)

### 8.4 นำเข้าถนนและป้าย (ทดสอบทีละโซน)
1. วาง Actor **DTGeoImporter** (Place Actors → ค้นหา "DTGeoImporter")
2. ตั้งค่าใน Details panel:
   - RoadsFile = `_Downtown/Data/Roads/Roads_TestArea.json`
   - **ZoneFilter** = `test-area`, `TH-12`, `Nonthaburi`, `นนทบุรี`, `TH-10,TH-13`, `bangkok-metro`, `north` … หรือเว้นว่าง/`all` เพื่อนำเข้าทั้งไฟล์
   - ProvinceTable = `DT_Provinces` (ค่าเริ่มต้นชี้ไปที่ `/Game/_Downtown/Data/Provinces/DT_Provinces`)
   - SignClass = `BP_ProvinceSign` (ถ้ามีแล้ว)
3. กดปุ่มตามลำดับ:

| ปุ่ม | ทำอะไร |
|---|---|
| **Validate Data** | อ่านไฟล์และตรวจทุกอย่าง แล้วบอกว่า "จะ" spawn อะไรบ้าง แต่ไม่ spawn จริง |
| **Preview Zone** | วาดเส้นขอบจังหวัดในโซนเป็นสีเหลือง และจุดอ้างอิงเป็นทรงกลมสีเขียว ค้างใน Viewport 30 วินาที พร้อม log ขอบเขต (ใช้เช็กว่า Landscape คลุมโซนครบไหม) |
| **Import** | spawn จริง ทุก Actor ติด Tag `DTGeoImported`, `DTGeoRun_<n>`, `DTGeoZone_<โซน>` |
| **Clear Last Run** | ลบเฉพาะของที่ Import รอบล่าสุด |
| **Clear Zone** | ลบของที่ Import ไว้สำหรับโซนใน ZoneFilter |
| **Clear Imported** | ลบของที่ Importer ทุกตัว spawn ไว้ |

- `ClearBeforeImport` (ค่าเริ่มต้น `SameZone`): Import โซนเดิมซ้ำจะลบของเก่าก่อนเสมอ ถ้าเลือก `None` จะวางหลายโซนซ้อนกันได้
- `bTrimRoadsToZone`: ตัดถนนที่เส้นเขตของโซน (ถ้าปิด จะเก็บถนนทั้งเส้นที่แตะโซน)
- World Partition: การ Clear จะเจอเฉพาะ Actor ใน Cell ที่โหลดอยู่ ต้องโหลด Region ก่อน

### 8.5 GameMode / HUD
- `BP_GameMode`: ProvinceTable = `DT_Provinces`, MinimapWorldBounds = ค่า `minimap` ใน `map_bounds.json` (Min 274714, −397901 / Max 709466, −26034)
- `DA_PopulationConfig`: ใส่ `BP_Vehicle_Sedan` (รถ Traffic จะวิ่งบนเลนที่นำเข้ามา)
- `WBP_HUD`: Implement `OnProvinceEntered` เป็น Banner ที่เลื่อนเข้าออกภายใน 3 วินาที
- Minimap: สร้าง `T_Minimap_TestArea.png` ด้วย `build_provinces.py --minimap` แล้ว import เป็น Texture (ใช้ Compression UserInterface2D, ปิด sRGB ได้)

---

## 9. อ่านผลใน Output Log

เปิด **Window → Output Log** แล้วพิมพ์ `LogDTGeo` ในช่อง Search (ทุกบรรทัดของระบบนี้ใช้ Category นี้) ตัวอย่างบรรทัดที่ควรเห็นหลังกด Import:

```
LogDTGeo: [GeoImport] ===== IMPORT run #1 | importer DTGeoImporter_0 | zone 'test-area' =====
LogDTGeo: [GeoImport] Projection: origin 13.0000 N 101.0000 E = world (0,0); horizontal 1:20 (1 km in game = 20 km real, 100 km real = 5.00 km in game); vertical 1:5; road level 700 cm
LogDTGeo: [GeoImport] Province table /Game/_Downtown/Data/Provinces/DT_Provinces.DT_Provinces: 77/77 provinces usable.
LogDTGeo: [GeoImport] Zone 'test-area' -> 3 province(s): TH-10 Bangkok, TH-12 Nonthaburi, TH-13 Pathum Thani
LogDTGeo: [GeoImport] Reading C:/.../Content/_Downtown/Data/Roads/Roads_TestArea.json (334.1 KB)
LogDTGeo: [GeoImport] File projection: origin 13.0000 N 101.0000 E, 1:20
LogDTGeo: [GeoImport] Roads in file 1481 -> spawned ... road pieces, ... traffic lanes (... junction links)
LogDTGeo: [GeoImport]   skipped roads: ... outside zone, 0 malformed, 0 with < 2 valid points; ...
LogDTGeo: [GeoImport] Ground trace: .../... points hit the ground, ... used BaseZ=0
LogDTGeo: [GeoImport] Result bounds X ...  = 4.3x km x 3.7x km in game (8x x 7x km real) | lat 13.5.. , lon 100.3..
LogDTGeo: [GeoImport] ===== done in 1.23 s | undo with 'Clear Last Run' (tag DTGeoRun_1) or 'Clear Zone' (tag DTGeoZone_test-area) =====
```

สัญญาณที่ต้องระวัง:
- `Warning: ... point(s) skipped` / `road #N skipped`: ข้อมูลในไฟล์มีพิกัดว่าง ผิดรูปแบบ หรืออยู่นอกขอบเขต (เกิน ±100 กม.) ระบบข้ามรายการนั้นไป แสดงรายละเอียด 10 รายการแรก (ปรับได้ที่ `MaxDetailedWarnings`)
- `Error: ... projection mismatch`: ไฟล์ถูกสร้างด้วยสเกลหรือ Origin คนละค่ากับ Project Settings ระบบจะไม่ spawn อะไรเลย
- `No ground under ANY point`: ไม่มี Landscape ใต้โซน หรือ Cell ยังไม่โหลด ให้กด Preview Zone เพื่อดูตำแหน่ง
- `Zone provinces NOT in this file`: ไฟล์ถนนไม่ครอบคลุมจังหวัดนั้น ให้สร้างไฟล์ใหม่ด้วย `build_roads.py --area ...`

ตอนเล่น (PIE) ใช้คำสั่ง Console (กด `~`):

| คำสั่ง | ผลใน Log |
|---|---|
| `dt.Province.Zone TH-12` / `dt.Province.Zone all` | จำกัดการตรวจจับเฉพาะโซน |
| `dt.Province.List test-area` | รายชื่อจังหวัด จุดอ้างอิง lat/lon และขนาดในเกม/จริง |
| `dt.Province.Where` | ตำแหน่งผู้เล่น (X/Y/Z, lat/lon) และจังหวัดที่อยู่ |
| `dt.Province.Teleport Nonthaburi` | วาร์ป (รับทั้งรหัสและชื่อไทย/อังกฤษ) |
| `dt.Province.ResetVisited` | ล้างความคืบหน้า X/77 |
| `dt.Province.LogChecks 1` | log ทุกจังหวะที่ตรวจเจอจังหวัดใหม่ (ช่วงรอยืนยัน 0.6 วินาที) |
| `dt.Province.DrawBorders 1` | วาดเส้นเขต (เขียว = ปัจจุบัน, เหลือง = ในโซน, เทา = นอกโซน) |

ตอนเริ่ม PIE จะเห็น `[Province] Loaded 77/77 provinces ... (Central 22, Northeast 20, ...)` และเมื่อข้ามเขตจะเห็น
`[Province] Entered TH-12 Nonthaburi / นนทบุรี (Central) from TH-10 Bangkok | FIRST VISIT | visited 2/77 | at X=... (13.86..N 100.49..E)`

## 10. แผนการทดสอบ (สเกล + การโหลด)

| # | ทดสอบ | วิธี | ผ่านเมื่อ |
|---|---|---|---|
| 1 | ความถูกต้องของพิกัด | `dt.Province.DrawBorders 1` ขับดูเส้นเขต | เส้นเขต (เหลือง/เขียว) ตรงกับป้าย และ Banner ขึ้นภายใน ~1 วินาทีหลังข้ามเส้น |
| 2 | ไม่เด้งรัวที่เส้นเขต | ขับเลียบเส้นเขตกรุงเทพฯ–นนทบุรี | Banner ไม่ขึ้นซ้ำรัวๆ |
| 3 | ความรู้สึกของสเกล | จับเวลาขับจากกรุงเทพฯ ไปปทุมธานี (~1.5 กม. ในเกม) | ใช้เวลา 1–2 นาที ถ้ารู้สึกสั้นไปให้ลองปรับ HorizontalScale เป็น 15 แล้ว rebuild |
| 4 | Streaming | ขับทางด่วนเต็มความเร็ว เปิด `wp.Runtime.ToggleDrawRuntimeHash2D`, `stat streaming`, `stat unit` | ไม่มี log "Slow streaming", ไม่มีกระตุกเกิน 33 ms |
| 5 | วาร์ปข้ามจังหวัด | `dt.Province.Teleport TH-13` | ฉากโหลดเสร็จแล้วรถไม่ตกทะลุพื้น |
| 6 | Traffic | เปิด Population | รถ AI วิ่งชิดซ้าย และเลี้ยวต่อเลนที่ทางแยกได้ |

ถ้า Streaming ไม่ทัน: เพิ่ม Loading Range, เปิด HLOD (World Partition → Build HLODs), ลดจำนวน `ADTGeoRoad` ด้วยการเพิ่ม `MaxRoadPieceCm` หรือรวม Mesh

---

## 11. Roadmap ไปให้ครบ 77 จังหวัด

1. **ฉากทดสอบ 2 (ภูเขา):** เชียงใหม่ (TH-50), ลำพูน (TH-51), ลำปาง (TH-52) เพื่อทดสอบ Heightmap จริง (Copernicus DEM ÷ VerticalScale) และถนนบนเขา
2. **Heightmap ทั้งประเทศ:** แบ่ง Landscape ตามภาค ความละเอียด 4–8 ม./quad (ทั้งประเทศ ~83 × 44 กม.) แล้วใช้ PCG เติมรายละเอียด
3. **ชายฝั่งและทะเล:** ใช้ OSM land polygon ตัดขอบ Landscape และวาง Water Body (Ocean)
4. **ทางหลวงทั้งประเทศ:** `build_roads.py --area <ภาค>` ทีละภาค (motorway/trunk ทั้งประเทศ แล้วเพิ่ม primary เฉพาะเส้นที่จำเป็น)
5. **เมืองหลักแต่ละภาค:** ออกแบบเองตามทางเลือก A ในข้อ 7
6. **ระบบเที่ยวครบ 77 จังหวัด:** ใช้ `GetVisited()` ร่วมกับ Save Game, หน้าสมุดสะสมแสตมป์จังหวัด และแผนที่ใหญ่ (กด M) จาก `T_Minimap` ทั้งประเทศ
