#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTGeoRoad.generated.h"

class USplineComponent;
class UStaticMesh;

/**
 * Visual road piece built from OSM by ADTGeoImporter: a spline (always) plus optional
 * spline meshes. Kept short (MaxSegmentLength) so World Partition can stream them per cell.
 * Swap RoadMesh for a proper road kit, or replace with Landscape Splines / PCG later.
 */
UCLASS()
class DOWNTOWN_API ADTGeoRoad : public AActor
{
	GENERATED_BODY()

public:
	ADTGeoRoad();

	void Build(const TArray<FVector>& WorldPoints, float InWidthCm, UStaticMesh* InMesh);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road") FString RoadName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road") FString Highway;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road") int32 Lanes = 2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road") bool bOneWay = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road") float WidthCm = 700.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;
};
