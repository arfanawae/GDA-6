#include "Vehicle/DTVehicleSeatComponent.h"

FRotator UDTVehicleSeatComponent::GetDoorFacingRotation() const
{
	const FVector ToSeat = GetComponentLocation() - GetDoorWorldLocation();
	return FRotator(0.f, ToSeat.Rotation().Yaw, 0.f);
}
