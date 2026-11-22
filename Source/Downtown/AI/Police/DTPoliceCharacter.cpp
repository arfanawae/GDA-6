#include "AI/Police/DTPoliceCharacter.h"
#include "AI/Police/DTPoliceAIController.h"
#include "Combat/DTWeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADTPoliceCharacter::ADTPoliceCharacter()
{
	WeaponComponent = CreateDefaultSubobject<UDTWeaponComponent>(TEXT("Weapons"));

	AIControllerClass = ADTPoliceAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = true; // face focus target while shooting
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}
