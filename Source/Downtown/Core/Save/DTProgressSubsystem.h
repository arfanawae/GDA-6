#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DTProgressSubsystem.generated.h"

class UDTSaveGame;
class UDTProvinceSubsystem;
class USaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTOnProgressSaved, bool, bSuccess, int32, VisitedCount);

/**
 * Keeps the 77-province progress across sessions.
 *   - Loads the save slot when the game instance starts.
 *   - UDTProvinceSubsystem calls AttachProvinceSubsystem() on BeginPlay: visited provinces are restored
 *     (unknown codes dropped) and every province change is recorded.
 *   - First visits are saved immediately (async); the current province is saved on shutdown.
 * Console: dt.Save.Now, dt.Save.Info, dt.Save.Reset   (LogDTGeo, prefix [Progress])
 */
UCLASS()
class DOWNTOWN_API UDTProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Called by UDTProvinceSubsystem once its province table is loaded. */
	void AttachProvinceSubsystem(UDTProvinceSubsystem* Provinces);

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void SaveNow(bool bAsync = true);

	/** Clears the save slot and the in-game visited list. */
	UFUNCTION(BlueprintCallable, Category = "Progress")
	void ResetProgress();

	UFUNCTION(BlueprintPure, Category = "Progress")
	TArray<FName> GetVisitedProvinces() const;

	UFUNCTION(BlueprintPure, Category = "Progress")
	bool GetFirstVisitTime(FName Province, FDateTime& OutTime) const;

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void LogInfo() const;

	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FDTOnProgressSaved OnProgressSaved;

	static const TCHAR* SlotName;

protected:
	UFUNCTION()
	void HandleProvinceChanged(FName NewProvince, FName OldProvince, bool bFirstVisit);

	void HandleAsyncSaved(const FString& Slot, const int32 UserIndex, bool bSuccess);
	UDTSaveGame* GetOrCreateSave();

	UPROPERTY(Transient)
	TObjectPtr<UDTSaveGame> Save;

	TWeakObjectPtr<UDTProvinceSubsystem> AttachedProvinces;
	bool bSaveInFlight = false;
	bool bSaveQueued = false;
	bool bDirty = false;
};
