#include "Combat/DTWeaponBase.h"
#include "Combat/DTWeaponData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"

ADTWeaponBase::ADTWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;
}

void ADTWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	if (WeaponData)
	{
		AmmoInMagazine = WeaponData->MagazineSize;
		ReserveAmmo = WeaponData->StartingReserveAmmo;
	}
}

float ADTWeaponBase::GetTimeBetweenShots() const
{
	return WeaponData && WeaponData->FireRate > 0.f ? 60.f / WeaponData->FireRate : 0.25f;
}

bool ADTWeaponBase::CanFire() const
{
	return WeaponData && AmmoInMagazine > 0 && GetWorld()->GetTimeSeconds() - LastFireTime >= GetTimeBetweenShots();
}

bool ADTWeaponBase::CanReload() const
{
	return WeaponData && ReserveAmmo > 0 && AmmoInMagazine < WeaponData->MagazineSize;
}

void ADTWeaponBase::FinishReload()
{
	if (!WeaponData)
	{
		return;
	}
	const int32 Needed = WeaponData->MagazineSize - AmmoInMagazine;
	const int32 Taken = FMath::Min(Needed, ReserveAmmo);
	AmmoInMagazine += Taken;
	ReserveAmmo -= Taken;
}

bool ADTWeaponBase::Fire(const FVector& ViewLocation, const FVector& ViewDirection, bool bAiming)
{
	if (!CanFire())
	{
		return false;
	}

	UWorld* World = GetWorld();
	LastFireTime = World->GetTimeSeconds();
	--AmmoInMagazine;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTWeaponTrace), true);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	// 1) Camera trace: find what the crosshair points at.
	const float Spread = bAiming ? WeaponData->AimSpreadDegrees : WeaponData->HipSpreadDegrees;
	const FVector ShotDir = FMath::VRandCone(ViewDirection, FMath::DegreesToRadians(Spread));
	const FVector ViewEnd = ViewLocation + ShotDir * WeaponData->Range;

	FHitResult ViewHit;
	const FVector AimPoint = World->LineTraceSingleByChannel(ViewHit, ViewLocation, ViewEnd, ECC_Visibility, Params)
		? ViewHit.ImpactPoint : ViewEnd;

	// 2) Muzzle trace: make sure nothing blocks the barrel (prevents shooting through walls from behind cover).
	const FVector Muzzle = Mesh->DoesSocketExist(MuzzleSocketName) ? Mesh->GetSocketLocation(MuzzleSocketName) : GetActorLocation();
	const FVector MuzzleDir = (AimPoint - Muzzle).GetSafeNormal();
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, AimPoint + MuzzleDir * 10.f, ECC_Visibility, Params);

	if (bHit && Hit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), WeaponData->Damage, MuzzleDir, Hit, InstigatorController, this, UDamageType::StaticClass());
	}

	if (WeaponData->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->FireSound, Muzzle);
	}

	// Let AI hear the shot.
	UAISense_Hearing::ReportNoiseEvent(World, Muzzle, 1.f, GetOwner(), WeaponData->NoiseRange, TEXT("Gunshot"));

	OnFired(Muzzle, bHit ? Hit.ImpactPoint : AimPoint, bHit, Hit);
	return true;
}
