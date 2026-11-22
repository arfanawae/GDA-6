#include "AI/Pedestrian/DTPedestrianCharacter.h"
#include "AI/Pedestrian/DTPedestrianAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

ADTPedestrianCharacter::ADTPedestrianCharacter()
{
	AIControllerClass = ADTPedestrianAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 200.f;
}

void ADTPedestrianCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (MeshVariants.Num() > 0)
	{
		if (USkeletalMesh* Variant = MeshVariants[FMath::RandRange(0, MeshVariants.Num() - 1)])
		{
			GetMesh()->SetSkeletalMeshAsset(Variant);
		}
	}
}
