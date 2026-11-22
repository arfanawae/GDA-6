#include "Core/DTPlayerController.h"
#include "Character/DTPlayerCharacter.h"
#include "Vehicle/DTVehicleBase.h"
#include "Downtown.h"
#include "UI/DTWorldMapWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"

void ADTPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ADTPlayerCharacter* PlayerChar = Cast<ADTPlayerCharacter>(InPawn))
	{
		PlayerCharacter = PlayerChar;
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
	if (GlobalContext)
	{
		Subsystem->AddMappingContext(GlobalContext, 1);
	}
	if (InPawn && InPawn->IsA<ADTVehicleBase>())
	{
		if (VehicleContext) Subsystem->AddMappingContext(VehicleContext, 0);
	}
	else
	{
		if (OnFootContext) Subsystem->AddMappingContext(OnFootContext, 0);
	}
}

void ADTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MapAction)
		{
			Input->BindAction(MapAction, ETriggerEvent::Started, this, &ADTPlayerController::ToggleWorldMap);
		}
	}
}

void ADTPlayerController::ToggleWorldMap()
{
	if (bWorldMapOpen)
	{
		bWorldMapOpen = false;
		if (WorldMap)
		{
			WorldMap->RemoveFromParent();
		}
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		if (bPauseWhileMapOpen)
		{
			UGameplayStatics::SetGamePaused(this, false);
		}
		return;
	}

	if (!WorldMapClass)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[WorldMap] WorldMapClass is not set on %s - create WBP_WorldMap (parent DTWorldMapWidget) and assign it."), *GetClass()->GetName());
		return;
	}
	if (!WorldMap)
	{
		WorldMap = CreateWidget<UDTWorldMapWidget>(this, WorldMapClass);
	}
	if (!WorldMap)
	{
		return;
	}
	WorldMap->AddToViewport(50);
	bWorldMapOpen = true;
	WorldMap->CenterOnPlayer();

	// Game input stays active so the same key closes the map again.
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(WorldMap->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	if (bPauseWhileMapOpen)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
}
