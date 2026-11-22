#include "AI/Traffic/DTTrafficLane.h"
#include "Components/SplineComponent.h"

ADTTrafficLane::ADTTrafficLane()
{
	PrimaryActorTick.bCanEverTick = false;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetDrawDebug(true);
	RootComponent = Spline;
#if WITH_EDITORONLY_DATA
	Spline->EditorUnselectedSplineSegmentColor = FLinearColor(0.1f, 0.6f, 1.f);

	// Lanes hold hard references to their neighbours and are tiny (a spline), so keep them
	// always loaded instead of splitting the graph across World Partition cells.
	bIsSpatiallyLoaded = false;
#endif
}

void ADTTrafficLane::InitializeFromPoints(const TArray<FVector>& WorldPoints, float InSpeedLimitKmh)
{
	SpeedLimitKmh = InSpeedLimitKmh;
	if (WorldPoints.Num() > 0)
	{
		SetActorLocation(WorldPoints[0]);
	}
	Spline->ClearSplinePoints(false);
	for (const FVector& P : WorldPoints)
	{
		Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
	}
	// Linear segments: OSM polylines are already dense enough, curves would overshoot at junctions.
	for (int32 i = 0; i < WorldPoints.Num(); ++i)
	{
		Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
	}
	Spline->UpdateSpline();
}

ADTTrafficLane* ADTTrafficLane::PickNextLane() const
{
	TArray<ADTTrafficLane*> Valid;
	for (const TObjectPtr<ADTTrafficLane>& Lane : NextLanes)
	{
		if (Lane) Valid.Add(Lane);
	}
	return Valid.Num() > 0 ? Valid[FMath::RandRange(0, Valid.Num() - 1)] : nullptr;
}
