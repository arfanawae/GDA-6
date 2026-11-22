#include "World/Geo/DTGeoCoastline.h"
#include "Components/SplineComponent.h"

ADTGeoCoastline::ADTGeoCoastline()
{
	PrimaryActorTick.bCanEverTick = false;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
#if WITH_EDITORONLY_DATA
	Spline->EditorUnselectedSplineSegmentColor = FLinearColor(0.1f, 0.8f, 1.f);
#endif
}

void ADTGeoCoastline::Build(const TArray<FVector>& WorldPoints, bool bInClosedLoop)
{
	if (WorldPoints.Num() < 2)
	{
		return;
	}
	SetActorLocation(WorldPoints[0]);
	Spline->ClearSplinePoints(false);
	for (int32 i = 0; i < WorldPoints.Num(); ++i)
	{
		Spline->AddSplinePoint(WorldPoints[i], ESplineCoordinateSpace::World, false);
		Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
	}
	bClosedLoop = bInClosedLoop;
	Spline->SetClosedLoop(bClosedLoop, false);
	Spline->UpdateSpline();
	LengthCm = Spline->GetSplineLength();
}
