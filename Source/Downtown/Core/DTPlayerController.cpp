#include "Core/DTPlayerController.h"
#include "Character/DTPlayerCharacter.h"
#include "Vehicle/DTVehicleBase.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void ADTPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ADTPlayerCharacter* Character = Cast<ADTPlayerCharacter>(InPawn))
	{
		PlayerCharacter = Character;
	}
	ApplyMappingContexts(InPawn);
}

void ADTPlayerController::ApplyMappingContexts(APawn* InPawn)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	Subsystem->ClearAllMappings();
	if (InPawn && InPawn->IsA<ADTVehicleBase>())
	{
		if (VehicleContext) Subsystem->AddMappingContext(VehicleContext, 0);
	}
	else
	{
		if (OnFootContext) Subsystem->AddMappingContext(OnFootContext, 0);
	}
}
