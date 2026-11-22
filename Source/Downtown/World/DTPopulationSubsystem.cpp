#include "World/DTPopulationSubsystem.h"
#include "World/DTPopulationConfig.h"
#include "AI/Pedestrian/DTSidewalkPath.h"
#include "AI/Pedestrian/DTPedestrianCharacter.h"
#include "AI/Traffic/DTTrafficLane.h"
#include "AI/Traffic/DTTrafficAIController.h"
#include "Vehicle/DTVehicleBase.h"
#include "Vehicle/DTVehicleSeatComponent.h"
#include "Core/DTGameMode.h"
#include "Downtown.h"
#include "Components/SplineComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

bool UDTPopulationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDTPopulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (const ADTGameMode* GM = InWorld.GetAuthGameMode<ADTGameMode>())
	{
		Config = GM->PopulationConfig;
	}
	if (!Config)
	{
		UE_LOG(LogDowntown, Warning, TEXT("No PopulationConfig on the GameMode; ambient population disabled."));
		return;
	}

	for (TActorIterator<ADTSidewalkPath> It(&InWorld); It; ++It) SidewalkPaths.Add(*It);
	for (TActorIterator<ADTTrafficLane> It(&InWorld); It; ++It)  TrafficLanes.Add(*It);
}

bool UDTPopulationSubsystem::IsHiddenFromPlayer(const FVector& Location) const
{
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!Camera)
	{
		return true;
	}
	const FVector ToPoint = Location - Camera->GetCameraLocation();
	const float Dot = FVector::DotProduct(ToPoint.GetSafeNormal(), Camera->GetCameraRotation().Vector());
	// Behind the camera, or far enough to be hidden by fog/buildings.
	return Dot < 0.4f || ToPoint.Size() > Config->SpawnRadiusMax * 0.85f;
}

void UDTPopulationSubsystem::Tick(float DeltaTime)
{
	if (!Config || !bEnabled)
	{
		return;
	}
	Timer += DeltaTime;
	if (Timer < Config->UpdateInterval)
	{
		return;
	}
	Timer = 0.f;

	Despawn();

	int32 Budget = Config->MaxSpawnsPerUpdate;
	while (Budget-- > 0 && Pedestrians.Num() < Config->MaxPedestrians && SidewalkPaths.Num() > 0 && Config->PedestrianClasses.Num() > 0)
	{
		SpawnPedestrian();
	}
	Budget = Config->MaxSpawnsPerUpdate;
	while (Budget-- > 0 && Vehicles.Num() < Config->MaxVehicles && TrafficLanes.Num() > 0 && Config->TrafficVehicleClasses.Num() > 0)
	{
		SpawnVehicle();
	}
}

void UDTPopulationSubsystem::Despawn()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}
	const FVector Center = Player->GetActorLocation();
	const float DespawnSq = FMath::Square(Config->DespawnRadius);

	auto Prune = [&](TArray<TWeakObjectPtr<APawn>>& List)
	{
		for (int32 i = List.Num() - 1; i >= 0; --i)
		{
			APawn* Pawn = List[i].Get();
			if (!Pawn)
			{
				List.RemoveAtSwap(i);
				continue;
			}
			// Never remove what the player is driving or what they can currently see.
			if (Pawn->IsPlayerControlled())
			{
				List.RemoveAtSwap(i); // the player owns it now
				continue;
			}
			if (FVector::DistSquared(Pawn->GetActorLocation(), Center) > DespawnSq && IsHiddenFromPlayer(Pawn->GetActorLocation()))
			{
				if (AController* C = Pawn->GetController()) C->Destroy();
				Pawn->Destroy();
				List.RemoveAtSwap(i);
			}
		}
	};
	Prune(Pedestrians);
	Prune(Vehicles);
}

void UDTPopulationSubsystem::SpawnPedestrian()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;

	ADTSidewalkPath* Path = SidewalkPaths[FMath::RandRange(0, SidewalkPaths.Num() - 1)];
	if (!Path) return;
	const USplineComponent* Spline = Path->GetSpline();
	const float Distance = FMath::FRandRange(0.f, Spline->GetSplineLength());
	const FVector Location = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

	const float Dist = FVector::Dist2D(Location, Player->GetActorLocation());
	if (Dist < Config->SpawnRadiusMin || Dist > Config->SpawnRadiusMax || !IsHiddenFromPlayer(Location))
	{
		return; // try again next update
	}

	TSubclassOf<ADTPedestrianCharacter> Class = Config->PedestrianClasses[FMath::RandRange(0, Config->PedestrianClasses.Num() - 1)];
	if (!Class) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	Params.bDeferConstruction = true;
	const FTransform Transform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location + FVector(0, 0, 100.f));
	if (ADTPedestrianCharacter* Ped = GetWorld()->SpawnActor<ADTPedestrianCharacter>(Class, Transform, Params))
	{
		Ped->SetStartPath(Path, Distance);
		UGameplayStatics::FinishSpawningActor(Ped, Transform);
		Pedestrians.Add(Ped);
	}
}

void UDTPopulationSubsystem::SpawnVehicle()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;

	ADTTrafficLane* Lane = TrafficLanes[FMath::RandRange(0, TrafficLanes.Num() - 1)];
	if (!Lane) return;
	const USplineComponent* Spline = Lane->GetSpline();
	const float Distance = FMath::FRandRange(0.f, Spline->GetSplineLength() * 0.8f);
	const FVector Location = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	const FRotator Rotation = Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

	const float Dist = FVector::Dist2D(Location, Player->GetActorLocation());
	if (Dist < Config->SpawnRadiusMin || Dist > Config->SpawnRadiusMax || !IsHiddenFromPlayer(Location))
	{
		return;
	}

	TSubclassOf<ADTVehicleBase> Class = Config->TrafficVehicleClasses[FMath::RandRange(0, Config->TrafficVehicleClasses.Num() - 1)];
	if (!Class) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	if (ADTVehicleBase* Vehicle = GetWorld()->SpawnActor<ADTVehicleBase>(Class, Location + FVector(0, 0, 60.f), FRotator(0.f, Rotation.Yaw, 0.f), Params))
	{
		ADTTrafficAIController* AI = GetWorld()->SpawnActor<ADTTrafficAIController>();
		AI->SetLane(Lane, Distance);
		AI->Possess(Vehicle);
		Vehicles.Add(Vehicle);
	}
}
