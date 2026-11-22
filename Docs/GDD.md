# Game Design Document — Downtown (Open-World Action Prototype)

**Engine:** Unreal Engine 5.5+ (C++ คู่กับ Blueprint)
**สเกล:** Indie Prototype, ทีม 1–3 คน, ใช้เวลาประมาณ 3–4 เดือน

---

## 1. ภาพรวมเกม

| หัวข้อ | รายละเอียด |
|---|---|
| **แนวเกม** | Third-Person Open-World Action / Sandbox |
| **Core Loop** | สำรวจเมือง → ก่อเหตุหรือทำภารกิจ → ถูกตำรวจไล่ → หลบหนีหรือสู้ → กลับไปสำรวจต่อ |
| **Design Pillars** | 1) ขยับตัวแล้วรู้สึกดีทั้งตอนเดินเท้าและขับรถ 2) โลกตอบสนองต่อผู้เล่น (NPC หนี, ตำรวจตามมา) 3) สลับเดินกับขับได้ทันที ไม่มีโหลด |
| **แผนที่ Prototype** | เมืองขนาดประมาณ 1×1 กม. มีถนน 4–6 สาย ทางแยก 6–10 จุด และทางเท้าครบ |
| **Out of Scope (เฟส 1)** | ภารกิจเนื้อเรื่อง, ร้านค้า, Save/Load, Multiplayer, ระบบเครื่องแต่งกาย |

### กฎสถาปัตยกรรม
- เขียน **ลอจิกหลักใน C++** และใช้ **Blueprint สำหรับปรับค่า, ต่อ Asset และทำ Prototype เร็ว**
- ระบบต่างๆ สื่อสารกันผ่าน **Delegate/Event** ไม่ให้ระบบไหนอ้างถึงอีกระบบตรงๆ เช่น อาวุธแค่ประกาศ `OnWeaponFired` แล้ว Wanted System รับไปตัดสินเอง
- ระบบที่มีชุดเดียวในโลก (Wanted, Police Dispatch, Population, Minimap) ทำเป็น **`UWorldSubsystem`** และอ่าน Config จาก `ADTGameMode`
- ค่าปรับจูนทั้งหมดอยู่ใน **Data Asset** (`UPrimaryDataAsset`) เพื่อให้ Designer ปรับได้โดยไม่ต้องแตะโค้ด
- Prefix ของคลาส C++ คือ `DT` (Downtown)

---

## 2. โครงสร้างโฟลเดอร์

### 2.1 Source (C++) — `Source/Downtown/`
```
Core/              ADTGameMode, ADTPlayerController
Character/         ADTCharacterBase, ADTPlayerCharacter
Combat/            UDTHealthComponent, UDTWeaponComponent, ADTWeaponBase, UDTWeaponData
Interaction/       IDTInteractable, UDTInteractionComponent
Vehicle/           ADTVehicleBase, UDTVehicleSeatComponent, UDTVehicleEntryComponent, UDTVehicleData
AI/Traffic/        ADTTrafficLane, ADTTrafficLight, ADTTrafficAIController
AI/Pedestrian/     ADTSidewalkPath, ADTPedestrianCharacter, ADTPedestrianAIController
AI/Police/         ADTPoliceCharacter, ADTPoliceAIController, ADTPoliceVehicleAIController, UDTPoliceDispatchSubsystem
Wanted/            UDTWantedSubsystem, UDTCrimeData, UDTWantedConfig
World/             UDTPopulationSubsystem, UDTPopulationConfig
UI/                ADTHUD, UDTHUDWidget, UDTMinimapWidget, UDTMinimapSubsystem, UDTMapIconComponent
```

