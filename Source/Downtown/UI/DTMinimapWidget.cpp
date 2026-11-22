#include "UI/DTMinimapWidget.h"
#include "UI/DTMinimapSubsystem.h"
#include "UI/DTMapIconComponent.h"
#include "Vehicle/DTVehicleBase.h"
#include "Wanted/DTWantedSubsystem.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

void UDTMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CurrentZoom = ZoomOnFoot;
	if (MapImage)
	{
		MapMaterial = MapImage->GetDynamicMaterial();
	}
}

FVector2D UDTMinimapWidget::MapUVToWidget(const FVector2D& UV, const FVector2D& CenterUV, float HalfSizePx) const
{
	// UV offset from the centre -> pixels, then rotate opposite to the map rotation.
	const FVector2D Offset = (UV - CenterUV) / CurrentZoom * (HalfSizePx * 2.f);
	return Offset.GetRotated(-CurrentRotation);
}

void UDTMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UDTMinimapSubsystem* Minimap = GetWorld()->GetSubsystem<UDTMinimapSubsystem>();
	const APawn* Pawn = GetOwningPlayerPawn();
	if (!Minimap || !Pawn)
	{
		return;
	}

	// Zoom out with speed.
	float TargetZoom = ZoomOnFoot;
	if (const ADTVehicleBase* Vehicle = Cast<ADTVehicleBase>(Pawn))
	{
		const float Alpha = FMath::Clamp(FMath::Abs(Vehicle->GetSpeedKmh()) / MaxZoomSpeedKmh, 0.f, 1.f);
		TargetZoom = FMath::Lerp(ZoomOnFoot, ZoomAtMaxSpeed, Alpha);
	}
	CurrentZoom = FMath::FInterpTo(CurrentZoom, TargetZoom, InDeltaTime, ZoomInterpSpeed);

	float CameraYaw = 0.f;
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		CameraYaw = Camera->GetCameraRotation().Yaw;
	}
	CurrentRotation = bRotateWithCamera ? CameraYaw : 0.f;

	const FVector2D CenterUV = Minimap->WorldToMapUV(Pawn->GetActorLocation());
	if (MapMaterial)
	{
		MapMaterial->SetScalarParameterValue(TEXT("CenterU"), CenterUV.X);
		MapMaterial->SetScalarParameterValue(TEXT("CenterV"), CenterUV.Y);
		MapMaterial->SetScalarParameterValue(TEXT("Zoom"), CurrentZoom);
		MapMaterial->SetScalarParameterValue(TEXT("Rotation"), CurrentRotation / 360.f);
	}

	const float HalfSize = MyGeometry.GetLocalSize().X * 0.5f;

	IconViews.Reset();
	for (UDTMapIconComponent* Icon : Minimap->GetIcons())
	{
		const AActor* Owner = Icon->GetOwner();
		if (!Owner || Owner->IsHidden())
		{
			continue;
		}
		FDTMinimapIconView View;
		View.Icon = Icon;
		View.Position = MapUVToWidget(Minimap->WorldToMapUV(Owner->GetActorLocation()), CenterUV, HalfSize);
		View.Angle = Icon->bRotateWithOwner ? Owner->GetActorRotation().Yaw - CurrentRotation : 0.f;

		const float Len = View.Position.Size();
		View.bVisible = Len <= HalfSize || Icon->bClampToEdge;
		if (Len > HalfSize && Icon->bClampToEdge)
		{
			View.Position = View.Position / Len * HalfSize;
		}
		IconViews.Add(View);
	}

	// Police search area.
	SearchRadius = 0.f;
	if (const UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>(); Wanted && Wanted->GetWantedLevel() > 0 && !Wanted->IsPlayerSeen())
	{
		SearchCenter = MapUVToWidget(Minimap->WorldToMapUV(Wanted->GetLastKnownLocation()), CenterUV, HalfSize);
		SearchRadius = Minimap->WorldDistanceToUV(Wanted->GetSearchRadius()) / CurrentZoom * HalfSize * 2.f;
	}
}
