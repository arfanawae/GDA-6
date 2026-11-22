#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DTMinimapWidget.generated.h"

class UImage;
class UCanvasPanel;
class UMaterialInstanceDynamic;
class UDTMapIconComponent;

USTRUCT(BlueprintType)
struct FDTMinimapIconView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TObjectPtr<UDTMapIconComponent> Icon;
	/** Position relative to the widget centre, in pixels. */
	UPROPERTY(BlueprintReadOnly) FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float Angle = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bVisible = false;
};

/**
 * Base for WBP_Minimap. Expects an Image named "MapImage" using M_MinimapMask with
 * scalar params CenterU, CenterV, Zoom, Rotation. Computes icon positions each frame;
 * the BP places WBP_MapIcon children from GetIconViews().
 */
UCLASS(Abstract)
class DOWNTOWN_API UDTMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Minimap")
	const TArray<FDTMinimapIconView>& GetIconViews() const { return IconViews; }

	/** Police search radius circle: centre (px, relative to widget centre) and radius (px). 0 radius = hide. */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	void GetSearchArea(FVector2D& OutCenter, float& OutRadius) const { OutCenter = SearchCenter; OutRadius = SearchRadius; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	FVector2D MapUVToWidget(const FVector2D& UV, const FVector2D& CenterUV, float HalfSizePx) const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	/** Fraction of the map texture visible on foot; zooms out with vehicle speed. */
	UPROPERTY(EditAnywhere, Category = "Minimap") float ZoomOnFoot = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Minimap") float ZoomAtMaxSpeed = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Minimap") float MaxZoomSpeedKmh = 120.f;
	UPROPERTY(EditAnywhere, Category = "Minimap") float ZoomInterpSpeed = 2.f;
	/** Rotate the map with the camera (true) or keep north up (false). */
	UPROPERTY(EditAnywhere, Category = "Minimap") bool bRotateWithCamera = true;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MapMaterial;

	TArray<FDTMinimapIconView> IconViews;
	FVector2D SearchCenter = FVector2D::ZeroVector;
	float SearchRadius = 0.f;
	float CurrentZoom = 0.05f;
	float CurrentRotation = 0.f;
};