### 2.2 Content — `Content/_Downtown/`
มีขีดล่างนำหน้าเพื่อให้โฟลเดอร์ของโปรเจกต์อยู่บนสุดและไม่ปนกับ Asset จาก Marketplace/Fab
```
Core/              BP_GameMode, BP_PlayerController, BP_HUD
Input/Actions/     IA_Move, IA_Look, IA_Jump, IA_Sprint, IA_Aim, IA_Fire, IA_Reload, IA_SwitchWeapon, IA_Interact,
                   IA_Throttle, IA_Brake, IA_Steer, IA_Handbrake, IA_VehicleLook, IA_ExitVehicle
Input/Contexts/    IMC_OnFoot, IMC_Vehicle
Characters/Player/ BP_PlayerCharacter, ABP_Player, Montages/ (AM_EnterVehicle, AM_CarJack, AM_ExitVehicle, AM_BailOut)
Characters/Pedestrians/  BP_Pedestrian_Base, Variants/
Characters/Police/ BP_PoliceOfficer
Weapons/           BP_Weapon_Pistol, BP_Weapon_Rifle, DA_Weapon_Pistol, DA_Weapon_Rifle, VFX/, SFX/
Vehicles/Shared/   Wheels/ (BP_Wheel_Front, BP_Wheel_Rear)
Vehicles/Sedan/    BP_Vehicle_Sedan, DA_Vehicle_Sedan
Vehicles/Police/   BP_Vehicle_PoliceCruiser
World/             BP_TrafficLane, BP_TrafficLight, BP_SidewalkPath
Wanted/            DA_WantedConfig, DA_Crime_Shooting, DA_Crime_Murder, DA_Crime_CarJack, DA_Crime_CopKill
UI/HUD/            WBP_HUD, WBP_HealthBar, WBP_AmmoCounter, WBP_WantedStars, WBP_Crosshair, WBP_Speedometer
UI/Minimap/        WBP_Minimap, WBP_MapIcon, T_MinimapTexture, M_MinimapMask
Maps/              L_Downtown_Main (World Partition), L_Test_Character, L_Test_Vehicle, L_Test_AI
Environment/ Audio/ VFX/ Materials/
```

**Naming Convention:** `BP_` Blueprint · `WBP_` Widget · `ABP_` Anim BP · `AM_` Montage · `BT_`/`BB_` Behavior Tree/Blackboard · `DA_` Data Asset · `IA_`/`IMC_` Input · `SK_`/`SM_` Mesh · `M_`/`MI_` Material · `T_` Texture · `L_` Level

---

## 3. รายละเอียดระบบ

### ระบบ 1 — Third-Person Character Controller
**Feature:** เดิน/วิ่ง, กระโดด, เล็ง (กล้องซูมเข้าไหล่), ยิงแบบ Hitscan, รีโหลด, เปลี่ยนอาวุธ 2 ช่อง

| คลาส | หน้าที่ |
|---|---|
| `ADTCharacterBase` | ฐานร่วมของ Player/คนเดินเท้า/ตำรวจ: Health, TakeDamage, Death + Ragdoll, รายงาน `DeathCrime` เมื่อผู้เล่นเป็นคนฆ่า |
| `ADTPlayerCharacter` | SpringArm + Camera, ผูก Enhanced Input, โหมดเล็ง (หันตามกล้อง + ซูม), รายงาน `ShootingCrime` เมื่อยิง |
| `UDTHealthComponent` | HP, `OnHealthChanged`, `OnDeath` |
| `UDTWeaponComponent` | ถืออาวุธ, ยิง (Semi/Auto), รีโหลด, Recoil, `OnAmmoChanged`, `OnWeaponFired` |
| `ADTWeaponBase` | Line Trace สองชั้น (กล้อง → เป้า แล้วปากกระบอก → จุดนั้น) กันยิงทะลุกำแพง, ส่ง Noise "Gunshot" ให้ AI ได้ยิน |
| `UDTWeaponData` | Damage, Fire Rate, Mag, Spread, Recoil, Noise Range, FX |
| `IDTInteractable` + `UDTInteractionComponent` | สแกนหาสิ่งที่ Interact ได้ทุก 0.1 วินาที และส่ง Prompt ให้ HUD |

### ระบบ 2 — Vehicle System
**Feature:** ฟิสิกส์ Chaos Vehicles, กล้องตามรถที่กลับตำแหน่งเอง, ขึ้นลงรถพร้อมแอนิเมชัน, ดึงคนขับ NPC ออก (Carjack), กระโดดออกจากรถที่วิ่งเร็ว

