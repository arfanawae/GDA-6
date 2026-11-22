#include "UI/DTHUDWidget.h"
#include "Character/DTPlayerCharacter.h"
#include "Combat/DTHealthComponent.h"
#include "Combat/DTWeaponComponent.h"
#include "Combat/DTWeaponBase.h"
#include "Interaction/DTInteractionComponent.h"
#include "Vehicle/DTVehicleEntryComponent.h"
#include "Vehicle/DTVehicleBase.h"
#include "Wanted/DTWantedSubsystem.h"
#include "GameFramework/PlayerController.h"

void UDTHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>())
	{
		Wanted->OnWantedLevelChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandleWantedChanged);
		Wanted->OnPlayerVisibilityChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandleVisibilityChanged);
		OnWantedLevelUpdated(Wanted->GetWantedLevel());
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandlePawnChanged);
		BindToCharacter(Cast<ADTPlayerCharacter>(PC->GetPawn()));
	}
}

void UDTHUDWidget::NativeDestruct()
{
	if (UDTWantedSubsystem* Wanted = GetWorld() ? GetWorld()->GetSubsystem<UDTWantedSubsystem>() : nullptr)
	{
		Wanted->OnWantedLevelChanged.RemoveAll(this);
		Wanted->OnPlayerVisibilityChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UDTHUDWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// Only rebind when a *new* character is possessed (respawn). Entering a vehicle keeps the character.
	if (ADTPlayerCharacter* Character = Cast<ADTPlayerCharacter>(NewPawn); Character && Character != PlayerCharacter.Get())
	{
		BindToCharacter(Character);
	}
}

void UDTHUDWidget::BindToCharacter(ADTPlayerCharacter* Character)
{
	if (!Character)
	{
		return;
	}
	PlayerCharacter = Character;

	UDTHealthComponent* Health = Character->GetHealthComponent();
	Health->OnHealthChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandleHealthChanged);
	OnHealthUpdated(Health->GetHealthPercent());

	UDTWeaponComponent* Weapons = Character->GetWeaponComponent();
	Weapons->OnAmmoChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandleAmmoChanged);
	if (const ADTWeaponBase* Weapon = Weapons->GetCurrentWeapon())
	{
		OnAmmoUpdated(Weapon->GetAmmoInMagazine(), Weapon->GetReserveAmmo());
	}

	Character->GetInteractionComponent()->OnFocusChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandleFocusChanged);
	Character->GetVehicleEntryComponent()->OnStateChanged.AddUniqueDynamic(this, &UDTHUDWidget::HandleVehicleState);
}

void UDTHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (const ADTPlayerCharacter* Character = PlayerCharacter.Get())
	{
		if (const ADTVehicleBase* Vehicle = Character->GetVehicleEntryComponent()->GetCurrentVehicle())
		{
			OnSpeedUpdated(FMath::Abs(Vehicle->GetSpeedKmh()));
		}
	}
}

void UDTHUDWidget::HandleHealthChanged(UDTHealthComponent* Health, float NewHealth, float Delta) { OnHealthUpdated(Health->GetHealthPercent()); }
void UDTHUDWidget::HandleAmmoChanged(ADTWeaponBase* Weapon, int32 Magazine, int32 Reserve)       { OnAmmoUpdated(Magazine, Reserve); }
void UDTHUDWidget::HandleWantedChanged(int32 NewLevel, int32 OldLevel)                           { OnWantedLevelUpdated(NewLevel); }
void UDTHUDWidget::HandleVisibilityChanged(bool bSeen)                                            { OnWantedFlashingChanged(!bSeen); }
void UDTHUDWidget::HandleFocusChanged(AActor* NewFocus, FText Prompt)                             { OnInteractionPromptChanged(Prompt, NewFocus != nullptr); }

void UDTHUDWidget::HandleVehicleState(EDTVehicleEntryState NewState, ADTVehicleBase* Vehicle)
{
	if (NewState == EDTVehicleEntryState::Seated || NewState == EDTVehicleEntryState::None)
	{
		OnVehicleModeChanged(NewState == EDTVehicleEntryState::Seated);
	}
}
