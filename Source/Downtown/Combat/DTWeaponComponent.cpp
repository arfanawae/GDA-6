#include "Combat/DTWeaponComponent.h"
#include "Combat/DTWeaponBase.h"
#include "Combat/DTWeaponData.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"

UDTWeaponComponent::UDTWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDTWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;

	for (const TSubclassOf<ADTWeaponBase>& WeaponClass : StartingWeapons)
	{
		if (!WeaponClass)
		{
			continue;
		}
		ADTWeaponBase* Weapon = GetWorld()->SpawnActor<ADTWeaponBase>(WeaponClass, Params);
		Weapon->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocketName);
		Weapon->SetActorHiddenInGame(true);
		Weapons.Add(Weapon);
	}

	if (Weapons.Num() > 0)
	{
		EquipIndex(0);
	}
}

void UDTWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ADTWeaponBase* Weapon : Weapons)
	{
		if (Weapon)
		{
			Weapon->Destroy();
		}
	}
	Super::EndPlay(EndPlayReason);
}

ADTWeaponBase* UDTWeaponComponent::GetCurrentWeapon() const
{
	return Weapons.IsValidIndex(CurrentIndex) ? Weapons[CurrentIndex].Get() : nullptr;
}

void UDTWeaponComponent::SetWeaponHidden(bool bHidden)
{
	StopFire();
	if (ADTWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->SetActorHiddenInGame(bHidden);
	}
}

void UDTWeaponComponent::EquipIndex(int32 Index)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return;
	}
	if (ADTWeaponBase* Old = GetCurrentWeapon())
	{
		Old->SetActorHiddenInGame(true);
	}
	CurrentIndex = Index;
	GetCurrentWeapon()->SetActorHiddenInGame(false);
	OnWeaponEquipped.Broadcast(GetCurrentWeapon());
	BroadcastAmmo();
}

void UDTWeaponComponent::CycleWeapon()
{
	if (Weapons.Num() > 1 && !bReloading)
	{
		StopFire();
		EquipIndex((CurrentIndex + 1) % Weapons.Num());
	}
}

void UDTWeaponComponent::StartFire()
{
	bTriggerHeld = true;
	FireOnce();

	const ADTWeaponBase* Weapon = GetCurrentWeapon();
	if (Weapon && Weapon->GetWeaponData() && Weapon->GetWeaponData()->bAutomatic)
	{
		GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &UDTWeaponComponent::FireOnce, Weapon->GetTimeBetweenShots(), true);
	}
}

void UDTWeaponComponent::StopFire()
{
	bTriggerHeld = false;
	GetWorld()->GetTimerManager().ClearTimer(FireTimer);
}

void UDTWeaponComponent::FireOnce()
{
	ADTWeaponBase* Weapon = GetCurrentWeapon();
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!Weapon || !OwnerPawn || bReloading)
	{
		return;
	}

	if (Weapon->GetAmmoInMagazine() <= 0)
	{
		StopFire();
		Reload();
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (AController* Controller = OwnerPawn->GetController())
	{
		// Players: camera view point. AI: eyes view point.
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		OwnerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	if (!Weapon->Fire(ViewLocation, ViewRotation.Vector(), bAiming))
	{
		return;
	}

	const UDTWeaponData* Data = Weapon->GetWeaponData();
	if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
	{
		PC->AddPitchInput(-Data->RecoilPitch);
	}
	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn); Character && Data->FireMontage)
	{
		Character->PlayAnimMontage(Data->FireMontage);
	}

	OnWeaponFired.Broadcast(Weapon, Weapon->GetActorLocation());
	BroadcastAmmo();
}

void UDTWeaponComponent::Reload()
{
	ADTWeaponBase* Weapon = GetCurrentWeapon();
	if (!Weapon || bReloading || !Weapon->CanReload())
	{
		return;
	}

	bReloading = true;
	const UDTWeaponData* Data = Weapon->GetWeaponData();
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()); Character && Data->ReloadMontage)
	{
		Character->PlayAnimMontage(Data->ReloadMontage);
	}
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UDTWeaponComponent::FinishReload, Data->ReloadTime, false);
}

void UDTWeaponComponent::FinishReload()
{
	bReloading = false;
	if (ADTWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->FinishReload();
	}
	BroadcastAmmo();
}

void UDTWeaponComponent::BroadcastAmmo()
{
	if (ADTWeaponBase* Weapon = GetCurrentWeapon())
	{
		OnAmmoChanged.Broadcast(Weapon, Weapon->GetAmmoInMagazine(), Weapon->GetReserveAmmo());
	}
}
