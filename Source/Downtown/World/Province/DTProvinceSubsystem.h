#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/Province/DTProvinceTypes.h"
#include "World/Province/DTProvinceGeometry.h"
#include "DTProvinceSubsystem.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDTOnProvinceChanged, FName, NewProvince, FName, OldProvince, bool, bFirstVisit);

/**
 * Tracks which of the 77 provinces the player is in, using point-in-polygon on the
 * outlines from DT_Provinces (no hand-placed trigger volumes needed).
 *
 *  - Checks ~5x per second, current province first, then its neighbours, then everything.
 *  - A new province must hold for ConfirmSeconds before the change is announced, so driving
 *    along a border road does not spam the HUD.
 *  - Remembers visited provinces (checkpoint / collection progress).
 *  - An optional "active zone" limits detection to some provinces for focused tests.
 *
 * Console (PIE / game):
 *   dt.Province.Zone <zone|all>      limit detection, e.g. dt.Province.Zone test-area
 *   dt.Province.List [zone]          list provinces with reference points and bounds
 *   dt.Province.Where                where is the player (world, lat/lon, province)
 *   dt.Province.Teleport <code|name> e.g. dt.Province.Teleport TH-12 / Nonthaburi
 *   dt.Province.ResetVisited         clear checkpoint progress
 *   dt.Province.DrawBorders 1        draw borders around the player
 *   dt.Province.LogChecks 1          verbose log of every detection step
 * All output goes to the LogDTGeo category.
 */
UCLASS()
class DOWNTOWN_API UDTProvinceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDTProvinceSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Province containing a world location, or NAME_None (sea / outside Thailand / outside the active zone). */
	UFUNCTION(BlueprintCallable, Category = "Province")
	FName FindProvinceAt(const FVector& WorldLocation, FName Hint = NAME_None) const;

	UFUNCTION(BlueprintPure, Category = "Province")
	FName GetCurrentProvince() const { return CurrentProvince; }

	UFUNCTION(BlueprintCallable, Category = "Province")
	bool GetProvinceRow(FName Code, FDTProvinceRow& OutRow) const;

	UFUNCTION(BlueprintPure, Category = "Province")
	bool HasVisited(FName Code) const { return Visited.Contains(Code); }

	UFUNCTION(BlueprintPure, Category = "Province")
	int32 GetVisitedCount() const { return Visited.Num(); }

	UFUNCTION(BlueprintPure, Category = "Province")
	int32 GetProvinceCount() const { return Geometry.Num(); }

	/** Restore progress (e.g. from a save game). Does not fire events. */
	UFUNCTION(BlueprintCallable, Category = "Province")
	void SetVisited(const TArray<FName>& Codes) { Visited = TSet<FName>(Codes); }

	UFUNCTION(BlueprintPure, Category = "Province")
	TArray<FName> GetVisited() const { return Visited.Array(); }

	UFUNCTION(BlueprintCallable, Category = "Province")
	void ResetVisited();

	/** Moves the player (or the vehicle they drive) to a province's reference point. */
	UFUNCTION(BlueprintCallable, Category = "Province")
	bool TeleportPlayerToProvince(FName Code);

	/** Same, but accepts a code or Thai/English name ("TH-12", "Nonthaburi"). Uses the first match. */
	UFUNCTION(BlueprintCallable, Category = "Province|Testing")
	bool TeleportPlayerToZone(const FString& Zone);

	/**
	 * Test helper: only provinces of this zone are detected ("all"/"" = every province).
	 * Same zone syntax as ADTGeoImporter::ZoneFilter. Returns false (and logs why) for unknown zones.
	 */
	UFUNCTION(BlueprintCallable, Category = "Province|Testing")
	bool SetActiveZone(const FString& Zone);

	UFUNCTION(BlueprintPure, Category = "Province|Testing")
	FString GetActiveZone() const { return ActiveZoneName; }

	/** Logs every province of a zone with reference point, lat/lon and size. */
	UFUNCTION(BlueprintCallable, Category = "Province|Testing")
	void LogZone(const FString& Zone) const;

	/** Logs the player's world position, lat/lon and detected province. */
	UFUNCTION(BlueprintCallable, Category = "Province|Testing")
	void LogPlayerLocation() const;

	UPROPERTY(BlueprintAssignable, Category = "Province")
	FDTOnProvinceChanged OnProvinceChanged;

	void DebugDrawBorders() const;

protected:
	void SetCurrent(FName NewProvince);
	void LogLoadSummary(const UDataTable* Table, const TArray<FString>& Warnings) const;
	FString DescribeLocation(const FVector& Location) const;

	FDTProvinceGeometry Geometry;
	TSet<FName> Visited;

	/** Empty = all provinces. */
	TSet<FName> ActiveZone;
	FString ActiveZoneName = TEXT("all");

	FName CurrentProvince = NAME_None;
	FName PendingProvince = NAME_None;
	float PendingTime = 0.f;
	float CheckTimer = 0.f;
	bool bLoggedOutside = false;

	/** Seconds between checks. */
	float CheckInterval = 0.2f;

	/** A new province must be detected continuously this long before it is announced. */
	float ConfirmSeconds = 0.6f;
};
