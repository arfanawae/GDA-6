#include "AI/Pedestrian/DTPedestrianAIController.h"
#include "AI/Pedestrian/DTPedestrianCharacter.h"
#include "AI/Pedestrian/DTSidewalkPath.h"
#include "Character/DTCharacterBase.h"
#include "Combat/DTHealthComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADTPedestrianAIController::ADTPedestrianAIController()
{
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	SightConfig->SightRadius = 1500.f;
	SightConfig->LoseSightRadius = 1800.f;
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(2.f);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	HearingConfig->HearingRange = 3000.f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(1.f);

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
}

void ADTPedestrianAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	PerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ADTPedestrianAIController::HandlePerception);
	if (ADTCharacterBase* CharacterBase = Cast<ADTCharacterBase>(InPawn))
	{
		CharacterBase->GetHealthComponent()->OnHealthChanged.AddUniqueDynamic(this, &ADTPedestrianAIController::HandleDamaged);
	}

	if (ADTPedestrianCharacter* Ped = Cast<ADTPedestrianCharacter>(InPawn); Ped && Ped->GetStartPath())
	{
		CurrentPath = Ped->GetStartPath();
		PathDistance = Ped->GetStartDistance();
		bForward = FMath::RandBool();
	}
	else
	{
		SnapToNearestPath();
	}

	SetSpeed(false);
	MoveToNextWaypoint();
}

void ADTPedestrianAIController::SetSpeed(bool bRun)
{
	if (const ADTPedestrianCharacter* Ped = Cast<ADTPedestrianCharacter>(GetPawn()))
	{
		Ped->GetCharacterMovement()->MaxWalkSpeed = bRun ? Ped->RunSpeed : Ped->WalkSpeed;
	}
}

void ADTPedestrianAIController::SnapToNearestPath()
{
	if (!GetPawn()) return;
	const FVector Here = GetPawn()->GetActorLocation();
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ADTSidewalkPath> It(GetWorld()); It; ++It)
	{
		const USplineComponent* Spline = It->GetSpline();
		const float Key = Spline->FindInputKeyClosestToWorldLocation(Here);
		const float DistSq = FVector::DistSquared(Here, Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World));
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			CurrentPath = *It;
			PathDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
		}
	}
	bForward = FMath::RandBool();
}

void ADTPedestrianAIController::MoveToNextWaypoint()
{
	if (State != EDTPedestrianState::Wander || !CurrentPath)
	{
		return;
	}

	const USplineComponent* Spline = CurrentPath->GetSpline();
	const float Length = Spline->GetSplineLength();
	PathDistance += bForward ? WaypointSpacing : -WaypointSpacing;

	// Reached an end: hop to a connected path or turn around.
	if (PathDistance > Length || PathDistance < 0.f)
	{
		bool bStartFromEnd = false;
		if (ADTSidewalkPath* Next = CurrentPath->PickConnectedPath(PathDistance > Length, bStartFromEnd))
		{
			CurrentPath = Next;
			bForward = !bStartFromEnd;
			PathDistance = bStartFromEnd ? Next->GetSpline()->GetSplineLength() : 0.f;
		}
		else
		{
			bForward = !bForward;
			PathDistance = FMath::Clamp(PathDistance, 0.f, Length);
		}
	}

	// Small sideways offset so pedestrians do not walk in a single file.
	const USplineComponent* PathSpline = CurrentPath->GetSpline();
	const FVector Point = PathSpline->GetLocationAtDistanceAlongSpline(PathDistance, ESplineCoordinateSpace::World)
		+ PathSpline->GetRightVectorAtDistanceAlongSpline(PathDistance, ESplineCoordinateSpace::World) * FMath::FRandRange(-80.f, 80.f);

	MoveToLocation(Point, 50.f, false, true, true, false);
}

void ADTPedestrianAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	switch (State)
	{
	case EDTPedestrianState::Wander:
		MoveToNextWaypoint();
		break;
	case EDTPedestrianState::Flee:
		// Keep running while scared, from wherever we ended up.
		StartFlee();
		break;
	default:
		break;
	}
}

void ADTPedestrianAIController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.Tag == TEXT("Gunshot"))
	{
		Panic(Stimulus.StimulusLocation);
	}
	else if (const ADTCharacterBase* Seen = Cast<ADTCharacterBase>(Actor); Seen && Seen->IsDead())
	{
		Panic(Seen->GetActorLocation());
	}
}

void ADTPedestrianAIController::HandleDamaged(UDTHealthComponent* Health, float NewHealth, float Delta)
{
	if (Delta < 0.f && GetPawn())
	{
		Panic(GetPawn()->GetActorLocation() - GetPawn()->GetActorForwardVector() * 100.f);
	}
}

void ADTPedestrianAIController::Panic(FVector InThreatLocation)
{
	const ADTCharacterBase* CharacterBase = Cast<ADTCharacterBase>(GetPawn());
	if (!CharacterBase || CharacterBase->IsDead())
	{
		return;
	}
	ThreatLocation = InThreatLocation;
	GetWorldTimerManager().SetTimer(CalmTimer, this, &ADTPedestrianAIController::EndFlee, CalmDownTime, false);

	if (State != EDTPedestrianState::Flee)
	{
		State = EDTPedestrianState::Flee;
		SetSpeed(true);
		StartFlee();
	}
}

void ADTPedestrianAIController::StartFlee()
{
	const APawn* MyPawn = GetPawn();
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!MyPawn || !Nav)
	{
		return;
	}

	// Sample a few reachable points and keep the one furthest from the threat (cheap EQS substitute).
	const FVector Here = MyPawn->GetActorLocation();
	FVector Best = Here;
	float BestScore = -1.f;
	for (int32 i = 0; i < 8; ++i)
	{
		FNavLocation Candidate;
		if (Nav->GetRandomReachablePointInRadius(Here, FleeDistance, Candidate))
		{
			const float Score = FVector::Dist2D(Candidate.Location, ThreatLocation);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate.Location;
			}
		}
	}

	if (BestScore < FVector::Dist2D(Here, ThreatLocation))
	{
		// Nowhere better to go: cower in place.
		State = EDTPedestrianState::Cower;
		StopMovement();
		return;
	}
	MoveToLocation(Best, 100.f, false, true, false, false);
}

void ADTPedestrianAIController::EndFlee()
{
	State = EDTPedestrianState::Wander;
	SetSpeed(false);
	SnapToNearestPath();
	MoveToNextWaypoint();
}