| คลาส | หน้าที่ |
|---|---|
| `ADTVehicleBase : AWheeledVehiclePawn` | Input การขับ, กล้อง, Health, Implement `IDTInteractable`, ฟังก์ชัน `DriveTowards()` ให้ AI ใช้ |
| `UDTVehicleSeatComponent` | ตำแหน่งที่นั่ง, `DoorOffset`/`ExitOffset` (ลากปรับใน Viewport ได้), คนที่นั่งอยู่ |
| `UDTVehicleEntryComponent` (อยู่บน Character) | State Machine `None → Entering → Seated → Exiting` |
| `UDTVehicleData` | Max Health, ความเร็วที่ลงรถได้อย่างปลอดภัย, ค่าสำหรับ AI |

**Flow ขึ้นรถแบบไม่มีรอยต่อ:**
1. กด `IA_Interact` แล้ว `UDTInteractionComponent` ส่งต่อให้ `ADTVehicleBase::Interact`
2. จองที่นั่ง แล้วตั้ง **Motion Warping** target `VehicleDoor` ที่ประตู
3. ถ้ามีคนขับ AI ให้เรียก `EjectAIDriver()` ซึ่งจะ Spawn NPC และรายงาน `CarJackCrime` จากนั้นเล่น `AM_CarJack` (ถ้าไม่มีคนขับเล่น `AM_EnterVehicle`)
4. เมื่อ Montage จบ ให้ปิด Collision และ Movement, Attach เข้ากับ Seat แล้วเรียก `Possess(Vehicle)` ตามด้วย `SetViewTargetWithBlend` ให้กล้องค่อยๆ เลื่อน
5. `ADTPlayerController::OnPossess` สลับ Input Context จาก `IMC_OnFoot` เป็น `IMC_Vehicle` เอง
6. ตอนลงรถ: หาจุดว่างฝั่งประตูก่อน ถ้าไม่ว่างลองอีกฝั่ง ถ้าไม่ว่างทั้งสองฝั่งให้ขึ้นไปบนหลังคา ถ้ารถวิ่งเร็วกว่า `SafeExitSpeedKmh` ให้ใช้ท่ากระโดดออก (Launch + `AM_BailOut`)

> ถ้ายังไม่มี Montage ระบบจะขึ้นลงรถทันทีโดยไม่มีแอนิเมชัน จึงทดสอบ Flow ได้ตั้งแต่วันแรก

### ระบบ 3 — Traffic & Pedestrian AI
ใช้ **Spline Lane Graph** ที่วางเองในแผนที่ ซึ่ง Debug ง่ายกว่า ZoneGraph/MassAI ค่อยย้ายไป MassAI เมื่อต้องการ NPC เกิน 100 ตัว

| คลาส | หน้าที่ |
|---|---|
| `ADTTrafficLane` | Spline ทางเดียว, Speed Limit, `NextLanes` (ทางแยก), ไฟจราจรที่คุมปลายเลน |
| `ADTTrafficLight` | วงจร เขียว → เหลือง → แดง ตาม Timer, ตั้ง `PhaseOffset` ให้สองทิศสลับกัน |
| `ADTTrafficAIController` | Pure Pursuit บน Spline, ปรับความเร็วตาม Speed Limit, Box Sweep หาสิ่งกีดขวางแล้วเบรก, หยุดก่อนเส้นหยุดเมื่อไฟแดง |
| `ADTSidewalkPath` | Spline ทางเท้าหรือทางม้าลาย ต่อกันเป็น Graph, ตั้ง `bBlocked` ให้ทางม้าลายได้ |
| `ADTPedestrianCharacter` | สุ่ม Mesh, ใช้ RVO Avoidance |
| `ADTPedestrianAIController` | State **Wander** (เดินตาม Spline, เยื้องซ้ายขวาเล็กน้อย) → **Flee** (ได้ยินปืน, เห็นศพ, โดนทำร้าย แล้วสุ่มจุดที่ไกลจากภัย) → **Cower** (ถ้าไม่มีทางหนี) → กลับไป Wander |
| `UDTPopulationSubsystem` + `UDTPopulationConfig` | Spawn คนและรถในวงรอบผู้เล่นนอกมุมกล้อง (5–150 ม.) และ Despawn เมื่อไกลเกิน 200 ม. โดยไม่ให้ผู้เล่นเห็น |

