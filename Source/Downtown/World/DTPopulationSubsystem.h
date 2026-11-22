#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTPopulationSubsystem.generated.h"

class UDTPopulationConfig;
class ADTSidewalkPath;
class ADTTrafficLane;

/**
 * Keeps a bubble of ambient life around the player: spawns pedestrians on sidewalk splines
 * and traffic on lanes outside the camera view, despawns them when far away.
 * TODO (phase 7): pool actors instead of Spawn/Destroy if profiling shows hitches.
 */
UCLASS()
class DOWNTOWN_API UDTPopulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDTPopulationSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UFUNCTION(BlueprintPure, Category = "Population")
	int32 GetPedestrianCount() const { return Pedestrians.Num(); }

	UFUNCTION(BlueprintPure, Category = "Population")
	int32 GetVehicleCount() const { return Vehicles.Num(); }

	/** Debug / gameplay toggle (e.g. disable during cutscenes). */
	UFUNCTION(BlueprintCallable, Category = "Population")
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

protected:
	bool IsHiddenFromPlayer(const FVector& Location) const;
	void Despawn();
	void SpawnPedestrian();
	void SpawnVehicle();

	UPROPERTY(Transient) TObjectPtr<UDTPopulationConfig> Config;
	UPROPERTY(Transient) TArray<TObjectPtr<ADTSidewalkPath>> SidewalkPaths;
	UPROPERTY(Transient) TArray<TObjectPtr<ADTTrafficLane>> TrafficLanes;

	TArray<TWeakObjectPtr<APawn>> Pedestrians;
	TArray<TWeakObjectPtr<APawn>> Vehicles;

	float Timer = 0.f;
	bool bEnabled = true;
};
