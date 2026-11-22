#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTWeaponBase.generated.h"

class UDTWeaponData;
class USkeletalMeshComponent;

/** A hitscan weapon actor held by UDTWeaponComponent. BP_Weapon_* derive from this. */
UCLASS(Abstract)
class DOWNTOWN_API ADTWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ADTWeaponBase();

	/** Fires one round. ViewLocation/ViewDirection come from the camera (player) or the eyes (AI). Returns true if a round was fired. */
	bool Fire(const FVector& ViewLocation, const FVector& ViewDirection, bool bAiming);

	bool CanFire() const;
	bool CanReload() const;
	void FinishReload();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	const UDTWeaponData* GetWeaponData() const { return WeaponData; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	float GetTimeBetweenShots() const;

	/** Blueprint hook for muzzle flash, tracer and impact FX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnFired(const FVector& MuzzleLocation, const FVector& ImpactPoint, bool bHit, const FHitResult& Hit);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UDTWeaponData> WeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	int32 AmmoInMagazine = 0;
	int32 ReserveAmmo = 0;
	float LastFireTime = -1000.f;
};
