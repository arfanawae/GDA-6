#include "World/Geo/DTGeoImporter.h"
#include "World/Geo/DTGeoRoad.h"
#include "World/Geo/DTGeoSettings.h"
#include "World/Province/DTProvinceSign.h"
#include "AI/Traffic/DTTrafficLane.h"
#include "Downtown.h"
#include "Components/BillboardComponent.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Algo/Reverse.h"
#include "EngineUtils.h"
#include "Engine/World.h"

static const FName GeoImportTag(TEXT("DTGeoImported"));

ADTGeoImporter::ADTGeoImporter()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
	}
	bIsSpatiallyLoaded = false;
#endif
	bIsEditorOnlyActor = true;

	RoadClass = ADTGeoRoad::StaticClass();
	LaneClass = ADTTrafficLane::StaticClass();
	SignClass = ADTProvinceSign::StaticClass();
}

template <typename T>
T* ADTGeoImporter::SpawnTagged(TSubclassOf<T> Class, const FVector& Location, const FRotator& Rotation, const FString& Folder)
{
	if (!Class)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	T* Actor = GetWorld()->SpawnActor<T>(Class, Location, Rotation, Params);
	if (Actor)
	{
		Actor->Tags.Add(GeoImportTag);
#if WITH_EDITOR
		Actor->SetFolderPath(FName(*Folder));
#endif
	}
	return Actor;
}

FVector ADTGeoImporter::ToWorld(double X, double Y, int32 Level) const
{
	double Z = BaseZ;
	if (bTraceGround)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGeoImportGround), false);
		Params.bTraceComplex = false;
		if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, 500000.0), FVector(X, Y, -500000.0), ECC_WorldStatic, Params))
		{
			Z = Hit.ImpactPoint.Z;
		}
	}
	return FVector(X, Y, Z + 10.0 + Level * GetDefault<UDTGeoSettings>()->RoadLevelHeightCm);
}

TArray<TArray<FVector>> ADTGeoImporter::SplitByLength(const TArray<FVector>& Points, float MaxLength)
{
	TArray<TArray<FVector>> Pieces;
	TArray<FVector> Current;
	float Length = 0.f;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		if (Current.Num() > 0)
		{
			const FVector& Prev = Current.Last();
			float SegLen = FVector::Dist(Prev, Points[i]);
			// Insert intermediate points on very long segments (rural roads) so pieces stay short.
			while (Length + SegLen > MaxLength)
			{
				const float Alpha = (MaxLength - Length) / SegLen;
				const FVector Cut = FMath::Lerp(Current.Last(), Points[i], Alpha);
				Current.Add(Cut);
				Pieces.Add(Current);
				Current = { Cut };
				SegLen = FVector::Dist(Cut, Points[i]);
				Length = 0.f;
			}
			Length += SegLen;
		}
		Current.Add(Points[i]);
	}
	if (Current.Num() >= 2)
	{
		Pieces.Add(Current);
	}
	return Pieces;
}

TArray<FVector> ADTGeoImporter::OffsetPolyline(const TArray<FVector>& Points, float LeftOffset)
{
	// Left of travel direction in UE top view (X = north, Y = east): Left(d) = (d.Y, -d.X).
	TArray<FVector> Result;
	Result.Reserve(Points.Num());
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector Prev = Points[FMath::Max(0, i - 1)];
		const FVector Next = Points[FMath::Min(Points.Num() - 1, i + 1)];
		const FVector Dir = (Next - Prev).GetSafeNormal2D();
		const FVector Left(Dir.Y, -Dir.X, 0.0);
		Result.Add(Points[i] + Left * LeftOffset);
	}
	return Result;
}

