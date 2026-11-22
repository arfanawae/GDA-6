#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DTCrimeData.generated.h"

/** One kind of crime (DA_Crime_Shooting, DA_Crime_Murder, DA_Crime_CarJack, DA_Crime_Assault...). */
UCLASS(BlueprintType)
class DOWNTOWN_API UDTCrimeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crime")
	FText DisplayName;

	/** Heat added each time this crime is reported. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crime")
	float Heat = 10.f;

	/** If true, only counts when a pedestrian or police officer can see the offender. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crime")
	bool bRequiresWitness = true;

	/** Minimum stars this crime forces, regardless of heat (e.g. killing police = 2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crime", meta = (ClampMin = "0", ClampMax = "3"))
	int32 MinimumStars = 0;

	/** Repeat reports of this crime within this window are ignored (e.g. one report per burst of gunfire). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crime")
	float ReportCooldown = 2.f;
};
