#include "Wanted/DTWantedSubsystem.h"
#include "Wanted/DTCrimeData.h"
#include "Wanted/DTWantedConfig.h"
#include "Core/DTGameMode.h"
#include "Core/DTPlayerController.h"
#include "Character/DTPlayerCharacter.h"
#include "Character/DTCharacterBase.h"
#include "Downtown.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool UDTWantedSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDTWantedSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (const ADTGameMode* GM = InWorld.GetAuthGameMode<ADTGameMode>())
	{
		Config = GM->WantedConfig;
	}
	if (!Config)
	{
		Config = GetMutableDefault<UDTWantedConfig>();
		UE_LOG(LogDowntown, Warning, TEXT("No WantedConfig on the GameMode; using defaults (no police classes = no dispatch)."));
	}
}

APawn* UDTWantedSubsystem::GetPlayerPawn() const
{
	// Use the character even while driving so distance checks follow the player.
	if (const ADTPlayerController* PC = Cast<ADTPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (PC->GetPlayerCharacter())
		{
			return PC->GetPlayerCharacter();
		}
	}
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

bool UDTWantedSubsystem::HasWitness(const FVector& Location, const AActor* Offender) const
{
	const float RadiusSq = FMath::Square(Config->WitnessRadius);
	for (TActorIterator<ADTCharacterBase> It(GetWorld()); It; ++It)
	{
		ADTCharacterBase* Candidate = *It;
		if (Candidate == Offender || Candidate->IsDead() || Candidate->IsPlayerControlled())
		{
			continue;
		}
		if (FVector::DistSquared(Candidate->GetActorLocation(), Location) > RadiusSq)
		{
			continue;
		}
		FVector Eyes; FRotator EyesRot;
		Candidate->GetActorEyesViewPoint(Eyes, EyesRot);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DTWitness), false, Offender);
		Params.AddIgnoredActor(Candidate);
		if (!GetWorld()->LineTraceTestByChannel(Eyes, Location, ECC_Visibility, Params))
		{
			return true;
		}
	}
	return false;
}

void UDTWantedSubsystem::ReportCrime(const UDTCrimeData* Crime, FVector Location, AActor* Offender)
{
	if (!Crime || !Config || Offender != GetPlayerPawn())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (const float* Last = LastReportTime.Find(Crime->GetFName()); Last && Now - *Last < Crime->ReportCooldown)
	{
		return;
	}

	if (Crime->bRequiresWitness && WantedLevel == 0 && !HasWitness(Location, Offender))
	{
		return; // nobody saw it (once wanted, police are already tracking crimes)
	}

	LastReportTime.Add(Crime->GetFName(), Now);
	Heat += Crime->Heat;
	LastKnownLocation = Location;
	OutOfSightTime = 0.f;

	const int32 NewLevel = FMath::Max(ComputeLevelFromHeat(), FMath::Min(Crime->MinimumStars, Config->GetMaxStars()));
	if (NewLevel > WantedLevel)
	{
		SetWantedLevel(NewLevel);
	}
	UE_LOG(LogDowntown, Verbose, TEXT("Crime %s: heat %.0f, stars %d"), *Crime->GetName(), Heat, WantedLevel);
}

void UDTWantedSubsystem::ReportPlayerSighted(FVector Location)
{
	if (WantedLevel == 0)
	{
		return;
	}
	LastKnownLocation = Location;
	LastSightedTime = GetWorld()->GetTimeSeconds();
	OutOfSightTime = 0.f;
	if (!bPlayerSeen)
	{
		bPlayerSeen = true;
		OnPlayerVisibilityChanged.Broadcast(true);
	}
}

int32 UDTWantedSubsystem::ComputeLevelFromHeat() const
{
	int32 Level = 0;
	for (int32 i = 0; i < Config->Levels.Num(); ++i)
	{
		if (Heat >= Config->Levels[i].HeatThreshold)
		{
			Level = i + 1;
		}
	}
	return Level;
}

void UDTWantedSubsystem::SetWantedLevel(int32 NewLevel)
{
	NewLevel = FMath::Clamp(NewLevel, 0, Config ? Config->GetMaxStars() : 3);
	if (NewLevel == WantedLevel)
	{
		return;
	}
	const int32 Old = WantedLevel;
	WantedLevel = NewLevel;
	if (WantedLevel > 0 && Old == 0)
	{
		// Starting a chase: police "know" where the crime happened.
		LastSightedTime = GetWorld()->GetTimeSeconds();
		bPlayerSeen = true;
		OnPlayerVisibilityChanged.Broadcast(true);
	}
	OnWantedLevelChanged.Broadcast(WantedLevel, Old);
}

void UDTWantedSubsystem::ClearWantedLevel()
{
	Heat = 0.f;
	OutOfSightTime = 0.f;
	if (bPlayerSeen)
	{
		bPlayerSeen = false;
		OnPlayerVisibilityChanged.Broadcast(false);
	}
	SetWantedLevel(0);
}

float UDTWantedSubsystem::GetSearchRadius() const
{
	return Config && WantedLevel > 0 ? Config->Levels[WantedLevel - 1].SearchRadius : 0.f;
}

bool UDTWantedSubsystem::CanUseLethalForce() const
{
	return Config && WantedLevel > 0 && Config->Levels[WantedLevel - 1].bUseLethalForce;
}

float UDTWantedSubsystem::GetEvadeProgress() const
{
	if (!Config || WantedLevel == 0)
	{
		return 0.f;
	}
	return FMath::Clamp(OutOfSightTime / Config->Levels[WantedLevel - 1].EvadeSeconds, 0.f, 1.f);
}

void UDTWantedSubsystem::Tick(float DeltaTime)
{
	if (WantedLevel == 0 || !Config)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bSeenNow = Now - LastSightedTime < SightingTimeout;
	if (bSeenNow != bPlayerSeen)
	{
		bPlayerSeen = bSeenNow;
		OnPlayerVisibilityChanged.Broadcast(bPlayerSeen);
	}

	if (bPlayerSeen)
	{
		return;
	}

	// Evasion timer only runs while the player is outside the search area.
	const APawn* Player = GetPlayerPawn();
	const bool bOutsideSearch = Player && FVector::Dist2D(Player->GetActorLocation(), LastKnownLocation) > GetSearchRadius();
	if (bOutsideSearch)
	{
		OutOfSightTime += DeltaTime;
		if (OutOfSightTime >= Config->Levels[WantedLevel - 1].EvadeSeconds)
		{
			ClearWantedLevel();
		}
	}
}
