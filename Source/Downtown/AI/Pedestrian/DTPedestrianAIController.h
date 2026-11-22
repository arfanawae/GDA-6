#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "DTPedestrianAIController.generated.h"

class ADTSidewalkPath;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UENUM(BlueprintType)
enum class EDTPedestrianState : uint8
{
	Wander,
	Flee,
	Cower
};

/**
 * Pedestrian brain (lightweight state machine, swap for BT_Pedestrian later if it grows):
 * Wander along sidewalk splines -> Flee on gunshots / corpses / damage -> Cower when cornered -> back to Wander.
 */
UCLASS()
class DOWNTOWN_API ADTPedestrianAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADTPedestrianAIController();

	UFUNCTION(BlueprintPure, Category = "Pedestrian")
	EDTPedestrianState GetState() const { return State; }

	/** Start fleeing from a threat location (also callable from gameplay, e.g. a car mounting the curb). */
	UFUNCTION(BlueprintCallable, Category = "Pedestrian")
	void Panic(FVector ThreatLocation);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleDamaged(class UDTHealthComponent* Health, float NewHealth, float Delta);

	void MoveToNextWaypoint();
	void StartFlee();
	void EndFlee();
	void SnapToNearestPath();
	void SetSpeed(bool bRun);

	UPROPERTY(VisibleAnywhere, Category = "AI") TObjectPtr<UAISenseConfig_Sight> SightConfig;
	UPROPERTY(VisibleAnywhere, Category = "AI") TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Pedestrian") float WaypointSpacing = 400.f;
	UPROPERTY(EditDefaultsOnly, Category = "Pedestrian") float FleeDistance = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Pedestrian") float CalmDownTime = 10.f;

	UPROPERTY(Transient) TObjectPtr<ADTSidewalkPath> CurrentPath;
	float PathDistance = 0.f;
	bool bForward = true;

	EDTPedestrianState State = EDTPedestrianState::Wander;
	FVector ThreatLocation = FVector::ZeroVector;
	FTimerHandle CalmTimer;
};
