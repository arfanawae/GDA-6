#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DTGameMode.generated.h"

class UDTWantedConfig;
class UDTPopulationConfig;

/** Holds world-level config the subsystems read on BeginPlay. BP_GameMode derives from this. */
UCLASS()
class DOWNTOWN_API ADTGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADTGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Downtown")
	TObjectPtr<UDTWantedConfig> WantedConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Downtown")
	TObjectPtr<UDTPopulationConfig> PopulationConfig;

	/** World-space XY bounds covered by T_MinimapTexture (match the top-down capture). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FBox2D MinimapWorldBounds = FBox2D(FVector2D(-50000.f, -50000.f), FVector2D(50000.f, 50000.f));
};
