# Downtown — Open-World Action Prototype (UE5)

Indie-scale prototype of a third-person open-world action game built with **Unreal Engine 5 (C++ + Blueprint)**.

| Phase 1 system | Main classes |
|---|---|
| Third-person controller + shooting | `ADTPlayerCharacter`, `UDTWeaponComponent`, `ADTWeaponBase`, `UDTHealthComponent` |
| Vehicles + seamless enter/exit | `ADTVehicleBase`, `UDTVehicleEntryComponent`, `UDTVehicleSeatComponent` |
| Traffic & pedestrian AI | `ADTTrafficLane`, `ADTTrafficAIController`, `ADTSidewalkPath`, `ADTPedestrianAIController`, `UDTPopulationSubsystem` |
| Wanted level (1–3 stars) | `UDTWantedSubsystem`, `UDTPoliceDispatchSubsystem`, `ADTPoliceAIController`, `ADTPoliceVehicleAIController` |
| HUD & minimap | `ADTHUD`, `UDTHUDWidget`, `UDTMinimapWidget`, `UDTMinimapSubsystem`, `UDTMapIconComponent` |
| Thailand map (77 provinces, 1:20) | `UDTGeoLibrary`, `UDTGeoSettings`, `UDTProvinceSubsystem`, `ADTProvinceSign`, `ADTGeoImporter`, `ADTGeoRoad` |

## Documents
- [`Docs/GDD.md`](Docs/GDD.md) — Game Design Document + step-by-step roadmap (Thai)
- [`Docs/EditorSetup.md`](Docs/EditorSetup.md) — the Blueprint / asset checklist to wire the C++ into a playable level
- [`Docs/ThailandMap.md`](Docs/ThailandMap.md) — scaled 1:20 map of Thailand (77 provinces): projection, OSM pipeline, province checkpoints, test scene (Thai)
- [`Tools/GeoPipeline`](Tools/GeoPipeline) — Python scripts that turn real map data into game data

## Getting started
1. Install **Unreal Engine 5.5+** and Visual Studio 2022 (Windows) or Xcode/Rider.
2. Install **Git LFS** before adding assets: `git lfs install` (binary assets are tracked via `.gitattributes`).
3. Right-click `Downtown.uproject` → *Generate Visual Studio project files* (or *Switch Unreal Engine version* if you use a newer engine).
4. Build the `DowntownEditor` target, open the project, then follow `Docs/EditorSetup.md`.

> The C++ in this repo was written without an engine available to compile it, so expect to fix a few small errors on the first build.

## Layout
```
Config/                 Engine/Game/Input defaults (Enhanced Input enabled), geo projection settings
Tools/GeoPipeline/      Python: OSM/geoBoundaries -> province DataTable + road JSON
Content/_Downtown/      All project assets (see Docs/GDD.md §2 for naming)
Source/Downtown/
  Core/                 GameMode, PlayerController (input context swapping)
  Character/            Character base (health, ragdoll), player character
  Combat/               Health, weapon component, hitscan weapon, weapon data
  Interaction/          IDTInteractable + scanner component
  Vehicle/              Chaos vehicle pawn, seats, enter/exit state machine
  AI/Traffic/           Lane splines, traffic lights, lane-following driver
  AI/Pedestrian/        Sidewalk splines, pedestrian + flee behaviour
  AI/Police/            Officer, cruiser driver, dispatch subsystem
  Wanted/               Wanted subsystem, crime + config data assets
  World/                Ambient population spawner + config
    Geo/                Lat/lon projection, settings, OSM road importer
    Province/           77-province data types, detection subsystem, border signs
  UI/                   HUD, minimap, map icons
```

## Map data
Province and road data are derived from OpenStreetMap — © OpenStreetMap contributors, ODbL. See `Content/_Downtown/Data/ATTRIBUTION.md`.

## Architecture rules
- Gameplay logic in C++, tuning and asset wiring in Blueprint/Data Assets.
- Systems talk through **delegates**, not direct references (e.g. weapons broadcast `OnWeaponFired`; the wanted system decides what it means).
- World-wide singletons are `UWorldSubsystem`s; they read their config from `ADTGameMode`.
