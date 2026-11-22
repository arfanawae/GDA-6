#include "AI/Police/DTPoliceAIController.h"
#include "AI/Police/DTPoliceCharacter.h"
#include "Combat/DTWeaponComponent.h"
#include "Wanted/DTWantedSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ADTPoliceAIController::ADTPoliceAIController()
{
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	SightConfig->SightRadius = 4000.f;
	SightConfig->LoseSightRadius = 4500.f;
	SightConfig->PeripheralVisionAngleDegrees = 80.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(3.f);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	HearingConfig->HearingRange = 5000.f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
}

void ADTPoliceAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	PerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ADTPoliceAIController::HandlePerception);
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ADTPoliceAIController::Think, ThinkInterval, true, FMath::FRand() * ThinkInterval);
}

void ADTPoliceAIController::OnUnPossess()
{
	SetFiring(false);
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::OnUnPossess();
}

bool ADTPoliceAIController::IsPlayerPawn(const AActor* Actor) const
{
	const APawn* CandidatePawn = Cast<APawn>(Actor);
	return CandidatePawn && CandidatePawn->IsPlayerControlled(); // the player's character OR the vehicle they drive
}

void ADTPoliceAIController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>();
	if (!Wanted)
	{
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>() && IsPlayerPawn(Actor))
	{
		bTargetVisible = Stimulus.WasSuccessfullySensed();
		Target = Actor;
		if (bTargetVisible)
		{
			Wanted->ReportPlayerSighted(Actor->GetActorLocation());
		}
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed() && Wanted->GetWantedLevel() > 0)
	{
		// Gunfire gives away the player's position.
		if (IsPlayerPawn(Actor))
		{
			Wanted->ReportPlayerSighted(Stimulus.StimulusLocation);
		}
	}
}

void ADTPoliceAIController::SetFiring(bool bFire)
{
	if (bFire == bFiring)
	{
		return;
	}
	bFiring = bFire;
	if (const ADTPoliceCharacter* Officer = Cast<ADTPoliceCharacter>(GetPawn()))
	{
		bFire ? Officer->GetWeaponComponent()->StartFire() : Officer->GetWeaponComponent()->StopFire();
	}
}

void ADTPoliceAIController::Think()
{
	const ADTPoliceCharacter* Officer = Cast<ADTPoliceCharacter>(GetPawn());
	UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>();
	if (!Officer || Officer->IsDead() || !Wanted)
	{
		return;
	}

	if (Wanted->GetWantedLevel() == 0)
	{
		State = EDTPoliceState::Idle;
		SetFiring(false);
		ClearFocus(EAIFocusPriority::Gameplay);
		StopMovement();
		return;
	}

	// The player may have switched pawn (entered/exited a vehicle) - always chase the current one.
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Target.Get() != PlayerPawn)
	{
		Target = PlayerPawn;
		bTargetVisible = PlayerPawn && LineOfSightTo(PlayerPawn);
	}

	if (bTargetVisible && Target.IsValid())
	{
		Wanted->ReportPlayerSighted(Target->GetActorLocation());
		const float Dist = FVector::Dist(Officer->GetActorLocation(), Target->GetActorLocation());

		if (Dist <= AttackRange && Wanted->CanUseLethalForce())
		{
			State = EDTPoliceState::Attack;
			SetFocus(Target.Get());
			StopMovement();

			BurstClock += ThinkInterval;
			const float Cycle = BurstTime + BurstPause;
			SetFiring(FMath::Fmod(BurstClock, Cycle) < BurstTime);
		}
		else
		{
			State = EDTPoliceState::Chase;
			SetFiring(false);
			SetFocus(Target.Get());
			MoveToActor(Target.Get(), ChaseAcceptance);
		}
		return;
	}

	// Lost visual: search around the last known location.
	SetFiring(false);
	ClearFocus(EAIFocusPriority::Gameplay);
	if (State != EDTPoliceState::Search || GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		State = EDTPoliceState::Search;
		FNavLocation Point;
		const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
		const float Radius = FMath::Min(Wanted->GetSearchRadius() * 0.5f, 3000.f);
		if (Nav && Nav->GetRandomReachablePointInRadius(Wanted->GetLastKnownLocation(), Radius, Point))
		{
			MoveToLocation(Point.Location, 150.f);
		}
	}
}