### ระบบ 4 — Wanted Level (1–3 ดาว)

| คลาส | หน้าที่ |
|---|---|
| `UDTWantedSubsystem` | Heat, ระดับดาว, Last Known Location, เช็กพยาน, Timer หลบหนี, `OnWantedLevelChanged`, `OnPlayerVisibilityChanged` |
| `UDTCrimeData` | Heat ที่ได้, ต้องมีพยานหรือไม่, ดาวขั้นต่ำที่บังคับ, Cooldown กันรายงานซ้ำ |
| `UDTWantedConfig` | ตารางกฎของแต่ละดาว, คลาสตำรวจ, ระยะ Spawn/Despawn |
| `UDTPoliceDispatchSubsystem` | ทยอย Spawn ตำรวจและรถตำรวจนอกสายตาผู้เล่นให้ครบตามดาว และเก็บกวาดเมื่อหลบหนีได้ |
| `ADTPoliceCharacter` + `ADTPoliceAIController` | Chase → Attack (ยิงเป็นชุด เฉพาะระดับที่อนุญาต Lethal Force) → Search รอบ Last Known Location, รายงานทุกครั้งที่เห็นผู้เล่น |
| `ADTPoliceVehicleAIController` | ไล่ตามโดยเล็งดักทางหน้ารถเป้าหมาย, พุ่งชนที่ 3 ดาว, จอดแล้วปล่อยตำรวจลงไปเมื่อผู้เล่นเดินเท้า |

**กฎของดาว** (ค่าเริ่มต้นใน `UDTWantedConfig`):

| ดาว | Heat | สิ่งที่ส่งมา | ยิงหรือไม่ | เงื่อนไขหลบหนี |
|---|---|---|---|---|
| ⭐ | 1+ | ตำรวจเดินเท้า 2 นาย | ไม่ยิง | ออกนอกรัศมี 60 ม. และไม่ถูกเห็น 10 วินาที |
| ⭐⭐ | 50+ | เดินเท้า 2 นาย + รถตำรวจ 2 คัน | ยิง | รัศมี 120 ม., 20 วินาที |
| ⭐⭐⭐ | 150+ | เดินเท้า 4 นาย + รถตำรวจ 4 คัน (พุ่งชน) | ยิง | รัศมี 200 ม., 30 วินาที |

**Crime ที่แนะนำ:** `DA_Crime_Shooting` (Heat 10, ต้องมีพยาน), `DA_Crime_CarJack` (20, ต้องมีพยาน), `DA_Crime_Murder` (40, ต้องมีพยาน), `DA_Crime_CopKill` (100, ไม่ต้องมีพยาน, ขั้นต่ำ 2 ดาว)

**Flow:** อาวุธ, การตาย หรือการขึ้นรถเรียก `ReportCrime()` → เช็กว่ามี NPC หรือตำรวจเห็นในรัศมี (Line Trace) → เพิ่ม Heat → ดาวขึ้น → Dispatch ส่ง Unit และ HUD อัปเดต
เมื่อตำรวจคลาดสายตาผู้เล่นเกิน 1 วินาที ดาวจะกะพริบ Minimap จะแสดงวงค้นหา และ Timer หลบหนีจะเริ่มนับเมื่อผู้เล่นออกนอกวงนั้น

### ระบบ 5 — HUD & Minimap

