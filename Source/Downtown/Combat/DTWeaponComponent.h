#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTWeaponComponent.generated.h"

class ADTWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDTOnAmmoChanged, ADTWeaponBase*, Weapon, int32, Magazine, int32, Reserve);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTOnWeaponFired, ADTWeaponBase*, Weapon, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDTOnWeaponEquipped, ADTWeaponBase*, Weapon);

/** Owns the inventory (2 slots in phase 1), fire timing, reload and aiming state. Used by the player and police. */
UCLASS(ClassGroup = (Downtown), meta = (BlueprintSpawnableComponent))
class DOWNTOWN_API UDTWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDTWeaponComponent();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CycleWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetAiming(bool bNewAiming) { bAiming = bNewAiming; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const { return bAiming; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bReloading; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	ADTWeaponBase* GetCurrentWeapon() const;

	/** Hides/shows the held weapon, e.g. while driving. */
	void SetWeaponHidden(bool bHidden);

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FDTOnAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FDTOnWeaponFired OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FDTOnWeaponEquipped OnWeaponEquipped;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void FireOnce();
	void FinishReload();
	void EquipIndex(int32 Index);
	void BroadcastAmmo();

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TArray<TSubclassOf<ADTWeaponBase>> StartingWeapons;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName HandSocketName = TEXT("hand_r_weapon");

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADTWeaponBase>> Weapons;

	int32 CurrentIndex = INDEX_NONE;
	bool bAiming = false;
	bool bReloading = false;
	bool bTriggerHeld = false;
	FTimerHandle FireTimer;
	FTimerHandle ReloadTimer;
};