void ADTGeoImporter::LinkLanes(const TArray<ADTTrafficLane*>& Lanes) const
{
	// Spatial hash of lane starts.
	const float Cell = ConnectRadiusCm * 2.f;
	TMultiMap<FIntPoint, ADTTrafficLane*> Starts;
	auto CellOf = [Cell](const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); };
	for (ADTTrafficLane* Lane : Lanes)
	{
		Starts.Add(CellOf(Lane->GetSpline()->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World)), Lane);
	}

	int32 Links = 0;
	for (ADTTrafficLane* Lane : Lanes)
	{
		const USplineComponent* S = Lane->GetSpline();
		const int32 Last = S->GetNumberOfSplinePoints() - 1;
		const FVector End = S->GetLocationAtSplinePoint(Last, ESplineCoordinateSpace::World);
		const FVector EndDir = S->GetDirectionAtSplinePoint(Last, ESplineCoordinateSpace::World).GetSafeNormal2D();
		const FIntPoint C = CellOf(End);

		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				TArray<ADTTrafficLane*> Candidates;
				Starts.MultiFind(C + FIntPoint(dx, dy), Candidates);
				for (ADTTrafficLane* Other : Candidates)
				{
					if (Other == Lane)
					{
						continue;
					}
					const USplineComponent* O = Other->GetSpline();
					const FVector Start = O->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
					if (FVector::Dist2D(End, Start) > ConnectRadiusCm || FMath::Abs(End.Z - Start.Z) > 300.f)
					{
						continue;
					}
					// No U-turns onto the opposite carriageway.
					const FVector StartDir = O->GetDirectionAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal2D();
					if (FVector::DotProduct(EndDir, StartDir) < -0.3f)
					{
						continue;
					}
					Lane->AddNextLane(Other);
					++Links;
				}
			}
		}
	}
	UE_LOG(LogDowntown, Log, TEXT("GeoImport: %d lane links."), Links);
}

