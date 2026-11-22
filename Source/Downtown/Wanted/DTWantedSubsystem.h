#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTWantedSubsystem.generated.h"

class UDTCrimeData;
class UDTWantedConfig;
class ADTCharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTOnWantedLevelChanged, int32, NewLevel, int32, OldLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDTOnPlayerVisibilityChanged, bool, bSeenByPolice);

/**
 * Owns the wanted level (0-3 stars). Anything can ReportCrime(); police controllers call
 * ReportPlayerSighted() while they can see the player. When out of sight and outside the
 * search radius for EvadeSeconds, the stars clear.
 */
UCLASS()
class DOWNTOWN_API UDTWantedSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDTWantedSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Reports a crime by Offender. Only the player's crimes raise the wanted level. */
	UFUNCTION(BlueprintCallable, Category = "Wanted")
	void ReportCrime(const UDTCrimeData* Crime, FVector Location, AActor* Offender);

	/** Police call this every perception update in which they see the player. */
	UFUNCTION(BlueprintCallable, Category = "Wanted")
	void ReportPlayerSighted(FVector Location);

	UFUNCTION(BlueprintCallable, Category = "Wanted")
	void ClearWantedLevel();

	UFUNCTION(BlueprintPure, Category = "Wanted")
	int32 GetWantedLevel() const { return WantedLevel; }

	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetHeat() const { return Heat; }

	UFUNCTION(BlueprintPure, Category = "Wanted")
	FVector GetLastKnownLocation() const { return LastKnownLocation; }

	/** True while police currently have eyes on the player (HUD stars stop flashing). */
	UFUNCTION(BlueprintPure, Category = "Wanted")
	bool IsPlayerSeen() const { return bPlayerSeen; }

	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetSearchRadius() const;

	/** 0..1 progress towards losing the cops. */
	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetEvadeProgress() const;

	UFUNCTION(BlueprintPure, Category = "Wanted")
	bool CanUseLethalForce() const;

	const UDTWantedConfig* GetConfig() const { return Config; }

	UPROPERTY(BlueprintAssignable, Category = "Wanted")
	FDTOnWantedLevelChanged OnWantedLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wanted")
	FDTOnPlayerVisibilityChanged OnPlayerVisibilityChanged;

protected:
	bool HasWitness(const FVector& Location, const AActor* Offender) const;
	void SetWantedLevel(int32 NewLevel);
	int32 ComputeLevelFromHeat() const;
	APawn* GetPlayerPawn() const;

	UPROPERTY(Transient)
	TObjectPtr<UDTWantedConfig> Config;

	TMap<FName, float> LastReportTime;

	int32 WantedLevel = 0;
	float Heat = 0.f;
	FVector LastKnownLocation = FVector::ZeroVector;
	bool bPlayerSeen = false;
	float LastSightedTime = -1000.f;
	float OutOfSightTime = 0.f;

	/** Sightings older than this count as "lost visual". */
	static constexpr float SightingTimeout = 1.0f;
};
