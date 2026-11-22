#include "AI/Traffic/DTTrafficAIController.h"
#include "AI/Traffic/DTTrafficLane.h"
#include "AI/Traffic/DTTrafficLight.h"
#include "Vehicle/DTVehicleBase.h"
#include "Vehicle/DTVehicleData.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ADTTrafficAIController::ADTTrafficAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f; // 20 Hz is plenty for traffic
}

void ADTTrafficAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (ADTVehicleBase* Vehicle = Cast<ADTVehicleBase>(InPawn))
	{
		Vehicle->SetHasAIDriver(true);
		if (!CurrentLane)
		{
			float Dist = 0.f;
			SetLane(FindNearestLane(InPawn->GetActorLocation(), Dist), Dist);
		}
	}
}

void ADTTrafficAIController::SetLane(ADTTrafficLane* InLane, float InDistance)
{
	CurrentLane = InLane;
	DistanceOnLane = InDistance;
}

ADTTrafficLane* ADTTrafficAIController::FindNearestLane(const FVector& Location, float& OutDistance) const
{
	ADTTrafficLane* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ADTTrafficLane> It(GetWorld()); It; ++It)
	{
		const USplineComponent* Spline = It->GetSpline();
		const float Key = Spline->FindInputKeyClosestToWorldLocation(Location);
		const FVector Point = Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
		const float DistSq = FVector::DistSquared(Point, Location);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
			OutDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
		}
	}
	return Best;
}

float ADTTrafficAIController::GetObstacleDistance(const ADTVehicleBase* Vehicle) const
{
	const FVector Forward = Vehicle->GetActorForwardVector();
	const FVector Start = Vehicle->GetActorLocation() + Forward * 300.f + FVector(0, 0, 80.f);
	const FVector End = Start + Forward * ObstacleScanDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTTrafficScan), false, Vehicle);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);

	FHitResult Hit;
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat(Vehicle->GetActorRotation()), Objects, FCollisionShape::MakeBox(FVector(50.f, 90.f, 60.f)), Params))
	{
		return Hit.Distance;
	}
	return TNumericLimits<float>::Max();
}

void ADTTrafficAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ADTVehicleBase* Vehicle = Cast<ADTVehicleBase>(GetPawn());
	if (!Vehicle || !CurrentLane)
	{
		if (Vehicle) Vehicle->StopDriving();
		return;
	}

	const USplineComponent* Spline = CurrentLane->GetSpline();
	const float Speed = Vehicle->GetSpeedKmh();

	// Track progress along the lane.
	DistanceOnLane = Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(Vehicle->GetActorLocation()));
	const float LaneLength = Spline->GetSplineLength();
	const float Remaining = LaneLength - DistanceOnLane;

	const ADTTrafficLight* Light = CurrentLane->GetTrafficLight();
	const bool bMustStop = Light && !Light->CanPass();

	if (Remaining < 200.f && !bMustStop)
	{
		if (ADTTrafficLane* Next = CurrentLane->PickNextLane())
		{
			SetLane(Next, 0.f);
			return;
		}
	}

	// Pure pursuit: aim at a point ahead on the spline (spills into the next lane at the end).
	const float LookAhead = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 100.f), FVector2D(MinLookAhead, MaxLookAhead), Speed);
	FVector Target;
	if (DistanceOnLane + LookAhead <= LaneLength || CurrentLane->GetNextLanes().Num() == 0)
	{
		Target = Spline->GetLocationAtDistanceAlongSpline(FMath::Min(DistanceOnLane + LookAhead, LaneLength), ESplineCoordinateSpace::World);
	}
	else
	{
		const ADTTrafficLane* Next = CurrentLane->GetNextLanes()[0];
		Target = Next ? Next->GetSpline()->GetLocationAtDistanceAlongSpline(DistanceOnLane + LookAhead - LaneLength, ESplineCoordinateSpace::World)
		              : Spline->GetLocationAtDistanceAlongSpline(LaneLength, ESplineCoordinateSpace::World);
	}

	// Desired speed: lane limit, reduced for obstacles, stop for red lights and dead ends.
	float Desired = CurrentLane->GetSpeedLimitKmh();
	if (const UDTVehicleData* Data = Vehicle->GetVehicleData(); Data && Desired <= 0.f)
	{
		Desired = Data->DefaultCruiseSpeedKmh;
	}

	const float Obstacle = GetObstacleDistance(Vehicle);
	if (Obstacle < ObstacleScanDistance)
	{
		Desired = FMath::Min(Desired, FMath::Max(0.f, (Obstacle - SafeGap) / 30.f)); // ~0 km/h at SafeGap
	}

	const float StopDistance = Remaining - StopLineOffset;
	if ((bMustStop || CurrentLane->GetNextLanes().Num() == 0) && StopDistance < 2500.f)
	{
		Desired = FMath::Min(Desired, FMath::Max(0.f, StopDistance / 40.f));
	}

	Vehicle->DriveTowards(Target, Desired);
}
