#include "UI/DTWorldMapWidget.h"
#include "World/Province/DTProvinceSubsystem.h"
#include "World/Province/DTProvinceGeometry.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UDTWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible); // receive mouse input
	if (LabelFont.Size <= 0)
	{
		LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	}
	ResetView();
}

const UDTProvinceSubsystem* UDTWorldMapWidget::GetProvinces() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDTProvinceSubsystem>() : nullptr;
}

bool UDTWorldMapWidget::EnsureBounds() const
{
	if (WorldBounds.bIsValid)
	{
		return true;
	}
	const UDTProvinceSubsystem* Provinces = GetProvinces();
	if (!Provinces)
	{
		return false;
	}
	for (const TPair<FName, FDTProvinceGeometry::FEntry>& Pair : Provinces->GetGeometry().GetEntries())
	{
		WorldBounds += Pair.Value.Bounds;
	}
	return WorldBounds.bIsValid;
}

double UDTWorldMapWidget::GetScale(const FVector2D& Size) const
{
	// North up: screen X follows world Y (east), screen Y follows -world X (north). Fit with 5% padding.
	const FVector2D Extent = WorldBounds.GetSize();
	const double Fit = FMath::Min(Size.X / FMath::Max(Extent.Y, 1.0), Size.Y / FMath::Max(Extent.X, 1.0)) * 0.95;
	return Fit * Zoom;
}

FVector2D UDTWorldMapWidget::WorldToLocal(double X, double Y, const FVector2D& Size) const
{
	const FVector2D C = WorldBounds.GetCenter();
	const double S = GetScale(Size);
	return FVector2D(Size.X * 0.5 + Pan.X + (Y - C.Y) * S, Size.Y * 0.5 + Pan.Y - (X - C.X) * S);
}

FVector2D UDTWorldMapWidget::LocalToWorld(const FVector2D& Local, const FVector2D& Size) const
{
	const FVector2D C = WorldBounds.GetCenter();
	const double S = FMath::Max(GetScale(Size), 1e-9);
	const double Y = (Local.X - Size.X * 0.5 - Pan.X) / S + C.Y;
	const double X = -(Local.Y - Size.Y * 0.5 - Pan.Y) / S + C.X;
	return FVector2D(X, Y);
}

void UDTWorldMapWidget::ResetView()
{
	Zoom = 1.f;
	Pan = FVector2D::ZeroVector;
}

void UDTWorldMapWidget::CenterOnPlayer()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || !EnsureBounds())
	{
		return;
	}
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	const FVector Loc = Player->GetActorLocation();
	const FVector2D C = WorldBounds.GetCenter();
	const double S = GetScale(Size);
	Pan = FVector2D(-(Loc.Y - C.Y) * S, (Loc.X - C.X) * S);
}

