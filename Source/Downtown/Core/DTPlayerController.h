#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DTPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ADTPlayerCharacter;
class UDTWorldMapWidget;

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

	/** Opens/closes the Thailand world map (bound to MapAction, default key M in IMC_Global). */
	UFUNCTION(BlueprintCallable, Category = "World Map")
	void ToggleWorldMap();

	UFUNCTION(BlueprintPure, Category = "World Map")
	bool IsWorldMapOpen() const { return bWorldMapOpen; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	void ApplyMappingContexts(APawn* InPawn);

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> OnFootContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleContext;

	/** Always active on foot and in vehicles (map, pause...). Priority 1 so it wins over the pawn contexts. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GlobalContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MapAction;

	/** WBP_WorldMap (parent UDTWorldMapWidget). */
	UPROPERTY(EditDefaultsOnly, Category = "World Map")
	TSubclassOf<UDTWorldMapWidget> WorldMapClass;

	UPROPERTY(EditDefaultsOnly, Category = "World Map")
	bool bPauseWhileMapOpen = false;

	UPROPERTY(Transient)
	TObjectPtr<UDTWorldMapWidget> WorldMap;

	bool bWorldMapOpen = false;

	TWeakObjectPtr<ADTPlayerCharacter> PlayerCharacter;
};
