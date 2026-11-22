#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DTVehicleSeatComponent.generated.h"

class ADTCharacterBase;

UENUM(BlueprintType)
enum class EDTSeatType : uint8
{
	Driver,
	Passenger
};

/**
 * A seat inside a vehicle. Its transform is where the character's root is attached.
 * DoorOffset / ExitOffset are relative to the seat and mark where the character
 * stands to open the door and where it is placed after exiting.
 */
UCLASS(ClassGroup = (Downtown), meta = (BlueprintSpawnableComponent))
class DOWNTOWN_API UDTVehicleSeatComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Seat")
	FVector GetDoorWorldLocation() const { return GetComponentTransform().TransformPosition(DoorOffset); }

	UFUNCTION(BlueprintPure, Category = "Seat")
	FVector GetExitWorldLocation() const { return GetComponentTransform().TransformPosition(ExitOffset); }

	/** Rotation the character faces while opening the door (towards the seat). */
	FRotator GetDoorFacingRotation() const;

	UFUNCTION(BlueprintPure, Category = "Seat")
	bool IsOccupied() const { return Occupant != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Seat")
	ADTCharacterBase* GetOccupant() const { return Occupant; }

	void SetOccupant(ADTCharacterBase* NewOccupant) { Occupant = NewOccupant; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	EDTSeatType SeatType = EDTSeatType::Driver;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat", meta = (MakeEditWidget))
	FVector DoorOffset = FVector(0.f, -120.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat", meta = (MakeEditWidget))
	FVector ExitOffset = FVector(0.f, -150.f, 50.f);

	/** Montage section / variant selection hook for left vs right side doors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	bool bLeftSide = true;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ADTCharacterBase> Occupant;
};
