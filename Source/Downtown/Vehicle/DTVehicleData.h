#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DTVehicleData.generated.h"

/** Gameplay values for a vehicle model. Physics (torque curve, wheels, suspension) stays on the Chaos movement component in BP. */
UCLASS(BlueprintType)
class DOWNTOWN_API UDTVehicleData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float MaxHealth = 1000.f;

	/** Above this speed (km/h) exiting becomes a bail-out jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float SafeExitSpeedKmh = 15.f;

	/** Cruise speed for AI traffic when the lane has no limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	float DefaultCruiseSpeedKmh = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	float MaxSteerAngleDegrees = 40.f;
};
