#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTMapIconComponent.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EDTMapIconType : uint8
{
	Player,
	Police,
	PoliceVehicle,
	Vehicle,
	Objective,
	Custom
};

/** Add to any actor that should appear on the minimap. Registers itself with UDTMinimapSubsystem. */
UCLASS(ClassGroup = (Downtown), meta = (BlueprintSpawnableComponent))
class DOWNTOWN_API UDTMapIconComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDTMapIconComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	EDTMapIconType IconType = EDTMapIconType::Custom;

	/** Optional override; WBP_MapIcon picks a default per IconType when null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FLinearColor Tint = FLinearColor::White;

	/** Rotate the icon with the actor's yaw (player arrow, cars). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	bool bRotateWithOwner = false;

	/** Keep the icon on the minimap edge when out of range (objectives). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	bool bClampToEdge = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
