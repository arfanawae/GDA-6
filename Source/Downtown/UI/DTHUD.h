#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DTHUD.generated.h"

class UDTHUDWidget;

/** Creates WBP_HUD for the local player. */
UCLASS()
class DOWNTOWN_API ADTHUD : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "HUD")
	UDTHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:
	virtual void BeginPlay() override;

	/** Set to WBP_HUD in BP_HUD (or on the GameMode's HUD class). */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UDTHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDTHUDWidget> HUDWidget;
};
