#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTTrafficLight.generated.h"

UENUM(BlueprintType)
enum class EDTLightState : uint8
{
	Green,
	Yellow,
	Red
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDTOnLightChanged, EDTLightState, NewState);

/**
 * Timer-driven traffic light. Lanes that end at an intersection reference one of these.
 * For a 4-way junction place two lights with PhaseOffset = 0 and GreenTime + YellowTime.
 */
UCLASS()
class DOWNTOWN_API ADTTrafficLight : public AActor
{
	GENERATED_BODY()

public:
	ADTTrafficLight();

	UFUNCTION(BlueprintPure, Category = "Traffic")
	EDTLightState GetLightState() const { return LightState; }

	/** Vehicles may enter the intersection. */
	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool CanPass() const { return LightState == EDTLightState::Green; }

	/** BP updates the bulb materials from this. */
	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FDTOnLightChanged OnLightChanged;

protected:
	virtual void BeginPlay() override;
	void Advance();
	void SetLightState(EDTLightState NewState);

	UPROPERTY(EditAnywhere, Category = "Traffic") float GreenTime = 12.f;
	UPROPERTY(EditAnywhere, Category = "Traffic") float YellowTime = 3.f;
	UPROPERTY(EditAnywhere, Category = "Traffic") float RedTime = 15.f;

	/** Seconds to delay the first cycle, used to sync opposite directions. */
	UPROPERTY(EditAnywhere, Category = "Traffic") float PhaseOffset = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Traffic")
	EDTLightState LightState = EDTLightState::Green;

	FTimerHandle CycleTimer;
};
