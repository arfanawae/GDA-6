#include "UI/DTMinimapSubsystem.h"
#include "UI/DTMapIconComponent.h"
#include "Core/DTGameMode.h"

void UDTMinimapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (const ADTGameMode* GM = InWorld.GetAuthGameMode<ADTGameMode>())
	{
		WorldBounds = GM->MinimapWorldBounds;
	}
}

TArray<UDTMapIconComponent*> UDTMinimapSubsystem::GetIcons() const
{
	TArray<UDTMapIconComponent*> Result;
	Result.Reserve(Icons.Num());
	for (const TWeakObjectPtr<UDTMapIconComponent>& Icon : Icons)
	{
		if (Icon.IsValid())
		{
			Result.Add(Icon.Get());
		}
	}
	return Result;
}

FVector2D UDTMinimapSubsystem::WorldToMapUV(const FVector& WorldLocation) const
{
	// Top-down capture: world +X = up on the map, world +Y = right on the map.
	const FVector2D Size = WorldBounds.GetSize();
	const float U = (WorldLocation.Y - WorldBounds.Min.Y) / Size.Y;
	const float V = 1.f - (WorldLocation.X - WorldBounds.Min.X) / Size.X;
	return FVector2D(U, V);
}

float UDTMinimapSubsystem::WorldDistanceToUV(float Distance) const
{
	return Distance / FMath::Max(WorldBounds.GetSize().X, 1.f);
}
