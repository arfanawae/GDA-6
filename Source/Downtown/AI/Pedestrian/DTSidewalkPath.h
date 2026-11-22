#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTSidewalkPath.generated.h"

class USplineComponent;

/** A walkable sidewalk/crosswalk spline. Pedestrians walk it in either direction and hop to connected paths at the ends. */
UCLASS()
class DOWNTOWN_API ADTSidewalkPath : public AActor
{
	GENERATED_BODY()

public:
	ADTSidewalkPath();

	UFUNCTION(BlueprintPure, Category = "Pedestrian")
	USplineComponent* GetSpline() const { return Spline; }

	/** A random path connected near the given end (bAtEnd = end of spline, else start). */
	ADTSidewalkPath* PickConnectedPath(bool bAtEnd, bool& bOutStartFromEnd) const;

	/** Crosswalks: pedestrians wait until this is false. Hook up to a traffic light in BP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedestrian")
	bool bBlocked = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(EditAnywhere, Category = "Pedestrian")
	TArray<TObjectPtr<ADTSidewalkPath>> ConnectedPaths;

	/** Connected path endpoints within this distance count as joined. */
	UPROPERTY(EditAnywhere, Category = "Pedestrian")
	float ConnectionTolerance = 300.f;
};
