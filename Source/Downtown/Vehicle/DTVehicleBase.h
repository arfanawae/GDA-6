#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Interaction/DTInteractable.h"
#include "DTVehicleBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UDTHealthComponent;
class UDTVehicleSeatComponent;
class UDTVehicleData;
class UDTCrimeData;
class ADTCharacterBase;
struct FInputActionValue;

/**
 * Drivable Chaos vehicle. Player input when possessed by ADTPlayerController,
 * DriveTowards() when possessed by traffic/police AI.
 */
UCLASS(Abstract)
class DOWNTOWN_API ADTVehicleBase : public AWheeledVehiclePawn, public IDTInteractable
{
	GENERATED_BODY()

public:
	ADTVehicleBase();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// IDTInteractable
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	/** Returns the best seat to enter from the given location (driver first). */
	UDTVehicleSeatComponent* FindSeatForEntry(const FVector& FromLocation) const;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	UDTVehicleSeatComponent* GetDriverSeat() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSpeedKmh() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	const UDTVehicleData* GetVehicleData() const { return VehicleData; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool HasAIDriver() const { return bHasAIDriver; }

	/** Called by traffic spawning; on carjack an NPC of DriverNPCClass is ejected. */
	void SetHasAIDriver(bool bInHasDriver) { bHasAIDriver = bInHasDriver; }

	/** Ejects the AI driver (spawns a fleeing NPC), unpossesses the AI controller. Returns the ejected NPC. */
	ADTCharacterBase* EjectAIDriver(AActor* Jacker);

	/**
	 * Simple AI driving helper used by traffic and police controllers.
	 * Steers towards TargetLocation and holds DesiredSpeedKmh (0 = brake to stop).
	 */
	void DriveTowards(const FVector& TargetLocation, float DesiredSpeedKmh);
	void StopDriving();

protected:
	virtual void BeginPlay() override;

	void Throttle(const FInputActionValue& Value);
	void Brake(const FInputActionValue& Value);
	void Steer(const FInputActionValue& Value);
	void HandbrakeStart();
	void HandbrakeStop();
	void Look(const FInputActionValue& Value);
	void RequestExit();

	UFUNCTION()
	void HandleDestroyed(AActor* Killer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTHealthComponent> HealthComponent;

	/** Driver seat. Add more UDTVehicleSeatComponent in BP for passengers. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTVehicleSeatComponent> DriverSeat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UDTVehicleData> VehicleData;

	/** NPC spawned when the player pulls the AI driver out. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle")
	TSubclassOf<ADTCharacterBase> DriverNPCClass;

	UPROPERTY(EditDefaultsOnly, Category = "Wanted")
	TObjectPtr<UDTCrimeData> CarJackCrime;

	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> HandbrakeAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ExitAction;

	/** Camera swings back behind the car after this many seconds without look input. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraRecenterDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraRecenterSpeed = 3.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vehicle")
	bool bHasAIDriver = false;

private:
	float TimeSinceLookInput = 0.f;
};
