#pragma once

#include "CoreMinimal.h"
#include "Character/DTCharacterBase.h"
#include "DTPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UDTWeaponComponent;
class UDTInteractionComponent;
class UDTVehicleEntryComponent;
class UDTCrimeData;
class UMotionWarpingComponent;
class ADTWeaponBase;
struct FInputActionValue;

/** Third-person on-foot player: walk, sprint, jump, aim, shoot, interact (enter vehicles). */
UCLASS()
class DOWNTOWN_API ADTPlayerCharacter : public ADTCharacterBase
{
	GENERATED_BODY()

public:
	ADTPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Player")
	UDTWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Player")
	UDTInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "Player")
	UDTVehicleEntryComponent* GetVehicleEntryComponent() const { return VehicleEntryComponent; }

	UFUNCTION(BlueprintPure, Category = "Player")
	bool IsSprinting() const { return bSprinting; }

protected:
	virtual void BeginPlay() override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void StartAim();
	void StopAim();
	void StartFire();
	void StopFire();
	void Reload();
	void SwitchWeapon();
	void Interact();
	void UpdateMovementMode();

	UFUNCTION()
	void HandleWeaponFired(ADTWeaponBase* Weapon, FVector Location);

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTVehicleEntryComponent> VehicleEntryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	// Input (assign IA_* assets in BP_PlayerCharacter; the mapping context is handled by ADTPlayerController)
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> AimAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> FireAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> SwitchWeaponAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> InteractAction;

	// Tuning
	UPROPERTY(EditDefaultsOnly, Category = "Movement") float WalkSpeed = 350.f;
	UPROPERTY(EditDefaultsOnly, Category = "Movement") float SprintSpeed = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "Movement") float AimWalkSpeed = 250.f;
	UPROPERTY(EditDefaultsOnly, Category = "Camera") float DefaultArmLength = 350.f;
	UPROPERTY(EditDefaultsOnly, Category = "Camera") float AimArmLength = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Camera") FVector DefaultSocketOffset = FVector(0.f, 50.f, 60.f);
	UPROPERTY(EditDefaultsOnly, Category = "Camera") FVector AimSocketOffset = FVector(0.f, 70.f, 50.f);
	UPROPERTY(EditDefaultsOnly, Category = "Camera") float CameraInterpSpeed = 10.f;

	/** Reported to the wanted system when the player fires a gun. */
	UPROPERTY(EditDefaultsOnly, Category = "Wanted")
	TObjectPtr<UDTCrimeData> ShootingCrime;

private:
	bool bSprinting = false;
	bool bAiming = false;
};
