#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DTPopulationConfig.generated.h"

class ADTPedestrianCharacter;
class ADTVehicleBase;

/** DA_PopulationConfig: how many ambient pedestrians and cars live around the player. */
UCLASS(BlueprintType)
class DOWNTOWN_API UDTPopulationConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Pedestrians") TArray<TSubclassOf<ADTPedestrianCharacter>> PedestrianClasses;
	UPROPERTY(EditDefaultsOnly, Category = "Pedestrians") int32 MaxPedestrians = 40;

	UPROPERTY(EditDefaultsOnly, Category = "Traffic") TArray<TSubclassOf<ADTVehicleBase>> TrafficVehicleClasses;
	UPROPERTY(EditDefaultsOnly, Category = "Traffic") int32 MaxVehicles = 20;

	/** Spawn ring around the player (cm). Spawns are also kept outside the camera view. */
	UPROPERTY(EditDefaultsOnly, Category = "Spawning") float SpawnRadiusMin = 5000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Spawning") float SpawnRadiusMax = 15000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Spawning") float DespawnRadius = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spawning") float UpdateInterval = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "Spawning") int32 MaxSpawnsPerUpdate = 3;
};
