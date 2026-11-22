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
#endif
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
