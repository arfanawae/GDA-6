#include "Vehicle/DTVehicleEntryComponent.h"
#include "Vehicle/DTVehicleBase.h"
#include "Vehicle/DTVehicleSeatComponent.h"
#include "Vehicle/DTVehicleData.h"
#include "Character/DTCharacterBase.h"
#include "Combat/DTWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "MotionWarpingComponent.h"
#include "Engine/World.h"

UDTVehicleEntryComponent::UDTVehicleEntryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDTVehicleEntryComponent::SetState(EDTVehicleEntryState NewState)
{
	State = NewState;
	OnStateChanged.Broadcast(State, CurrentVehicle);
}

bool UDTVehicleEntryComponent::PlayMontage(UAnimMontage* Montage, void (UDTVehicleEntryComponent::*OnFinished)())
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Montage || !Anim || Anim->Montage_Play(Montage) <= 0.f)
	{
		return false;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindWeakLambda(this, [this, OnFinished](UAnimMontage*, bool)
	{
		(this->*OnFinished)();
	});
	Anim->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

bool UDTVehicleEntryComponent::RequestEnter(ADTVehicleBase* Vehicle)
{
	ADTCharacterBase* Character = Cast<ADTCharacterBase>(GetOwner());
	if (!Vehicle || !Character || State != EDTVehicleEntryState::None || Character->IsDead())
	{
		return false;
	}

	UDTVehicleSeatComponent* Seat = Vehicle->FindSeatForEntry(Character->GetActorLocation());
	if (!Seat || FVector::Dist(Seat->GetDoorWorldLocation(), Character->GetActorLocation()) > MaxEnterDistance)
	{
		return false;
	}

	CurrentVehicle = Vehicle;
	CurrentSeat = Seat;
	Seat->SetOccupant(Character); // reserve the seat immediately
	SetState(EDTVehicleEntryState::Entering);

	if (UDTWeaponComponent* Weapons = Character->FindComponentByClass<UDTWeaponComponent>())
	{
		Weapons->SetWeaponHidden(true);
	}

	// Warp the character to the door so the montage lines up regardless of approach angle.
	if (UMotionWarpingComponent* Warp = Character->FindComponentByClass<UMotionWarpingComponent>())
	{
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, Seat->GetDoorWorldLocation(), Seat->GetDoorFacingRotation());
	}

	const bool bCarJack = Vehicle->HasAIDriver();
	if (bCarJack)
	{
		Vehicle->EjectAIDriver(Character);
	}

	if (!PlayMontage(bCarJack && CarJackMontage ? CarJackMontage.Get() : EnterMontage.Get(), &UDTVehicleEntryComponent::FinishEnter))
	{
		FinishEnter(); // no animation yet: snap in
	}
	return true;
}

void UDTVehicleEntryComponent::SetCharacterInVehicleMode(bool bInVehicle)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	Character->GetCapsuleComponent()->SetCollisionEnabled(bInVehicle ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	Character->SetActorEnableCollision(!bInVehicle);
	if (bInVehicle)
	{
		Character->GetCharacterMovement()->DisableMovement();
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	else
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void UDTVehicleEntryComponent::FinishEnter()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!CurrentVehicle || !CurrentSeat || State != EDTVehicleEntryState::Entering)
	{
		return;
	}

	SetCharacterInVehicleMode(true);
	Character->AttachToComponent(CurrentSeat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		const FRotator ControlRotation = PC->GetControlRotation();
		PC->Possess(CurrentVehicle);
		PC->SetControlRotation(ControlRotation);
		// Possess() snaps the view; restore and blend for a seamless transition.
		PC->SetViewTarget(Character);
		PC->SetViewTargetWithBlend(CurrentVehicle, CameraBlendTime, VTBlend_EaseInOut, 2.f);
	}

	SetState(EDTVehicleEntryState::Seated);
}

FVector UDTVehicleEntryComponent::FindExitLocation() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTVehicleExit), false, Character);
	Params.AddIgnoredActor(CurrentVehicle);

	const FVector DoorSide = CurrentSeat->GetExitWorldLocation();
	// Mirror the exit point across the vehicle's centre line for the passenger side.
	const FVector Center = CurrentVehicle->GetActorLocation();
	const FVector Right = CurrentVehicle->GetActorRightVector();
	const FVector OtherSide = DoorSide - Right * 2.f * FVector::DotProduct(DoorSide - Center, Right);
	const FVector Roof = Center + FVector(0.f, 0.f, 250.f);

	for (const FVector& Candidate : { DoorSide, OtherSide })
	{
		if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			return Candidate;
		}
	}
	return Roof; // blocked both sides: climb out on top
}

bool UDTVehicleEntryComponent::RequestExit()
{
	ADTCharacterBase* Character = Cast<ADTCharacterBase>(GetOwner());
	if (!Character || !CurrentVehicle || State != EDTVehicleEntryState::Seated)
	{
		return false;
	}

	const FVector ExitLocation = FindExitLocation();
	const FRotator ExitRotation(0.f, CurrentVehicle->GetActorRotation().Yaw, 0.f);

	const float SafeSpeed = CurrentVehicle->GetVehicleData() ? CurrentVehicle->GetVehicleData()->SafeExitSpeedKmh : 15.f;
	const bool bBailOut = FMath::Abs(CurrentVehicle->GetSpeedKmh()) > SafeSpeed;

	APlayerController* PC = Cast<APlayerController>(CurrentVehicle->GetController());
	if (!bBailOut)
	{
		CurrentVehicle->StopDriving();
	}

	SetState(EDTVehicleEntryState::Exiting);

	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocationAndRotation(ExitLocation, ExitRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetCharacterInVehicleMode(false);

	if (PC)
	{
		const FRotator ControlRotation = PC->GetControlRotation();
		PC->Possess(Character);
		PC->SetControlRotation(ControlRotation);
		PC->SetViewTarget(CurrentVehicle);
		PC->SetViewTargetWithBlend(Character, CameraBlendTime, VTBlend_EaseInOut, 2.f);
	}

	if (bBailOut)
	{
		const FVector Side = (ExitLocation - CurrentVehicle->GetActorLocation()).GetSafeNormal2D();
		Character->LaunchCharacter(CurrentVehicle->GetVelocity() * 0.5f + Side * 400.f + FVector(0.f, 0.f, 200.f), true, true);
	}

	if (!PlayMontage(bBailOut ? BailOutMontage.Get() : ExitMontage.Get(), &UDTVehicleEntryComponent::FinishExit))
	{
		FinishExit();
	}
	return true;
}

void UDTVehicleEntryComponent::FinishExit()
{
	if (State != EDTVehicleEntryState::Exiting)
	{
		return;
	}
	if (CurrentSeat)
	{
		CurrentSeat->SetOccupant(nullptr);
	}
	if (UDTWeaponComponent* Weapons = GetOwner()->FindComponentByClass<UDTWeaponComponent>())
	{
		Weapons->SetWeaponHidden(false);
	}

	ADTVehicleBase* OldVehicle = CurrentVehicle;
	CurrentVehicle = nullptr;
	CurrentSeat = nullptr;
	State = EDTVehicleEntryState::None;
	OnStateChanged.Broadcast(State, OldVehicle);
}
