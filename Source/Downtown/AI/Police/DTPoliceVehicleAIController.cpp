#include "AI/Police/DTPoliceVehicleAIController.h"
#include "AI/Police/DTPoliceDispatchSubsystem.h"
#include "Vehicle/DTVehicleBase.h"
#include "Vehicle/DTVehicleSeatComponent.h"
#include "Wanted/DTWantedSubsystem.h"
#include "Wanted/DTWantedConfig.h"
#include "Character/DTCharacterBase.h"
#include "Kismet/GameplayStatics.h"

ADTPoliceVehicleAIController::ADTPoliceVehicleAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
}

void ADTPoliceVehicleAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ADTVehicleBase* Car = Cast<ADTVehicleBase>(GetPawn());
	const UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>();
	if (!Car || !Wanted)
	{
		return;
	}

	if (Wanted->GetWantedLevel() == 0)
	{
		Car->StopDriving();
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const bool bSeen = Wanted->IsPlayerSeen() && PlayerPawn;
	const FVector Goal = bSeen ? PlayerPawn->GetActorLocation() : Wanted->GetLastKnownLocation();
	const float Dist = FVector::Dist(Car->GetActorLocation(), Goal);
	const bool bPlayerOnFoot = PlayerPawn && !PlayerPawn->IsA<ADTVehicleBase>();

	if (bPlayerOnFoot && Dist < DeployDistance)
	{
		Car->StopDriving();
		if (!bOfficersDeployed && FMath::Abs(Car->GetSpeedKmh()) < 5.f)
		{
			DeployOfficers();
		}
		return;
	}

	float Speed = bSeen ? PursuitSpeedKmh : SearchSpeedKmh;
	const bool bRam = Wanted->GetWantedLevel() >= RamAtStars && !bPlayerOnFoot;
	if (!bRam && !bPlayerOnFoot && Dist < FollowGap)
	{
		Speed = 0.f;
	}
	else if (!bSeen && Dist < 800.f)
	{
		Speed = 0.f; // reached last known location
	}

	// Lead the target slightly when it is moving.
	const FVector Lead = bSeen && PlayerPawn ? PlayerPawn->GetVelocity() * FMath::Clamp(Dist / 3000.f, 0.f, 1.f) : FVector::ZeroVector;
	Car->DriveTowards(Goal + Lead, Speed);
}

void ADTPoliceVehicleAIController::DeployOfficers()
{
	bOfficersDeployed = true;

	ADTVehicleBase* Car = Cast<ADTVehicleBase>(GetPawn());
	const UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>();
	const UDTWantedConfig* Config = Wanted ? Wanted->GetConfig() : nullptr;
	UDTPoliceDispatchSubsystem* Dispatch = GetWorld()->GetSubsystem<UDTPoliceDispatchSubsystem>();
	if (!Car || !Config || !Config->PoliceOfficerClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (int32 i = 0; i < OfficersPerCar; ++i)
	{
		const float Side = (i % 2 == 0) ? -1.f : 1.f;
		const FVector Location = Car->GetActorLocation() + Car->GetActorRightVector() * Side * 200.f + FVector(0, 0, 100.f);
		if (ADTCharacterBase* Officer = GetWorld()->SpawnActor<ADTCharacterBase>(Config->PoliceOfficerClass, Location, Car->GetActorRotation(), Params))
		{
			if (!Officer->GetController())
			{
				Officer->SpawnDefaultController();
			}
			if (Dispatch)
			{
				Dispatch->RegisterOfficer(Officer);
			}
		}
	}
}
