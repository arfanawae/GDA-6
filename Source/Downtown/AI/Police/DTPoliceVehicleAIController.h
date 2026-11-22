#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DTPoliceVehicleAIController.generated.h"

class ADTCharacterBase;

/**
 * Drives a police cruiser towards the player (or last known location). When close to a player
 * on foot it stops and deploys officers. Rams a player vehicle at 3 stars.
 */
UCLASS()
class DOWNTOWN_API ADTPoliceVehicleAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADTPoliceVehicleAIController();
	virtual void Tick(float DeltaSeconds) override;

protected:
	void DeployOfficers();

	UPROPERTY(EditDefaultsOnly, Category = "Police") float PursuitSpeedKmh = 110.f;
	UPROPERTY(EditDefaultsOnly, Category = "Police") float SearchSpeedKmh = 60.f;
	UPROPERTY(EditDefaultsOnly, Category = "Police") float DeployDistance = 1500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Police") int32 OfficersPerCar = 2;

	/** Leave this much space when following a player vehicle (unless ramming). */
	UPROPERTY(EditDefaultsOnly, Category = "Police") float FollowGap = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Police") int32 RamAtStars = 3;

	bool bOfficersDeployed = false;
};
