#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/Province/DTProvinceTypes.h"
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
 *
 * Console: dt.Province.Teleport TH-12   dt.Province.DrawBorders 1
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

	/** Province containing a world location, or NAME_None (sea / outside Thailand). */
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
	int32 GetProvinceCount() const { return Provinces.Num(); }

	/** Restore progress (e.g. from a save game). Does not fire events. */
	UFUNCTION(BlueprintCallable, Category = "Province")
	void SetVisited(const TArray<FName>& Codes) { Visited = TSet<FName>(Codes); }

	UFUNCTION(BlueprintPure, Category = "Province")
	TArray<FName> GetVisited() const { return Visited.Array(); }

	/** Moves the player (or the vehicle they drive) to a province's reference point. */
	UFUNCTION(BlueprintCallable, Category = "Province")
	bool TeleportPlayerToProvince(FName Code);

	UPROPERTY(BlueprintAssignable, Category = "Province")
	FDTOnProvinceChanged OnProvinceChanged;

	void DebugDrawBorders() const;

protected:
	struct FRuntimeProvince
	{
		FDTProvinceRow Row;
		FBox2D Bounds;
		TArray<FBox2D> RingBounds;
	};

	bool Contains(const FRuntimeProvince& P, const FVector2D& Point) const;
	static bool PointInRing(const TArray<FVector2D>& Ring, const FVector2D& Point);
	void SetCurrent(FName NewProvince);

	TMap<FName, FRuntimeProvince> Provinces;
	TSet<FName> Visited;

	FName CurrentProvince = NAME_None;
	FName PendingProvince = NAME_None;
	float PendingTime = 0.f;
	float CheckTimer = 0.f;

	/** Seconds between checks. */
	float CheckInterval = 0.2f;

	/** A new province must be detected continuously this long before it is announced. */
	float ConfirmSeconds = 0.6f;
};
