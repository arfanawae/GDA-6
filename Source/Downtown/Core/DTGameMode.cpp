#include "Core/DTGameMode.h"
#include "Core/DTPlayerController.h"
#include "Character/DTPlayerCharacter.h"
#include "UI/DTHUD.h"

ADTGameMode::ADTGameMode()
{
	DefaultPawnClass = ADTPlayerCharacter::StaticClass();
	PlayerControllerClass = ADTPlayerController::StaticClass();
	HUDClass = ADTHUD::StaticClass();
}
