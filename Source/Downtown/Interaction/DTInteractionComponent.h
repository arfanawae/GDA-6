#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTOnFocusChanged, AActor*, NewFocus, FText, Prompt);

/** Finds the best IDTInteractable near the owner on a timer and forwards Interact presses to it. */
UCLASS(ClassGroup = (Downtown), meta = (BlueprintSpawnableComponent))
class DOWNTOWN_API UDTInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDTInteractionComponent();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** HUD binds to this to show "Press F to enter vehicle". */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FDTOnFocusChanged OnFocusChanged;

protected:
	virtual void BeginPlay() override;
	void UpdateFocus();

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float ScanInterval = 0.1f;

	TWeakObjectPtr<AActor> FocusedActor;
	FTimerHandle ScanTimer;
};