void ADTGeoImporter::Import()
{
	ClearImported();

	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), RoadsFile);
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogDowntown, Error, TEXT("GeoImport: cannot read %s"), *Path);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		UE_LOG(LogDowntown, Error, TEXT("GeoImport: invalid JSON in %s"), *Path);
		return;
	}

	// Projection sanity check: the file must have been generated with the same settings.
	const UDTGeoSettings* Settings = GetDefault<UDTGeoSettings>();
	if (const TSharedPtr<FJsonObject>* Config; Root->TryGetObjectField(TEXT("config"), Config))
	{
		const double FileScale = (*Config)->GetNumberField(TEXT("horizontal_scale"));
		const double FileLat = (*Config)->GetNumberField(TEXT("origin_lat"));
		const double FileLon = (*Config)->GetNumberField(TEXT("origin_lon"));
		if (!FMath::IsNearlyEqual(FileScale, Settings->HorizontalScale) || !FMath::IsNearlyEqual(FileLat, Settings->OriginLat) || !FMath::IsNearlyEqual(FileLon, Settings->OriginLon))
		{
			UE_LOG(LogDowntown, Error, TEXT("GeoImport: %s was built with scale %.1f origin (%.3f, %.3f) but Project Settings use %.1f (%.3f, %.3f). Re-run the pipeline or fix the settings."),
				*RoadsFile, FileScale, FileLat, FileLon, Settings->HorizontalScale, Settings->OriginLat, Settings->OriginLon);
			return;
		}
	}

	int32 RoadCount = 0, SignCount = 0;
	TArray<ADTTrafficLane*> NewLanes;

	for (const TSharedPtr<FJsonValue>& RoadValue : Root->GetArrayField(TEXT("roads")))
	{
		const TSharedPtr<FJsonObject> Road = RoadValue->AsObject();
		const FString Highway = Road->GetStringField(TEXT("highway"));
		const bool bOneWay = Road->GetBoolField(TEXT("oneway"));
		const int32 LaneCount = FMath::Max(1, static_cast<int32>(Road->GetNumberField(TEXT("lanes"))));
		const float Speed = Road->GetNumberField(TEXT("speed_kmh"));
		FString Name = Road->GetStringField(TEXT("name_th"));
		if (Name.IsEmpty())
		{
			Name = Road->GetStringField(TEXT("name_en"));
		}

		TArray<FVector> Points;
		for (const TSharedPtr<FJsonValue>& PV : Road->GetArrayField(TEXT("points")))
		{
			const TArray<TSharedPtr<FJsonValue>>& P = PV->AsArray();
			const int32 Level = P.Num() > 2 ? static_cast<int32>(P[2]->AsNumber()) : 0;
			Points.Add(ToWorld(P[0]->AsNumber(), P[1]->AsNumber(), Level));
		}
		if (Points.Num() < 2)
		{
			continue;
		}

		const float CarriagewayWidth = LaneWidthCm * LaneCount;
		const bool bLink = Highway.EndsWith(TEXT("_link"));
		if (bOneWay && !bLink && bSeparateDualCarriageways)
		{
			Points = OffsetPolyline(Points, CarriagewayWidth * 0.5f + MedianWidthCm * 0.5f);
		}

		if (bImportRoads)
		{
			for (const TArray<FVector>& Piece : SplitByLength(Points, MaxRoadPieceCm))
			{
				if (ADTGeoRoad* GeoRoad = SpawnTagged<ADTGeoRoad>(RoadClass, Piece[0], FRotator::ZeroRotator, TEXT("Geo/Roads/") + Highway))
				{
					GeoRoad->RoadName = Name;
					GeoRoad->Highway = Highway;
					GeoRoad->Lanes = LaneCount;
					GeoRoad->bOneWay = bOneWay;
					GeoRoad->Build(Piece, CarriagewayWidth, RoadMesh);
#if WITH_EDITOR
					GeoRoad->SetActorLabel(FString::Printf(TEXT("Road_%s_%d"), *Highway, RoadCount));
#endif
					++RoadCount;
				}
			}
		}

		if (bImportTrafficLanes && TrafficClasses.Contains(Highway.Replace(TEXT("_link"), TEXT(""))))
		{
			// Thailand drives on the LEFT. One AI lane per direction, in the middle of that direction's half.
			TArray<TArray<FVector>> Directions;
			if (bOneWay)
			{
				Directions.Add(Points);
			}
			else
			{
				const float HalfOffset = CarriagewayWidth * 0.25f;
				Directions.Add(OffsetPolyline(Points, HalfOffset));
				TArray<FVector> Reversed = Points;
				Algo::Reverse(Reversed);
				Directions.Add(OffsetPolyline(Reversed, HalfOffset));
			}
			for (const TArray<FVector>& DirPoints : Directions)
			{
				if (ADTTrafficLane* Lane = SpawnTagged<ADTTrafficLane>(LaneClass, DirPoints[0], FRotator::ZeroRotator, TEXT("Geo/TrafficLanes")))
				{
					Lane->InitializeFromPoints(DirPoints, Speed);
					NewLanes.Add(Lane);
				}
			}
		}
	}

	if (NewLanes.Num() > 0)
	{
		LinkLanes(NewLanes);
	}

	if (bImportSigns)
	{
		for (const TSharedPtr<FJsonValue>& CV : Root->GetArrayField(TEXT("crossings")))
		{
			const TSharedPtr<FJsonObject> C = CV->AsObject();
			const float Heading = C->GetNumberField(TEXT("heading"));
			const FVector Forward = FRotator(0.f, Heading, 0.f).Vector();
			const FVector Left(Forward.Y, -Forward.X, 0.0);
			const FVector Base(C->GetNumberField(TEXT("x")), C->GetNumberField(TEXT("y")), 0.0);
			// Slightly before the border, on the driver's left, facing oncoming (approaching) traffic.
			const FVector Spot = Base - Forward * 1500.0 + Left * SignSideOffsetCm;
			const FVector Location = ToWorld(Spot.X, Spot.Y, 0);
			if (ADTProvinceSign* Sign = SpawnTagged<ADTProvinceSign>(SignClass, Location, FRotator(0.f, Heading + 180.f, 0.f), TEXT("Geo/ProvinceSigns")))
			{
				Sign->ProvinceCode = FName(*C->GetStringField(TEXT("to")));
				Sign->FromProvinceCode = FName(*C->GetStringField(TEXT("from")));
				Sign->RoadName = C->GetStringField(TEXT("road"));
				++SignCount;
			}
		}
	}

	UE_LOG(LogDowntown, Log, TEXT("GeoImport: %d road pieces, %d traffic lanes, %d province signs from %s"), RoadCount, NewLanes.Num(), SignCount, *RoadsFile);
}

void ADTGeoImporter::ClearImported()
{
	int32 Removed = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(GeoImportTag))
		{
			It->Destroy();
			++Removed;
		}
	}
	if (Removed > 0)
	{
		UE_LOG(LogDowntown, Log, TEXT("GeoImport: removed %d imported actors."), Removed);
	}
}
