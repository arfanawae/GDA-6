#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DTPlayerController.generated.h"

class UInputMappingContext;
class ADTPlayerCharacter;

/**
 * Swaps Enhanced Input mapping contexts when the possessed pawn changes
 * (IMC_OnFoot <-> IMC_Vehicle), and remembers the on-foot character while driving.
 */
UCLASS()
class DOWNTOWN_API ADTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** The player's character, valid even while it sits inside a vehicle. */
	UFUNCTION(BlueprintPure, Category = "Player")
	ADTPlayerCharacter* GetPlayerCharacter() const { return PlayerCharacter.Get(); }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	void ApplyMappingContexts(APawn* InPawn);

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> OnFootContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleContext;

	TWeakObjectPtr<ADTPlayerCharacter> PlayerCharacter;
};
