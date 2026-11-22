#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DTInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDTInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the player can press "Interact" on: vehicles, doors, pickups. */
class DOWNTOWN_API IDTInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* InteractInstigator) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* InteractInstigator);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt() const;
};
