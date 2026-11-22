# Editor Setup Checklist

The C++ classes are complete; these are the editor-side assets that wire them into a playable level.
Do them in order. Every step can be tested on its own in a `L_Test_*` map.

## 0. First build
1. Right-click `Downtown.uproject` → **Generate Visual Studio project files**.
2. Build `DowntownEditor` (Development Editor), open the project.
3. *Edit → Project Settings → Maps & Modes*: set the default GameMode to `BP_GameMode` once it exists.

## 1. Input (Content/_Downtown/Input)
| Asset | Value type | Suggested keys |
|---|---|---|
| `IA_Move` | Axis2D | WASD (Swizzle/Negate modifiers), Left stick |
| `IA_Look` | Axis2D | Mouse XY (Negate Y), Right stick |
| `IA_Jump` | Digital | Space, Gamepad A |
| `IA_Sprint` | Digital | Left Shift, L3 |
| `IA_Aim` | Digital | RMB, LT |
| `IA_Fire` | Digital | LMB, RT |
| `IA_Reload` | Digital | R, X |
| `IA_SwitchWeapon` | Digital | Q / Mouse wheel, Y |
| `IA_Interact` | Digital | F, Y |
| `IA_Throttle` | Axis1D | W, RT |
| `IA_Brake` | Axis1D | S, LT |
| `IA_Steer` | Axis1D | A (Negate) / D, Left stick X |
| `IA_Handbrake` | Digital | Space, A |
| `IA_VehicleLook` | Axis2D | Mouse XY, Right stick |
| `IA_ExitVehicle` | Digital | F, Y |

- `IMC_OnFoot`: the on-foot actions. `IMC_Vehicle`: the vehicle actions.
- `BP_PlayerController` (parent `ADTPlayerController`): set **OnFootContext** and **VehicleContext**.

## 2. Player
- `BP_PlayerCharacter` (parent `ADTPlayerCharacter`): mesh (UE5 Manny is fine), `ABP_Player`, all Input Action slots, `ShootingCrime = DA_Crime_Shooting`.
- `Weapons` component: `StartingWeapons = [BP_Weapon_Pistol, BP_Weapon_Rifle]`, add a socket `hand_r_weapon` on the skeleton's `hand_r` bone.
- `VehicleEntry` component: assign the montages (optional at first, since entry works without them).
  Montages need root motion + a **Motion Warping** notify state with target name `VehicleDoor`.
- `ABP_Player`: Locomotion Blend Space (Speed/Direction), Aim Offset driven by control rotation pitch,
  `DefaultSlot` for the fire/reload/vehicle montages, `bIsInVehicle` from `VehicleEntry->IsInVehicle()` → sitting pose.

## 3. Weapons
- `DA_Weapon_Pistol` (semi-auto, 25 dmg, 300 RPM, mag 12) / `DA_Weapon_Rifle` (auto, 18 dmg, 600 RPM, mag 30).
- `BP_Weapon_*` (parent `ADTWeaponBase`): skeletal mesh with a `Muzzle` socket, set `WeaponData`; implement **OnFired** for muzzle flash, tracer and impact FX.

## 4. Vehicles
- Start from the UE5 Vehicle template's sedan (mesh, physics asset, wheels).
- `BP_Vehicle_Sedan` (parent `ADTVehicleBase`): configure the Chaos movement component (wheels, torque curve), `VehicleData = DA_Vehicle_Sedan`,
  move the `DriverSeat` component to the driver position and drag the **DoorOffset / ExitOffset** widgets outside the driver door.
  Set the vehicle Input Action slots, `DriverNPCClass = BP_Pedestrian_Base`, `CarJackCrime = DA_Crime_CarJack`,
  and `AIControllerClass = ADTTrafficAIController`.
- `BP_Vehicle_PoliceCruiser` (child of `BP_Vehicle_Sedan`): police livery, siren/lightbar, `AIControllerClass = ADTPoliceVehicleAIController`,
  add a `UDTMapIconComponent` (PoliceVehicle).

