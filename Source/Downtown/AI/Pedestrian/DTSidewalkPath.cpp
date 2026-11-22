#include "AI/Pedestrian/DTSidewalkPath.h"
#include "Components/SplineComponent.h"

ADTSidewalkPath::ADTSidewalkPath()
{
	PrimaryActorTick.bCanEverTick = false;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetDrawDebug(true);
	RootComponent = Spline;
#if WITH_EDITORONLY_DATA
	Spline->EditorUnselectedSplineSegmentColor = FLinearColor(0.2f, 1.f, 0.3f);
#endif
}

ADTSidewalkPath* ADTSidewalkPath::PickConnectedPath(bool bAtEnd, bool& bOutStartFromEnd) const
{
	const FVector Here = Spline->GetLocationAtDistanceAlongSpline(bAtEnd ? Spline->GetSplineLength() : 0.f, ESplineCoordinateSpace::World);

	TArray<TPair<ADTSidewalkPath*, bool>> Options;
	for (const TObjectPtr<ADTSidewalkPath>& Path : ConnectedPaths)
	{
		if (!Path || Path->bBlocked) continue;
		const USplineComponent* Other = Path->GetSpline();
		const FVector OtherStart = Other->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World);
		const FVector OtherEnd = Other->GetLocationAtDistanceAlongSpline(Other->GetSplineLength(), ESplineCoordinateSpace::World);
		if (FVector::Dist(Here, OtherStart) < ConnectionTolerance) Options.Emplace(Path, false);
		else if (FVector::Dist(Here, OtherEnd) < ConnectionTolerance) Options.Emplace(Path, true);
	}

	if (Options.Num() == 0)
	{
		return nullptr;
	}
	const TPair<ADTSidewalkPath*, bool>& Choice = Options[FMath::RandRange(0, Options.Num() - 1)];
	bOutStartFromEnd = Choice.Value;
	return Choice.Key;
}
