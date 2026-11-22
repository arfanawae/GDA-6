#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Vehicle/DTVehicleEntryComponent.h"
#include "DTHUDWidget.generated.h"

class ADTPlayerCharacter;
class ADTWeaponBase;
class ADTVehicleBase;
class UDTHealthComponent;

/**
 * Base class for WBP_HUD. Binds to gameplay delegates once and forwards them as
 * BlueprintImplementableEvents, so the widget never polls with property bindings.
 */
UCLASS(Abstract)
class DOWNTOWN_API UDTHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Events implemented in WBP_HUD ---
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnHealthUpdated(float Percent);
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnAmmoUpdated(int32 Magazine, int32 Reserve);
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnWantedLevelUpdated(int32 Stars);
	/** Stars flash while police have lost sight of the player. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnWantedFlashingChanged(bool bFlashing);
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnInteractionPromptChanged(const FText& Prompt, bool bVisible);
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnVehicleModeChanged(bool bInVehicle);
	/** Called every frame while driving. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnSpeedUpdated(float SpeedKmh);

	UFUNCTION(BlueprintPure, Category = "HUD")
	ADTPlayerCharacter* GetPlayerCharacter() const { return PlayerCharacter.Get(); }

private:
	UFUNCTION() void HandleHealthChanged(UDTHealthComponent* Health, float NewHealth, float Delta);
	UFUNCTION() void HandleAmmoChanged(ADTWeaponBase* Weapon, int32 Magazine, int32 Reserve);
	UFUNCTION() void HandleWantedChanged(int32 NewLevel, int32 OldLevel);
	UFUNCTION() void HandleVisibilityChanged(bool bSeen);
	UFUNCTION() void HandleFocusChanged(AActor* NewFocus, FText Prompt);
	UFUNCTION() void HandleVehicleState(EDTVehicleEntryState NewState, ADTVehicleBase* Vehicle);
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void BindToCharacter(ADTPlayerCharacter* Character);

	TWeakObjectPtr<ADTPlayerCharacter> PlayerCharacter;
};