## 5. World
- Build a greybox with Modeling Tools; add a **NavMeshBoundsVolume** covering sidewalks.
- `BP_TrafficLane` (parent `ADTTrafficLane`): draw one spline per lane, one-way, at road height; link `NextLanes` at junctions.
- `BP_TrafficLight` (parent `ADTTrafficLight`): bulbs that react to `OnLightChanged`; assign it to the lanes that end at its stop line.
  For opposite directions use `PhaseOffset = GreenTime + YellowTime`.
- `BP_SidewalkPath` (parent `ADTSidewalkPath`): splines along sidewalks; list neighbours in `ConnectedPaths` (ends within 3 m connect).

## 6. Pedestrians & Population
- `BP_Pedestrian_Base` (parent `ADTPedestrianCharacter`): mesh variants, anim BP, `DeathCrime = DA_Crime_Murder`.
- `DA_PopulationConfig` (`UDTPopulationConfig`): pedestrian + traffic vehicle classes, counts.

## 7. Wanted & Police
- `DA_Crime_*` (`UDTCrimeData`): see GDD §3 for suggested values.
- `BP_PoliceOfficer` (parent `ADTPoliceCharacter`): mesh, `StartingWeapons = [BP_Weapon_Pistol]`, `DeathCrime = DA_Crime_CopKill`, `UDTMapIconComponent` (Police).
- `DA_WantedConfig` (`UDTWantedConfig`): defaults already match the GDD; set `PoliceOfficerClass` and `PoliceVehicleClass`.

## 8. Game Mode & HUD
- `BP_GameMode` (parent `ADTGameMode`): Default Pawn `BP_PlayerCharacter`, Player Controller `BP_PlayerController`, HUD `BP_HUD`,
  `WantedConfig`, `PopulationConfig`, `MinimapWorldBounds`.
- `BP_HUD` (parent `ADTHUD`): `HUDWidgetClass = WBP_HUD`.
- `WBP_HUD` (parent `UDTHUDWidget`): implement the `On...Updated` events; place `WBP_Minimap` in a corner.

## 9. Minimap
1. Place a `SceneCaptureComponent2D` above the city: Orthographic, Ortho Width = city size, pointing straight down, yaw 0 (world +X = map up).
   Capture to a render target, then right-click it → **Create Static Texture** → `T_MinimapTexture`.
2. Set `BP_GameMode.MinimapWorldBounds` to the captured XY area.
3. `M_MinimapMask` (User Interface domain): sample `T_MinimapTexture` with UVs = rotate((UV − 0.5) × Zoom, Rotation) + (CenterU, CenterV), multiply by a circular mask.
4. `WBP_Minimap` (parent `UDTMinimapWidget`): an Image named **MapImage** using `M_MinimapMask`; a Canvas with icons positioned from `GetIconViews()` (offset from the centre);
   draw the search circle from `GetSearchArea()`.
5. Add `UDTMapIconComponent` (Player, rotate with owner) to `BP_PlayerCharacter`.

## 10. Thailand map tools
See `Docs/ThailandMap.md` (Thai) for the full province / road / terrain / coast workflow with `DTGeoImporter`.

## 11. World map (M) + saved progress
1. `IA_Map` (Digital) and a new context `IMC_Global`: M -> IA_Map (enable *Trigger when Paused* on IA_Map if you turn on bPauseWhileMapOpen).
2. `BP_PlayerController`: GlobalContext = `IMC_Global`, MapAction = `IA_Map`, WorldMapClass = `WBP_WorldMap`.
3. `WBP_WorldMap` (parent `DTWorldMapWidget`): full-screen; set **LabelFont** to a Thai font (Noto Sans Thai / Sarabun);
   optional BackgroundBrush = the country minimap texture from `build_provinces.py --minimap` and BackgroundWorldBounds = the
   `minimap` bounds in `map_bounds.json`. Implement `OnHoveredProvinceChanged` for a tooltip; use
   `DTProvinceSubsystem.GetVisitedCountInRegion` for a per-region progress panel.
4. Saving needs no setup: `DTProgressSubsystem` loads/saves slot `Downtown_Progress` automatically
   (`dt.Save.Info`, `dt.Save.Now`, `dt.Save.Reset` in the console).
