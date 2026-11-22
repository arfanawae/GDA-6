#include "World/Geo/DTGeoRoad.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"

ADTGeoRoad::ADTGeoRoad()
{
	PrimaryActorTick.bCanEverTick = false;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
#if WITH_EDITORONLY_DATA
	Spline->EditorUnselectedSplineSegmentColor = FLinearColor(1.f, 0.8f, 0.2f);
#endif
}

void ADTGeoRoad::Build(const TArray<FVector>& WorldPoints, float InWidthCm, UStaticMesh* InMesh)
{
	WidthCm = InWidthCm;
	if (WorldPoints.Num() < 2)
	{
		return;
	}
	SetActorLocation(WorldPoints[0]);

	Spline->ClearSplinePoints(false);
	for (int32 i = 0; i < WorldPoints.Num(); ++i)
	{
		Spline->AddSplinePoint(WorldPoints[i], ESplineCoordinateSpace::World, false);
		Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
	}
	Spline->UpdateSpline();

	if (!InMesh)
	{
		return;
	}

	// One spline mesh per segment. The mesh is assumed to be 1 m wide along Y and to run along +X.
	const FBoxSphereBounds MeshBounds = InMesh->GetBounds();
	const float MeshWidth = FMath::Max(1.f, MeshBounds.BoxExtent.Y * 2.f);
	const FVector2D Scale(WidthCm / MeshWidth, 1.f);

	for (int32 i = 0; i < WorldPoints.Num() - 1; ++i)
	{
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this, NAME_None, RF_Transactional);
		Segment->SetMobility(EComponentMobility::Static);
		Segment->SetStaticMesh(InMesh);
		Segment->SetupAttachment(Spline);
		const FVector StartPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector EndPos = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
		const FVector Tangent = EndPos - StartPos;
		Segment->SetStartAndEnd(StartPos, Tangent, EndPos, Tangent, false);
		Segment->SetStartScale(Scale, false);
		Segment->SetEndScale(Scale, false);
		Segment->UpdateMesh();
		AddInstanceComponent(Segment);
		Segment->RegisterComponent();
	}
}
