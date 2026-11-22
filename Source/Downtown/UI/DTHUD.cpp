#include "UI/DTHUD.h"
#include "UI/DTHUDWidget.h"
#include "Downtown.h"

void ADTHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!HUDWidgetClass)
	{
		UE_LOG(LogDowntown, Warning, TEXT("ADTHUD: HUDWidgetClass not set, no HUD will be shown."));
		return;
	}
	HUDWidget = CreateWidget<UDTHUDWidget>(GetOwningPlayerController(), HUDWidgetClass);
	HUDWidget->AddToViewport();
}