int32 UDTWorldMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const UDTProvinceSubsystem* Provinces = GetProvinces();
	if (!Provinces || !EnsureBounds())
	{
		return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FDTProvinceGeometry& Geo = Provinces->GetGeometry();
	const FName Current = Provinces->GetCurrentProvince();

	// Background texture, if any.
	if (BackgroundWorldBounds.bIsValid && BackgroundBrush.GetResourceObject())
	{
		const FVector2D TopLeft = WorldToLocal(BackgroundWorldBounds.Max.X, BackgroundWorldBounds.Min.Y, Size);
		const FVector2D BottomRight = WorldToLocal(BackgroundWorldBounds.Min.X, BackgroundWorldBounds.Max.Y, Size);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(BottomRight - TopLeft), FSlateLayoutTransform(FVector2f(TopLeft))),
			&BackgroundBrush, ESlateDrawEffect::None, FLinearColor::White);
	}

	// Outlines: unvisited first, then visited, then hovered/current on top.
	TArray<FVector2f> Line;
	auto DrawProvince = [&](const FDTProvinceGeometry::FEntry& Entry, const FLinearColor& Color, float Thickness, int32 Layer)
	{
		for (const FDTProvinceRing& Ring : Entry.Row.Rings)
		{
			Line.Reset(Ring.Points.Num() + 1);
			for (const FVector2D& P : Ring.Points)
			{
				Line.Add(FVector2f(WorldToLocal(P.X, P.Y, Size)));
			}
			if (Line.Num() > 1)
			{
				Line.Add(Line[0]);
				FSlateDrawElement::MakeLines(OutDrawElements, Layer, PaintGeometry, Line, ESlateDrawEffect::None, Color, true, Thickness);
			}
		}
	};

	for (const TPair<FName, FDTProvinceGeometry::FEntry>& Pair : Geo.GetEntries())
	{
		if (!Provinces->HasVisited(Pair.Key))
		{
			DrawProvince(Pair.Value, UnvisitedColor, LineThickness, LayerId + 1);
		}
	}
	for (const TPair<FName, FDTProvinceGeometry::FEntry>& Pair : Geo.GetEntries())
	{
		if (Provinces->HasVisited(Pair.Key))
		{
			DrawProvince(Pair.Value, VisitedColor, LineThickness * 1.5f, LayerId + 2);
		}
	}
	if (const FDTProvinceGeometry::FEntry* Hovered = Geo.Find(HoveredProvince))
	{
		DrawProvince(*Hovered, HoverColor, HighlightThickness, LayerId + 3);
	}
	if (const FDTProvinceGeometry::FEntry* CurrentEntry = Geo.Find(Current))
	{
		DrawProvince(*CurrentEntry, CurrentColor, HighlightThickness, LayerId + 3);
	}

	// Names.
	if (bShowNames && Zoom >= NamesFromZoom)
	{
		for (const TPair<FName, FDTProvinceGeometry::FEntry>& Pair : Geo.GetEntries())
		{
			const FVector2D P = WorldToLocal(Pair.Value.Row.ReferencePoint.X, Pair.Value.Row.ReferencePoint.Y, Size);
			if (P.X < 0 || P.Y < 0 || P.X > Size.X || P.Y > Size.Y)
			{
				continue;
			}
			const FText& Name = bThaiNames ? Pair.Value.Row.NameTH : Pair.Value.Row.NameEN;
			const FLinearColor Color = Provinces->HasVisited(Pair.Key) ? FLinearColor::White : FLinearColor(0.75f, 0.75f, 0.75f, 1.f);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 4,
				AllottedGeometry.ToPaintGeometry(FVector2f(1.f, 1.f), FSlateLayoutTransform(FVector2f(P))),
				Name, LabelFont, ESlateDrawEffect::None, Color);
		}
	}

	// Player arrow (world yaw 0 = north = up on the map).
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector Loc = Player->GetActorLocation();
		const FVector2D P = WorldToLocal(Loc.X, Loc.Y, Size);
		const double Yaw = FMath::DegreesToRadians(Player->GetActorRotation().Yaw);
		const FVector2D Fwd(FMath::Sin(Yaw), -FMath::Cos(Yaw));
		const FVector2D Right(-Fwd.Y, Fwd.X);
		const double L = 12.0;
		TArray<FVector2f> Arrow = {
			FVector2f(P + Fwd * L), FVector2f(P - Fwd * L * 0.6 + Right * L * 0.6),
			FVector2f(P - Fwd * L * 0.3), FVector2f(P - Fwd * L * 0.6 - Right * L * 0.6), FVector2f(P + Fwd * L) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 5, PaintGeometry, Arrow, ESlateDrawEffect::None, PlayerColor, true, 2.5f);
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 6, InWidgetStyle, bParentEnabled);
}

FReply UDTWorldMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!EnsureBounds())
	{
		return FReply::Unhandled();
	}
	const FVector2D Size = InGeometry.GetLocalSize();
	const FVector2D MouseLocal = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Anchor = LocalToWorld(MouseLocal, Size);

	const float Factor = InMouseEvent.GetWheelDelta() > 0 ? 1.25f : 0.8f;
	Zoom = FMath::Clamp(Zoom * Factor, 1.f, MaxZoom);

	// Keep the world point under the cursor fixed.
	const FVector2D C = WorldBounds.GetCenter();
	const double S = GetScale(Size);
	Pan = FVector2D(MouseLocal.X - Size.X * 0.5 - (Anchor.Y - C.Y) * S, MouseLocal.Y - Size.Y * 0.5 + (Anchor.X - C.X) * S);
	if (Zoom <= 1.f)
	{
		Pan = FVector2D::ZeroVector;
	}
	return FReply::Handled();
}

FReply UDTWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		LastMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Unhandled();
}

FReply UDTWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply UDTWorldMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	if (bDragging)
	{
		Pan += Local - LastMouse;
		LastMouse = Local;
	}

	const UDTProvinceSubsystem* Provinces = GetProvinces();
	if (Provinces && EnsureBounds())
	{
		const FVector2D WorldPoint = LocalToWorld(Local, InGeometry.GetLocalSize());
		const FName Found = Provinces->GetGeometry().FindAt(WorldPoint, HoveredProvince);
		if (Found != HoveredProvince)
		{
			HoveredProvince = Found;
			FDTProvinceRow Row;
			const bool bKnown = Provinces->GetProvinceRow(Found, Row);
			OnHoveredProvinceChanged(Found, bKnown ? Row.NameTH : FText::GetEmpty(), bKnown ? Row.NameEN : FText::GetEmpty(),
				bKnown ? Row.Region : EDTThaiRegion::Central, Provinces->HasVisited(Found));
		}
	}
	return bDragging ? FReply::Handled() : FReply::Unhandled();
}
