#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTMinimapSubsystem.generated.h"

class UDTMapIconComponent;

/**
 * Registry of map icons + world <-> map UV conversion for the pre-baked top-down
 * minimap texture (T_MinimapTexture). Bounds come from ADTGameMode::MinimapWorldBounds.
 */
UCLASS()
class DOWNTOWN_API UDTMinimapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void RegisterIcon(UDTMapIconComponent* Icon) { Icons.AddUnique(Icon); }
	void UnregisterIcon(UDTMapIconComponent* Icon) { Icons.Remove(Icon); }

	UFUNCTION(BlueprintPure, Category = "Minimap")
	TArray<UDTMapIconComponent*> GetIcons() const;

	/** World XY -> 0..1 UV on the minimap texture (X = right, Y = down). */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	FVector2D WorldToMapUV(const FVector& WorldLocation) const;

	/** World distance (cm) -> UV distance, for drawing the police search radius. */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	float WorldDistanceToUV(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "Minimap")
	FBox2D GetWorldBounds() const { return WorldBounds; }

private:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UDTMapIconComponent>> Icons;

	FBox2D WorldBounds = FBox2D(FVector2D(-50000.f), FVector2D(50000.f));
};
