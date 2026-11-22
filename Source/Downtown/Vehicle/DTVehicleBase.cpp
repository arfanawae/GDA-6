#include "Vehicle/DTVehicleBase.h"
#include "Vehicle/DTVehicleSeatComponent.h"
#include "Vehicle/DTVehicleData.h"
#include "Vehicle/DTVehicleEntryComponent.h"
#include "Character/DTCharacterBase.h"
#include "Combat/DTHealthComponent.h"
#include "Wanted/DTWantedSubsystem.h"
#include "ChaosVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "AIController.h"

ADTVehicleBase::ADTVehicleBase()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 650.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 200.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->CameraRotationLagSpeed = 6.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	HealthComponent = CreateDefaultSubobject<UDTHealthComponent>(TEXT("Health"));

	DriverSeat = CreateDefaultSubobject<UDTVehicleSeatComponent>(TEXT("DriverSeat"));
	DriverSeat->SetupAttachment(GetMesh());
	DriverSeat->SeatType = EDTSeatType::Driver;

	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ADTVehicleBase::BeginPlay()
{
	Super::BeginPlay();
	HealthComponent->OnDeath.AddDynamic(this, &ADTVehicleBase::HandleDestroyed);
}

void ADTVehicleBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	if (ThrottleAction)
	{
		Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ADTVehicleBase::Throttle);
		Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ADTVehicleBase::Throttle);
	}
	if (BrakeAction)
	{
		Input->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ADTVehicleBase::Brake);
		Input->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ADTVehicleBase::Brake);
	}
	if (SteerAction)
	{
		Input->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ADTVehicleBase::Steer);
		Input->BindAction(SteerAction, ETriggerEvent::Completed, this, &ADTVehicleBase::Steer);
	}
	if (HandbrakeAction)
	{
		Input->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ADTVehicleBase::HandbrakeStart);
		Input->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ADTVehicleBase::HandbrakeStop);
	}
	if (LookAction) Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADTVehicleBase::Look);
	if (ExitAction) Input->BindAction(ExitAction, ETriggerEvent::Started, this, &ADTVehicleBase::RequestExit);
}

void ADTVehicleBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Recenter the chase camera behind the car when the player stops looking around.
	if (IsPlayerControlled())
	{
		TimeSinceLookInput += DeltaSeconds;
		if (TimeSinceLookInput > CameraRecenterDelay && GetSpeedKmh() > 5.f)
		{
			const FRotator Current = Controller->GetControlRotation();
			const FRotator Target(Current.Pitch, GetActorRotation().Yaw, 0.f);
			Controller->SetControlRotation(FMath::RInterpTo(Current, Target, DeltaSeconds, CameraRecenterSpeed));
		}
	}
}

void ADTVehicleBase::Throttle(const FInputActionValue& Value)  { GetVehicleMovementComponent()->SetThrottleInput(Value.Get<float>()); }
void ADTVehicleBase::Brake(const FInputActionValue& Value)     { GetVehicleMovementComponent()->SetBrakeInput(Value.Get<float>()); }
void ADTVehicleBase::Steer(const FInputActionValue& Value)     { GetVehicleMovementComponent()->SetSteeringInput(Value.Get<float>()); }
void ADTVehicleBase::HandbrakeStart()                          { GetVehicleMovementComponent()->SetHandbrakeInput(true); }
void ADTVehicleBase::HandbrakeStop()                           { GetVehicleMovementComponent()->SetHandbrakeInput(false); }

void ADTVehicleBase::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
	TimeSinceLookInput = 0.f;
}

void ADTVehicleBase::RequestExit()
{
	ADTCharacterBase* Driver = DriverSeat->GetOccupant();
	if (UDTVehicleEntryComponent* Entry = Driver ? Driver->FindComponentByClass<UDTVehicleEntryComponent>() : nullptr)
	{
		Entry->RequestExit();
	}
}

float ADTVehicleBase::GetSpeedKmh() const
{
	return GetVehicleMovementComponent()->GetForwardSpeed() * 0.036f; // cm/s -> km/h
}

