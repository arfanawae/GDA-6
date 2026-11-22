#include "AI/Police/DTPoliceDispatchSubsystem.h"
#include "AI/Traffic/DTTrafficLane.h"
#include "Wanted/DTWantedSubsystem.h"
#include "Wanted/DTWantedConfig.h"
#include "Character/DTCharacterBase.h"
#include "Vehicle/DTVehicleBase.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Camera/PlayerCameraManager.h"

bool UDTPoliceDispatchSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDTPoliceDispatchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UDTWantedSubsystem* Wanted = InWorld.GetSubsystem<UDTWantedSubsystem>())
	{
		Wanted->OnWantedLevelChanged.AddDynamic(this, &UDTPoliceDispatchSubsystem::HandleWantedLevelChanged);
	}
}

void UDTPoliceDispatchSubsystem::HandleWantedLevelChanged(int32 NewLevel, int32 OldLevel)
{
	DispatchTimer = DispatchInterval; // dispatch on next tick
}

bool UDTPoliceDispatchSubsystem::FindSpawnPoint(FVector& OutLocation, bool bOnRoad) const
{
	const UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>();
	const UDTWantedConfig* Config = Wanted ? Wanted->GetConfig() : nullptr;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!Config || !Camera)
	{
		return false;
	}

	const FVector Center = Wanted->GetLastKnownLocation();
	const FVector ViewLoc = Camera->GetCameraLocation();
	const FVector ViewDir = Camera->GetCameraRotation().Vector();

	auto IsHidden = [&](const FVector& P)
	{
		const FVector ToP = P - ViewLoc;
		return FVector::DotProduct(ToP.GetSafeNormal(), ViewDir) < 0.3f || ToP.Size() > Config->SpawnDistanceMax * 0.8f;
	};

	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		FVector Candidate;
		if (bOnRoad)
		{
			// Pick a point on a random traffic lane in the spawn ring.
			TArray<ADTTrafficLane*> Lanes;
			for (TActorIterator<ADTTrafficLane> It(GetWorld()); It; ++It) Lanes.Add(*It);
			if (Lanes.Num() == 0) return false;
			const ADTTrafficLane* Lane = Lanes[FMath::RandRange(0, Lanes.Num() - 1)];
			const USplineComponent* Spline = Lane->GetSpline();
			Candidate = Spline->GetLocationAtDistanceAlongSpline(FMath::FRandRange(0.f, Spline->GetSplineLength()), ESplineCoordinateSpace::World);
		}
		else
		{
			const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld());
			FNavLocation NavLoc;
			if (!Nav || !Nav->GetRandomReachablePointInRadius(Center, Config->SpawnDistanceMax, NavLoc)) continue;
			Candidate = NavLoc.Location;
		}

		const float Dist = FVector::Dist2D(Candidate, Center);
		if (Dist >= Config->SpawnDistanceMin && Dist <= Config->SpawnDistanceMax && IsHidden(Candidate))
		{
			OutLocation = Candidate;
			return true;
		}
	}
	return false;
}

void UDTPoliceDispatchSubsystem::SpawnOfficer()
{
	const UDTWantedConfig* Config = GetWorld()->GetSubsystem<UDTWantedSubsystem>()->GetConfig();
	FVector Location;
	if (!Config->PoliceOfficerClass || !FindSpawnPoint(Location, false))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	if (ADTCharacterBase* Officer = GetWorld()->SpawnActor<ADTCharacterBase>(Config->PoliceOfficerClass, Location + FVector(0, 0, 100), FRotator::ZeroRotator, Params))
	{
		Officer->SpawnDefaultController();
		Officers.Add(Officer);
	}
}

void UDTPoliceDispatchSubsystem::SpawnCruiser()
{
	const UDTWantedConfig* Config = GetWorld()->GetSubsystem<UDTWantedSubsystem>()->GetConfig();
	FVector Location;
	if (!Config->PoliceVehicleClass || !FindSpawnPoint(Location, true))
	{
		return;
	}
	const FRotator Facing = (GetWorld()->GetSubsystem<UDTWantedSubsystem>()->GetLastKnownLocation() - Location).Rotation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	if (ADTVehicleBase* Cruiser = GetWorld()->SpawnActor<ADTVehicleBase>(Config->PoliceVehicleClass, Location + FVector(0, 0, 50), FRotator(0, Facing.Yaw, 0), Params))
	{
		Cruiser->SetHasAIDriver(true);
		Cruiser->SpawnDefaultController(); // AIControllerClass = ADTPoliceVehicleAIController in BP
		Cruisers.Add(Cruiser);
	}
}

void UDTPoliceDispatchSubsystem::Cleanup(bool bForceAll)
{
	Officers.RemoveAll([](const TWeakObjectPtr<ADTCharacterBase>& P) { return !P.IsValid() || P->IsDead(); });
	Cruisers.RemoveAll([](const TWeakObjectPtr<ADTVehicleBase>& P) { return !P.IsValid(); });

	if (!bForceAll)
	{
		return;
	}

	const UDTWantedConfig* Config = GetWorld()->GetSubsystem<UDTWantedSubsystem>()->GetConfig();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	auto FarAway = [&](const AActor* A) { return !Player || FVector::Dist(A->GetActorLocation(), Player->GetActorLocation()) > Config->DespawnDistance; };

	for (int32 i = Officers.Num() - 1; i >= 0; --i)
	{
		if (FarAway(Officers[i].Get())) { Officers[i]->Destroy(); Officers.RemoveAt(i); }
	}
	for (int32 i = Cruisers.Num() - 1; i >= 0; --i)
	{
		if (FarAway(Cruisers[i].Get()) && !Cruisers[i]->IsPlayerControlled()) { Cruisers[i]->Destroy(); Cruisers.RemoveAt(i); }
	}
}

void UDTPoliceDispatchSubsystem::Tick(float DeltaTime)
{
	DispatchTimer += DeltaTime;
	if (DispatchTimer < DispatchInterval)
	{
		return;
	}
	DispatchTimer = 0.f;

	const UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>();
	if (!Wanted || !Wanted->GetConfig())
	{
		return;
	}

	const int32 Level = Wanted->GetWantedLevel();
	Cleanup(Level == 0);
	if (Level == 0)
	{
		return;
	}

	// Top up one unit per interval so reinforcements trickle in rather than popping in at once.
	const FDTWantedLevelRule& Rule = Wanted->GetConfig()->Levels[Level - 1];
	if (Officers.Num() < Rule.OfficersOnFoot)
	{
		SpawnOfficer();
	}
	if (Cruisers.Num() < Rule.PoliceVehicles)
	{
		SpawnCruiser();
	}
}
