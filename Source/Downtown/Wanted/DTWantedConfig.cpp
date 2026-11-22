#include "Wanted/DTWantedConfig.h"

UDTWantedConfig::UDTWantedConfig()
{
	// Defaults match the GDD table (Docs/GDD.md, Wanted Level section).
	FDTWantedLevelRule One;   One.HeatThreshold = 1.f;   One.SearchRadius = 6000.f;   One.EvadeSeconds = 10.f; One.OfficersOnFoot = 2; One.PoliceVehicles = 0; One.bUseLethalForce = false;
	FDTWantedLevelRule Two;   Two.HeatThreshold = 50.f;  Two.SearchRadius = 12000.f;  Two.EvadeSeconds = 20.f; Two.OfficersOnFoot = 2; Two.PoliceVehicles = 2; Two.bUseLethalForce = true;
	FDTWantedLevelRule Three; Three.HeatThreshold = 150.f; Three.SearchRadius = 20000.f; Three.EvadeSeconds = 30.f; Three.OfficersOnFoot = 4; Three.PoliceVehicles = 4; Three.bUseLethalForce = true;
	Levels = { One, Two, Three };
}