UDTVehicleSeatComponent* ADTVehicleBase::GetDriverSeat() const
{
	return DriverSeat;
}

UDTVehicleSeatComponent* ADTVehicleBase::FindSeatForEntry(const FVector& FromLocation) const
{
	// Phase 1: driver seat only. The player always takes the wheel (carjacking an AI driver if needed).
	return DriverSeat->IsOccupied() ? nullptr : DriverSeat.Get();
}

bool ADTVehicleBase::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return !HealthComponent->IsDead() && !DriverSeat->IsOccupied() && FMath::Abs(GetSpeedKmh()) < 30.f;
}

void ADTVehicleBase::Interact_Implementation(AActor* InteractInstigator)
{
	if (UDTVehicleEntryComponent* Entry = InteractInstigator ? InteractInstigator->FindComponentByClass<UDTVehicleEntryComponent>() : nullptr)
	{
		Entry->RequestEnter(this);
	}
}

FText ADTVehicleBase::GetInteractionPrompt_Implementation() const
{
	return bHasAIDriver ? NSLOCTEXT("Downtown", "Carjack", "Hijack vehicle") : NSLOCTEXT("Downtown", "EnterVehicle", "Enter vehicle");
}

ADTCharacterBase* ADTVehicleBase::EjectAIDriver(AActor* Jacker)
{
	if (!bHasAIDriver)
	{
		return nullptr;
	}
	bHasAIDriver = false;

	if (AController* AI = GetController(); AI && !AI->IsPlayerController())
	{
		AI->UnPossess();
		AI->Destroy();
	}
	StopDriving();

	ADTCharacterBase* Ejected = nullptr;
	if (DriverNPCClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Ejected = GetWorld()->SpawnActor<ADTCharacterBase>(DriverNPCClass, DriverSeat->GetExitWorldLocation(), GetActorRotation(), Params);
		// The pedestrian AI hears/sees the threat and flees on its own.
	}

	if (UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>())
	{
		Wanted->ReportCrime(CarJackCrime, GetActorLocation(), Jacker);
	}
	return Ejected;
}

void ADTVehicleBase::DriveTowards(const FVector& TargetLocation, float DesiredSpeedKmh)
{
	UChaosVehicleMovementComponent* Move = GetVehicleMovementComponent();
	const float MaxSteer = VehicleData ? VehicleData->MaxSteerAngleDegrees : 40.f;

	// Steering: angle to the target in local space.
	const FVector Local = GetActorTransform().InverseTransformPosition(TargetLocation);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	Move->SetSteeringInput(FMath::Clamp(AngleDeg / MaxSteer, -1.f, 1.f));

	// Speed: simple proportional controller, slow down for sharp turns.
	const float TurnFactor = FMath::GetMappedRangeValueClamped(FVector2D(10.f, 60.f), FVector2D(1.f, 0.4f), FMath::Abs(AngleDeg));
	const float Target = DesiredSpeedKmh * TurnFactor;
	const float Speed = GetSpeedKmh();
	const float Error = Target - Speed;

	if (Target <= 0.5f)
	{
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(1.f);
	}
	else if (Error > 0.f)
	{
		Move->SetThrottleInput(FMath::Clamp(Error / 10.f, 0.2f, 1.f));
		Move->SetBrakeInput(0.f);
	}
	else
	{
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(FMath::Clamp(-Error / 15.f, 0.f, 1.f));
	}
	Move->SetHandbrakeInput(false);
}

void ADTVehicleBase::StopDriving()
{
	UChaosVehicleMovementComponent* Move = GetVehicleMovementComponent();
	Move->SetThrottleInput(0.f);
	Move->SetSteeringInput(0.f);
	Move->SetBrakeInput(1.f);
}

float ADTVehicleBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	HealthComponent->ApplyDamage(Actual, EventInstigator ? EventInstigator->GetPawn() : DamageCauser);
	return Actual;
}

void ADTVehicleBase::HandleDestroyed(AActor* Killer)
{
	// Phase 1: disable the car. Explosion/fire FX are added in BP (bind OnDeath on the Health component).
	StopDriving();
	RequestExit();
}