| คลาส | หน้าที่ |
|---|---|
| `ADTHUD` | สร้าง `WBP_HUD` |
| `UDTHUDWidget` | Bind กับ Delegate ของระบบต่างๆ แล้วเรียก Event ใน Blueprint: `OnHealthUpdated`, `OnAmmoUpdated`, `OnWantedLevelUpdated`, `OnWantedFlashingChanged`, `OnInteractionPromptChanged`, `OnVehicleModeChanged`, `OnSpeedUpdated` |
| `UDTMinimapWidget` | เลื่อน/หมุน/ซูมแผนที่ผ่านพารามิเตอร์ของ Material, คำนวณตำแหน่งไอคอนและวงค้นหาของตำรวจ |
| `UDTMinimapSubsystem` | ทะเบียนไอคอน + แปลงพิกัดโลกเป็น UV ของแผนที่ |
| `UDTMapIconComponent` | ติดไว้กับ Actor ที่ต้องแสดงบนแผนที่ (ผู้เล่น, ตำรวจ, รถ, เป้าหมาย) |

**วิธีทำ Minimap:** ใช้ภาพที่ Bake ไว้ล่วงหน้า โดยวาง `SceneCapture2D` แบบ Orthographic ถ่ายจากด้านบนครั้งเดียว แล้วบันทึกเป็น `T_MinimapTexture` จากนั้นตั้ง `MinimapWorldBounds` ใน `BP_GameMode` ให้ตรงกับพื้นที่ที่ถ่าย วิธีนี้ถูกกว่าการใช้ SceneCapture แบบเรียลไทม์มาก

---

## 4. Step-by-Step Roadmap

ประมาณการคิดจากนักพัฒนา 1–2 คน แต่ละ Milestone ต้องผ่าน **Definition of Done (DoD)** ก่อนไปขั้นถัดไป
✅ = มีโค้ด C++ ใน repo แล้ว เหลือสร้าง Asset/Blueprint ตาม `Docs/EditorSetup.md`

### Phase 0 — Foundation (สัปดาห์ที่ 1)
- [x] โปรเจกต์ C++, Plugin (ChaosVehicles, EnhancedInput, MotionWarping), `.gitignore`, `.gitattributes` (LFS)
- [x] `ADTGameMode`, `ADTPlayerController`, โครงโฟลเดอร์
- [ ] Build ครั้งแรก, สร้าง Test Level แยกตามระบบ
- **DoD:** เปิดเกมได้ และ Build ผ่านทั้ง Editor และ Development

### Phase 1 — Character & Combat (สัปดาห์ที่ 2–4) ✅
1. `BP_PlayerCharacter` + `IMC_OnFoot` + Input Actions
2. `ABP_Player`: Locomotion Blend Space, Jump, Aim Offset, Upper-body Slot
3. `BP_Weapon_Pistol/Rifle` + `DA_Weapon_*`
4. `WBP_HUD` ขั้นแรก: Health, Ammo, Crosshair
- **DoD:** วิ่ง กระโดด และยิงเป้าซ้อมจนตายได้ใน `L_Test_Character` และ Game Feel ไม่หน่วง

### Phase 2 — Greybox City (สัปดาห์ที่ 5)
- [ ] บล็อกเมืองด้วย Modeling Tools: ถนน, ทางเท้า, ตึก
- [ ] เปิด World Partition และสร้าง NavMesh
- [ ] วาง `BP_TrafficLane` และ `BP_SidewalkPath` แล้วต่อเป็น Graph
- **DoD:** เดินทั่วเมืองได้ และถนนกว้างพอสำหรับรถ 2 เลน

### Phase 3 — Vehicle System (สัปดาห์ที่ 6–8) ✅
1. `BP_Vehicle_Sedan`: ตั้ง Wheel, Torque Curve, Suspension แล้วจูน (เผื่อเวลาไว้เยอะ)
2. `IMC_Vehicle` + Input การขับ
3. Montage ขึ้น/ลง/Carjack/กระโดดออก พร้อม Motion Warping notify
4. `WBP_Speedometer`
- **DoD:** เดินไปขึ้นรถ ขับ แล้วลงรถได้ 20 ครั้งติดโดยไม่มีบั๊กกล้องหรือ Collision

### Phase 4 — Pedestrian AI (สัปดาห์ที่ 9–10) ✅
1. `BP_Pedestrian_Base` + Mesh Variants
2. `DA_PopulationConfig` + ตั้งค่าใน `BP_GameMode`
- **DoD:** มี NPC 30–50 ตัวเดินทั่วเมือง หนีเมื่อมีเสียงปืน และ FPS ไม่ตกเกินเป้า

