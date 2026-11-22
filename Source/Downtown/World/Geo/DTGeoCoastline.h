#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTGeoCoastline.generated.h"

class USplineComponent;

/**
 * A piece of coastline at sea level, spawned by ADTGeoImporter from Coast_<name>.json
 * (Tools/GeoPipeline/build_heightmap.py --sea). Use it as a guide for Water Body Ocean shore splines,
 * beach PCG graphs or foam decals. Islands that fit in one piece are closed loops.
 */
UCLASS()
class DOWNTOWN_API ADTGeoCoastline : public AActor
{
	GENERATED_BODY()

public:
	ADTGeoCoastline();

	void Build(const TArray<FVector>& WorldPoints, bool bInClosedLoop);

	UFUNCTION(BlueprintPure, Category = "Coast")
	USplineComponent* GetSpline() const { return Spline; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coast")
	bool bClosedLoop = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coast")
	float LengthCm = 0.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;
};
