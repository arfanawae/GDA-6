#pragma once

#include "CoreMinimal.h"
#include "Character/DTCharacterBase.h"
#include "DTPedestrianCharacter.generated.h"

class ADTSidewalkPath;
class USkeletalMesh;

/** Ambient NPC. Randomizes its look on spawn; behaviour lives in ADTPedestrianAIController. */
UCLASS()
class DOWNTOWN_API ADTPedestrianCharacter : public ADTCharacterBase
{
	GENERATED_BODY()

public:
	ADTPedestrianCharacter();

	void SetStartPath(ADTSidewalkPath* Path, float Distance) { StartPath = Path; StartDistance = Distance; }
	ADTSidewalkPath* GetStartPath() const { return StartPath; }
	float GetStartDistance() const { return StartDistance; }

	UPROPERTY(EditDefaultsOnly, Category = "Pedestrian") float WalkSpeed = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Pedestrian") float RunSpeed = 550.f;

protected:
	virtual void BeginPlay() override;

	/** One is picked at random on spawn. */
	UPROPERTY(EditDefaultsOnly, Category = "Pedestrian")
	TArray<TObjectPtr<USkeletalMesh>> MeshVariants;

	UPROPERTY(Transient) TObjectPtr<ADTSidewalkPath> StartPath;
	float StartDistance = 0.f;
};
