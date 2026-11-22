#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "DTPoliceAIController.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UENUM(BlueprintType)
enum class EDTPoliceState : uint8
{
	Idle,
	Chase,
	Attack,
	Search
};

/**
 * Officer on foot: Chase the player when seen, Attack in range (only when lethal force is allowed),
 * otherwise Search around the last known location. Reports sightings to UDTWantedSubsystem.
 */
UCLASS()
class DOWNTOWN_API ADTPoliceAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADTPoliceAIController();

	UFUNCTION(BlueprintPure, Category = "Police")
	EDTPoliceState GetState() const { return State; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);

	void Think();
	void SetFiring(bool bFire);
	bool IsPlayerPawn(const AActor* Actor) const;

	UPROPERTY(VisibleAnywhere, Category = "AI") TObjectPtr<UAISenseConfig_Sight> SightConfig;
	UPROPERTY(VisibleAnywhere, Category = "AI") TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Police") float AttackRange = 2000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Police") float ChaseAcceptance = 500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Police") float ThinkInterval = 0.25f;

	/** Short bursts feel fairer than a laser: fire for BurstTime, pause for BurstPause. */
	UPROPERTY(EditDefaultsOnly, Category = "Police") float BurstTime = 0.8f;
	UPROPERTY(EditDefaultsOnly, Category = "Police") float BurstPause = 1.2f;

	TWeakObjectPtr<AActor> Target;
	bool bTargetVisible = false;
	bool bFiring = false;
	float BurstClock = 0.f;
	EDTPoliceState State = EDTPoliceState::Idle;
	FTimerHandle ThinkTimer;
};
