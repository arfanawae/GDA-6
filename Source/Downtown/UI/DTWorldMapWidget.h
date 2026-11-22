#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "World/Province/DTProvinceTypes.h"
#include "DTWorldMapWidget.generated.h"

class UDTProvinceSubsystem;

/**
 * Full-screen map of Thailand (toggle with IA_Map / M, see ADTPlayerController).
 * Draws every province outline itself (no assets needed) coloured by progress:
 *   current = gold, visited = green, not visited = grey; plus the player arrow and optional names.
 * Mouse wheel zooms around the cursor, left-drag pans. Hovering a province fires OnHoveredProvinceChanged
 * so WBP_WorldMap can show a tooltip / side panel. Child widgets of the BP are drawn on top.
 * Optional background: BackgroundBrush (e.g. T_Minimap from build_provinces.py --minimap) placed at
 * BackgroundWorldBounds (the "minimap" bounds in map_bounds.json).
 */
UCLASS()
class DOWNTOWN_API UDTWorldMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "World Map")
	void ResetView();

	/** Centre the view on the player (keeps the zoom). */
	UFUNCTION(BlueprintCallable, Category = "World Map")
	void CenterOnPlayer();

	UFUNCTION(BlueprintPure, Category = "World Map")
	FName GetHoveredProvince() const { return HoveredProvince; }

	/** Fired when the mouse moves onto another province (NAME_None = sea / outside). */
	UFUNCTION(BlueprintImplementableEvent, Category = "World Map")
	void OnHoveredProvinceChanged(FName Province, const FText& NameTH, const FText& NameEN, EDTThaiRegion Region, bool bVisited);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") FLinearColor VisitedColor = FLinearColor(0.20f, 0.85f, 0.35f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") FLinearColor UnvisitedColor = FLinearColor(0.55f, 0.55f, 0.60f, 0.8f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") FLinearColor CurrentColor = FLinearColor(1.f, 0.78f, 0.15f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") FLinearColor HoverColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") FLinearColor PlayerColor = FLinearColor(1.f, 0.25f, 0.2f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") float LineThickness = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Style") float HighlightThickness = 3.f;

	/** Province names at their reference points. Needs a font with Thai glyphs when bThaiNames is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Labels") bool bShowNames = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Labels") bool bThaiNames = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Labels") FSlateFontInfo LabelFont;
	/** Names appear once the map is zoomed in at least this much (1 = whole country). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Labels") float NamesFromZoom = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Background") FSlateBrush BackgroundBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|Background") FBox2D BackgroundWorldBounds = FBox2D(ForceInit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Map|View", meta = (ClampMin = "1")) float MaxZoom = 40.f;

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	const UDTProvinceSubsystem* GetProvinces() const;
	bool EnsureBounds() const;
	double GetScale(const FVector2D& Size) const;
	FVector2D WorldToLocal(double X, double Y, const FVector2D& Size) const;
	FVector2D LocalToWorld(const FVector2D& Local, const FVector2D& Size) const;

	/** World XY bounds of all provinces (computed lazily). */
	mutable FBox2D WorldBounds = FBox2D(ForceInit);

	float Zoom = 1.f;
	FVector2D Pan = FVector2D::ZeroVector;
	bool bDragging = false;
	FVector2D LastMouse = FVector2D::ZeroVector;
	FName HoveredProvince = NAME_None;
};