### Phase 5 — Traffic AI (สัปดาห์ที่ 11–12) ✅
1. `BP_TrafficLight` (หลอดไฟเปลี่ยนสีตาม `OnLightChanged`) แล้วผูกกับ Lane ที่ปลายแยก
2. เพิ่ม `BP_Vehicle_Sedan` ลงใน `TrafficVehicleClasses` และตั้ง `DriverNPCClass`
- **DoD:** มีรถ 15–25 คันวิ่งวนทั้งเมือง หยุดที่ไฟแดง ไม่ชนกันเอง และหยุดเมื่อผู้เล่นขวาง

### Phase 6 — Wanted & Police (สัปดาห์ที่ 13–14) ✅
1. `DA_WantedConfig` + `DA_Crime_*` แล้วนำไปใส่ใน Player, Pedestrian, Police และ Vehicle
2. `BP_PoliceOfficer` (อาวุธเริ่มต้น + `DeathCrime` = CopKill)
3. `BP_Vehicle_PoliceCruiser` (`AIControllerClass` = `ADTPoliceVehicleAIController`, ไซเรน)
4. `WBP_WantedStars` (กะพริบตาม `OnWantedFlashingChanged`)
- **DoD:** ก่ออาชญากรรมแล้วดาวขึ้นถูกต้อง ตำรวจไล่ตามระดับ และหลบหนีจนดาวหายได้

### Phase 7 — Minimap & Polish (สัปดาห์ที่ 15–16) ✅ (โค้ด)
1. Bake `T_MinimapTexture`, ทำ `M_MinimapMask` และ `WBP_Minimap`
2. ติด `UDTMapIconComponent` ให้ผู้เล่น ตำรวจ และรถตำรวจ
3. Profiling ด้วย Unreal Insights: ถ้าเจอ Spawn Hitch ให้ทำ Object Pool ใน `UDTPopulationSubsystem` และลด Tick ของ NPC ที่อยู่ไกล
4. Bug Bash และจูน Game Feel
- **DoD:** เล่นต่อเนื่องได้ 15 นาทีโดยไม่ Crash และ Core Loop ครบ

### Dependency Map
```
Foundation → Character/Combat → Greybox City → Vehicle
                    │                  │           │
                    ▼                  ▼           ▼
               HUD (ขั้นแรก)   Pedestrian AI → Traffic AI
                                       │           │
                                       ▼           ▼
                                   Wanted & Police
                                           │
                                           ▼
                                   Minimap & Polish
```

---

## 5. ความเสี่ยงและวิธีลด

| ความเสี่ยง | วิธีลดความเสี่ยง |
|---|---|
| จูนฟิสิกส์รถใช้เวลานานเกินแผน | เริ่มจากค่าของ Vehicle Template ของ Epic และกำหนด Timebox การจูนไว้ |
| AI กิน CPU | จำกัดจำนวนใน `DA_PopulationConfig`, Traffic Tick ที่ 20 Hz, ทำ Pooling, ย้ายไป MassAI ในเฟสหลัง |
| บั๊กกล้องหรือ Collision ตอนขึ้นลงรถ | ใช้ State Machine ที่ชัดเจนใน `UDTVehicleEntryComponent` และทดสอบโหมดไม่มี Montage ก่อน |
| Scope Creep | ทำเฉพาะที่อยู่ในข้อ 1 ส่วนไอเดียใหม่ให้จดลง Backlog ของเฟส 2 |
| ใช้ Asset ที่ติดลิขสิทธิ์ | ใช้ของจาก Fab/Marketplace ที่ License ถูกต้อง และไม่เลียนแบบชื่อ ตัวละคร หรืองานศิลป์ของเกมใดเกมหนึ่ง |

## 6. Backlog (เฟส 2)
ภารกิจ, ร้านค้าอาวุธ, Save/Load, รถหลายประเภท (มอเตอร์ไซค์), ด่านกั้นถนนของตำรวจ, เฮลิคอปเตอร์ (4–5 ดาว), วงจรกลางวัน/กลางคืน, วิทยุในรถ, MassAI Traffic
