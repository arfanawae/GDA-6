#include "Character/DTPlayerCharacter.h"
#include "Combat/DTWeaponComponent.h"
#include "Interaction/DTInteractionComponent.h"
#include "Vehicle/DTVehicleEntryComponent.h"
#include "Wanted/DTWantedSubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"

ADTPlayerCharacter::ADTPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->JumpZVelocity = 500.f;
	Move->AirControl = 0.3f;
	Move->MaxWalkSpeed = WalkSpeed;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->SocketOffset = DefaultSocketOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 15.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	WeaponComponent = CreateDefaultSubobject<UDTWeaponComponent>(TEXT("Weapons"));
	InteractionComponent = CreateDefaultSubobject<UDTInteractionComponent>(TEXT("Interaction"));
	VehicleEntryComponent = CreateDefaultSubobject<UDTVehicleEntryComponent>(TEXT("VehicleEntry"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
}

void ADTPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	WeaponComponent->OnWeaponFired.AddDynamic(this, &ADTPlayerCharacter::HandleWeaponFired);
}

void ADTPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	if (MoveAction)   Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADTPlayerCharacter::Move);
	if (LookAction)   Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADTPlayerCharacter::Look);
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (SprintAction)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ADTPlayerCharacter::StartSprint);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADTPlayerCharacter::StopSprint);
	}
	if (AimAction)
	{
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &ADTPlayerCharacter::StartAim);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &ADTPlayerCharacter::StopAim);
	}
	if (FireAction)
	{
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &ADTPlayerCharacter::StartFire);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ADTPlayerCharacter::StopFire);
	}
	if (ReloadAction)       Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ADTPlayerCharacter::Reload);
	if (SwitchWeaponAction) Input->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &ADTPlayerCharacter::SwitchWeapon);
	if (InteractAction)     Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ADTPlayerCharacter::Interact);
}

void ADTPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Smooth over-the-shoulder zoom while aiming.
	const float TargetLength = bAiming ? AimArmLength : DefaultArmLength;
	const FVector TargetOffset = bAiming ? AimSocketOffset : DefaultSocketOffset;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetLength, DeltaSeconds, CameraInterpSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetOffset, DeltaSeconds, CameraInterpSpeed);
}

static bool IsBusy(const UDTVehicleEntryComponent* Entry)
{
	return Entry && Entry->IsBusy();
}

void ADTPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || IsDead() || IsBusy(VehicleEntryComponent))
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADTPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ADTPlayerCharacter::StartSprint() { bSprinting = true;  UpdateMovementMode(); }
void ADTPlayerCharacter::StopSprint()  { bSprinting = false; UpdateMovementMode(); }

void ADTPlayerCharacter::StartAim()
{
	bAiming = true;
	WeaponComponent->SetAiming(true);
	UpdateMovementMode();
}

void ADTPlayerCharacter::StopAim()
{
	bAiming = false;
	WeaponComponent->SetAiming(false);
	UpdateMovementMode();
}

void ADTPlayerCharacter::UpdateMovementMode()
{
	// Aiming: strafe and face the camera. Otherwise: face the movement direction.
	bUseControllerRotationYaw = bAiming;
	GetCharacterMovement()->bOrientRotationToMovement = !bAiming;
	GetCharacterMovement()->MaxWalkSpeed = bAiming ? AimWalkSpeed : (bSprinting ? SprintSpeed : WalkSpeed);
}

void ADTPlayerCharacter::StartFire()
{
	if (!IsDead() && !IsBusy(VehicleEntryComponent) && !bSprinting)
	{
		WeaponComponent->StartFire();
	}
}

void ADTPlayerCharacter::StopFire()     { WeaponComponent->StopFire(); }
void ADTPlayerCharacter::Reload()       { WeaponComponent->Reload(); }
void ADTPlayerCharacter::SwitchWeapon() { WeaponComponent->CycleWeapon(); }

void ADTPlayerCharacter::Interact()
{
	if (!IsDead() && !IsBusy(VehicleEntryComponent))
	{
		InteractionComponent->TryInteract();
	}
}

void ADTPlayerCharacter::HandleWeaponFired(ADTWeaponBase* Weapon, FVector Location)
{
	if (UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>())
	{
		Wanted->ReportCrime(ShootingCrime, Location, this);
	}
}
