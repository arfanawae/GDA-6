#include "UI/DTMapIconComponent.h"
#include "UI/DTMinimapSubsystem.h"

UDTMapIconComponent::UDTMapIconComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDTMapIconComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UDTMinimapSubsystem* Minimap = GetWorld()->GetSubsystem<UDTMinimapSubsystem>())
	{
		Minimap->RegisterIcon(this);
	}
}

void UDTMapIconComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDTMinimapSubsystem* Minimap = GetWorld()->GetSubsystem<UDTMinimapSubsystem>())
	{
		Minimap->UnregisterIcon(this);
	}
	Super::EndPlay(EndPlayReason);
}
