#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DTTrafficAIController.generated.h"

class ADTTrafficLane;
class ADTVehicleBase;

/**
 * Drives an ADTVehicleBase along the lane graph: pure-pursuit steering on the spline,
 * speed limit, brake for obstacles and red lights.
 */
UCLASS()
class DOWNTOWN_API ADTTrafficAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADTTrafficAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** Places the controller on a lane. If not called, the nearest lane is used on possess. */
	void SetLane(ADTTrafficLane* InLane, float InDistance);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	ADTTrafficLane* FindNearestLane(const FVector& Location, float& OutDistance) const;
	float GetObstacleDistance(const ADTVehicleBase* Vehicle) const;

	/** How far ahead on the spline to aim, at standstill and at 100 km/h. */
	UPROPERTY(EditDefaultsOnly, Category = "Traffic") float MinLookAhead = 600.f;
	UPROPERTY(EditDefaultsOnly, Category = "Traffic") float MaxLookAhead = 1800.f;

	/** Distance from the lane end at which cars stop for a red light. */
	UPROPERTY(EditDefaultsOnly, Category = "Traffic") float StopLineOffset = 300.f;

	/** Keep at least this gap to the obstacle ahead. */
	UPROPERTY(EditDefaultsOnly, Category = "Traffic") float SafeGap = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traffic") float ObstacleScanDistance = 2500.f;

	UPROPERTY(Transient) TObjectPtr<ADTTrafficLane> CurrentLane;
	float DistanceOnLane = 0.f;
};
