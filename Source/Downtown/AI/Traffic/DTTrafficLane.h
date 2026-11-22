#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTTrafficLane.generated.h"

class USplineComponent;
class ADTTrafficLight;

/**
 * One-way driving lane drawn as a spline. Connect lanes into a graph with NextLanes
 * (several entries = an intersection with a choice of turns).
 */
UCLASS()
class DOWNTOWN_API ADTTrafficLane : public AActor
{
	GENERATED_BODY()

public:
	ADTTrafficLane();

	UFUNCTION(BlueprintPure, Category = "Traffic")
	USplineComponent* GetSpline() const { return Spline; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	float GetSpeedLimitKmh() const { return SpeedLimitKmh; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	ADTTrafficLight* GetTrafficLight() const { return TrafficLight; }

	/** Random connected lane, or nullptr for a dead end. */
	ADTTrafficLane* PickNextLane() const;

	const TArray<TObjectPtr<ADTTrafficLane>>& GetNextLanes() const { return NextLanes; }

	/** Used by ADTGeoImporter to build lanes from OpenStreetMap data. */
	void InitializeFromPoints(const TArray<FVector>& WorldPoints, float InSpeedLimitKmh);
	void AddNextLane(ADTTrafficLane* Lane) { if (Lane && Lane != this) NextLanes.AddUnique(Lane); }
	void ClearNextLanes() { NextLanes.Reset(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(EditAnywhere, Category = "Traffic")
	float SpeedLimitKmh = 50.f;

	UPROPERTY(EditAnywhere, Category = "Traffic")
	TArray<TObjectPtr<ADTTrafficLane>> NextLanes;

	/** Light controlling the END of this lane (the stop line). */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	TObjectPtr<ADTTrafficLight> TrafficLight;
};
