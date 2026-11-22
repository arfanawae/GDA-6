#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTVehicleEntryComponent.generated.h"

class ADTVehicleBase;
class UDTVehicleSeatComponent;
class UAnimMontage;

UENUM(BlueprintType)
enum class EDTVehicleEntryState : uint8
{
	None,
	Entering,
	Seated,
	Exiting
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTOnVehicleStateChanged, EDTVehicleEntryState, NewState, ADTVehicleBase*, Vehicle);

/**
 * Lives on the character. Runs the enter/exit state machine:
 * warp to door -> enter montage -> attach to seat -> possess vehicle (camera blend),
 * and the reverse on exit. Works without montages too (instant), for early prototyping.
 */
UCLASS(ClassGroup = (Downtown), meta = (BlueprintSpawnableComponent))
class DOWNTOWN_API UDTVehicleEntryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDTVehicleEntryComponent();

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	bool RequestEnter(ADTVehicleBase* Vehicle);

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	bool RequestExit();

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	EDTVehicleEntryState GetState() const { return State; }

	/** True while entering/exiting (blocks other input). */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsBusy() const { return State == EDTVehicleEntryState::Entering || State == EDTVehicleEntryState::Exiting; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsInVehicle() const { return State != EDTVehicleEntryState::None; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	ADTVehicleBase* GetCurrentVehicle() const { return CurrentVehicle; }

	UPROPERTY(BlueprintAssignable, Category = "Vehicle")
	FDTOnVehicleStateChanged OnStateChanged;

protected:
	void SetState(EDTVehicleEntryState NewState);
	void FinishEnter();
	void FinishExit();
	bool PlayMontage(UAnimMontage* Montage, void (UDTVehicleEntryComponent::*OnFinished)());
	void SetCharacterInVehicleMode(bool bInVehicle);
	/** Finds a free spot for the character: door side, then opposite side, then the roof. */
	FVector FindExitLocation() const;

	/** Root-motion montage; add a Motion Warping notify with target name WarpTargetName. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation") TObjectPtr<UAnimMontage> EnterMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation") TObjectPtr<UAnimMontage> CarJackMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation") TObjectPtr<UAnimMontage> ExitMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation") TObjectPtr<UAnimMontage> BailOutMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation") FName WarpTargetName = TEXT("VehicleDoor");

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraBlendTime = 0.5f;

	/** Max distance from the door to start entering. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle")
	float MaxEnterDistance = 350.f;

	UPROPERTY(Transient) TObjectPtr<ADTVehicleBase> CurrentVehicle;
	UPROPERTY(Transient) TObjectPtr<UDTVehicleSeatComponent> CurrentSeat;

	EDTVehicleEntryState State = EDTVehicleEntryState::None;
};
