#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTPoliceDispatchSubsystem.generated.h"

class ADTCharacterBase;
class ADTVehicleBase;

/**
 * Spawns police officers and cruisers out of the player's sight to match the current wanted level,
 * and despawns them once the player has lost the cops.
 */
UCLASS()
class DOWNTOWN_API UDTPoliceDispatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDTPoliceDispatchSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Track an officer spawned elsewhere (e.g. deployed from a cruiser) so it is cleaned up later. */
	void RegisterOfficer(ADTCharacterBase* Officer) { Officers.AddUnique(Officer); }

	UFUNCTION(BlueprintPure, Category = "Police")
	int32 GetActiveOfficerCount() const { return Officers.Num(); }

protected:
	UFUNCTION()
	void HandleWantedLevelChanged(int32 NewLevel, int32 OldLevel);

	bool FindSpawnPoint(FVector& OutLocation, bool bOnRoad) const;
	void SpawnOfficer();
	void SpawnCruiser();
	void Cleanup(bool bForceAll);

	TArray<TWeakObjectPtr<ADTCharacterBase>> Officers;
	TArray<TWeakObjectPtr<ADTVehicleBase>> Cruisers;

	float DispatchTimer = 0.f;
	static constexpr float DispatchInterval = 3.f;
};
