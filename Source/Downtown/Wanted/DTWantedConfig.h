#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DTWantedConfig.generated.h"

class ADTCharacterBase;
class ADTVehicleBase;

USTRUCT(BlueprintType)
struct FDTWantedLevelRule
{
	GENERATED_BODY()

	/** Heat needed to reach this star level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float HeatThreshold = 1.f;

	/** Radius around the last known location police search in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float SearchRadius = 6000.f;

	/** Seconds outside the search radius and out of sight to lose the stars. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float EvadeSeconds = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 OfficersOnFoot = 2;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 PoliceVehicles = 0;

	/** Officers are allowed to shoot at this level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bUseLethalForce = false;
};

/** DA_WantedConfig: star thresholds, evasion rules and police classes. */
UCLASS(BlueprintType)
class DOWNTOWN_API UDTWantedConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UDTWantedConfig();

	/** Index 0 = 1 star, index 2 = 3 stars. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wanted")
	TArray<FDTWantedLevelRule> Levels;

	/** Pedestrians/police within this distance with line of sight witness a crime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wanted")
	float WitnessRadius = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dispatch")
	TSubclassOf<ADTCharacterBase> PoliceOfficerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dispatch")
	TSubclassOf<ADTVehicleBase> PoliceVehicleClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dispatch")
	float SpawnDistanceMin = 6000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dispatch")
	float SpawnDistanceMax = 12000.f;

	/** Police further than this from the player are removed once wanted level is 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dispatch")
	float DespawnDistance = 15000.f;

	int32 GetMaxStars() const { return Levels.Num(); }
};
