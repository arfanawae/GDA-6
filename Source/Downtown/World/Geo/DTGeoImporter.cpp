#include "World/Geo/DTGeoImporter.h"
#include "World/Geo/DTGeoRoad.h"
#include "World/Geo/DTGeoSettings.h"
#include "World/Geo/DTGeoLibrary.h"
#include "World/Geo/DTGeoCoastline.h"
#include "World/Province/DTProvinceSign.h"
#include "AI/Traffic/DTTrafficLane.h"
#include "Downtown.h"
#include "Algo/Reverse.h"
#include "Components/BillboardComponent.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DTGeoImport
{
	static const FName ImportedTag(TEXT("DTGeoImported"));

	static FName RunTag(int32 RunId)
	{
		return FName(*FString::Printf(TEXT("DTGeoRun_%d"), RunId));
	}

	static FName ZoneTag(const FString& ZoneLabel)
	{
		return FName(*(TEXT("DTGeoZone_") + ZoneLabel));
	}

	static FString Km(double Cm)
	{
		return FString::Printf(TEXT("%.2f km"), Cm / 100000.0);
	}

	static FString JoinCodes(const TArray<FName>& Codes, int32 MaxShown = 12)
	{
		FString Out;
		for (int32 i = 0; i < Codes.Num() && i < MaxShown; ++i)
		{
			Out += (i > 0 ? TEXT(", ") : TEXT("")) + Codes[i].ToString();
		}
		if (Codes.Num() > MaxShown)
		{
			Out += FString::Printf(TEXT(" ... (+%d)"), Codes.Num() - MaxShown);
		}
		return Out;
	}
}

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
	CoastClass = ADTGeoCoastline::StaticClass();
}

// ---------------------------------------------------------------------------------------------------------------- buttons

void ADTGeoImporter::Import()
{
	RunImport(!bDryRun);
}

void ADTGeoImporter::ValidateData()
{
	RunImport(false);
}

void ADTGeoImporter::ClearLastRun()
{
	if (LastRunId <= 0)
	{
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Clear Last Run: nothing imported yet by %s."), *GetName());
		return;
	}
	ClearByTag(DTGeoImport::RunTag(LastRunId), *FString::Printf(TEXT("run #%d"), LastRunId));
}

void ADTGeoImporter::ClearZone()
{
	ClearByTag(DTGeoImport::ZoneTag(GetZoneLabel()), *FString::Printf(TEXT("zone '%s'"), *GetZoneLabel()));
}

void ADTGeoImporter::ClearImported()
{
	ClearByTag(DTGeoImport::ImportedTag, TEXT("all imports"));
}

int32 ADTGeoImporter::ClearByTag(FName Tag, const TCHAR* What)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Roads = 0, Lanes = 0, Signs = 0, Coast = 0, Other = 0;
	TArray<AActor*> ToRemove;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->Tags.Contains(Tag))
		{
			ToRemove.Add(*It);
		}
	}
	for (AActor* Actor : ToRemove)
	{
		if (Actor->IsA<ADTGeoRoad>()) { ++Roads; }
		else if (Actor->IsA<ADTTrafficLane>()) { ++Lanes; }
		else if (Actor->IsA<ADTProvinceSign>()) { ++Signs; }
		else if (Actor->IsA<ADTGeoCoastline>()) { ++Coast; }
		else { ++Other; }
#if WITH_EDITOR
		Actor->Modify();
#endif
		Actor->Destroy();
	}

	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Cleared %s: %d actors (%d road pieces, %d traffic lanes, %d signs, %d coastline, %d other)."),
		What, ToRemove.Num(), Roads, Lanes, Signs, Coast, Other);
	if (ToRemove.Num() == 0)
	{
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport]   Nothing matched tag %s. In World Partition only actors in LOADED cells are found - load the region first."), *Tag.ToString());
	}
	return ToRemove.Num();
}

void ADTGeoImporter::PreviewZone()
{
	FDTProvinceGeometry Geometry;
	if (!LoadProvinceGeometry(Geometry, true))
	{
		return;
	}
	TArray<FName> Codes;
	bool bAll = false;
	if (!ResolveZoneFilter(Geometry, Codes, bAll))
	{
		return;
	}

	const double Scale = GetDefault<UDTGeoSettings>()->HorizontalScale;
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Preview zone '%s': %d province(s)"), *GetZoneLabel(), Codes.Num());
	for (const FName& Code : Codes)
	{
		const FDTProvinceGeometry::FEntry* E = Geometry.Find(Code);
		if (!E)
		{
			continue;
		}
		const FVector2D Size = E->Bounds.GetSize();
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport]   %-6s %-24s ref X=%.0f Y=%.0f (%.4f N %.4f E), %s x %s in game (%.0f x %.0f km real)"),
			*Code.ToString(), *E->Row.NameEN.ToString(), E->Row.ReferencePoint.X, E->Row.ReferencePoint.Y,
			E->Row.ReferenceLatLon.X, E->Row.ReferenceLatLon.Y, *DTGeoImport::Km(Size.X), *DTGeoImport::Km(Size.Y),
			Size.X / 100000.0 * Scale, Size.Y / 100000.0 * Scale);
	}

	const FBox2D Box = Geometry.GetBounds(Codes);
	if (Box.bIsValid)
	{
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport]   Zone bounds X %.0f..%.0f, Y %.0f..%.0f cm (%s x %s). Landscape must cover this area."),
			Box.Min.X, Box.Max.X, Box.Min.Y, Box.Max.Y, *DTGeoImport::Km(Box.GetSize().X), *DTGeoImport::Km(Box.GetSize().Y));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Z = BaseZ + 2000.0;
	int32 Segments = 0;
	for (const FName& Code : Codes)
	{
		const FDTProvinceGeometry::FEntry* E = Geometry.Find(Code);
		if (!E)
		{
			continue;
		}
		for (const FDTProvinceRing& Ring : E->Row.Rings)
		{
			for (int32 i = 0; i < Ring.Points.Num(); ++i)
			{
				const FVector2D& A = Ring.Points[i];
				const FVector2D& B = Ring.Points[(i + 1) % Ring.Points.Num()];
				DrawDebugLine(World, FVector(A.X, A.Y, Z), FVector(B.X, B.Y, Z), FColor::Yellow, false, PreviewSeconds, 0, 100.f);
				++Segments;
			}
		}
		const FVector Ref(E->Row.ReferencePoint.X, E->Row.ReferencePoint.Y, Z);
		DrawDebugSphere(World, Ref, 3000.f, 12, FColor::Green, false, PreviewSeconds, 0, 50.f);
	}
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport]   Drew %d outline segments + reference points (green) at Z=%.0f for %.0f s."), Segments, Z, PreviewSeconds);
}

// ---------------------------------------------------------------------------------------------------------------- helpers

void ADTGeoImporter::Warn(FImportStats& Stats, const FString& Message) const
{
	if (Stats.WarningsLogged < MaxDetailedWarnings)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport]   %s"), *Message);
		++Stats.WarningsLogged;
	}
	else
	{
		++Stats.WarningsSuppressed;
	}
}

bool ADTGeoImporter::LoadProvinceGeometry(FDTProvinceGeometry& OutGeometry, bool bRequired) const
{
	const UDataTable* Table = ProvinceTable.LoadSynchronous();
	if (!Table)
	{
		if (bRequired)
		{
			UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] ProvinceTable not set or not found (%s). Import DT_Provinces.json as a DataTable (row struct DTProvinceRow) and assign it."),
				*ProvinceTable.ToString());
		}
		return false;
	}

	TArray<FString> Warnings;
	const int32 Count = OutGeometry.Build(Table, Warnings);
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Province table %s: %d/%d provinces usable."), *Table->GetPathName(), Count, Table->GetRowMap().Num());
	for (const FString& W : Warnings)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport]   %s"), *W);
	}
	return Count > 0;
}

FString ADTGeoImporter::GetZoneLabel() const
{
	FString Label = ZoneFilter.TrimStartAndEnd().ToLower();
	if (Label.IsEmpty())
	{
		return TEXT("all");
	}
	Label.ReplaceInline(TEXT(" "), TEXT(""));
	Label.ReplaceInline(TEXT(","), TEXT("+"));
	return Label;
}

bool ADTGeoImporter::ResolveZoneFilter(const FDTProvinceGeometry& Geometry, TArray<FName>& OutCodes, bool& bOutAll) const
{
	bOutAll = GetZoneLabel() == TEXT("all");
	FString Error;
	if (!Geometry.ResolveZone(ZoneFilter, OutCodes, Error))
	{
		UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] ZoneFilter '%s': %s"), *ZoneFilter, *Error);
		return false;
	}
	FString Names;
	for (int32 i = 0; i < OutCodes.Num() && i < 12; ++i)
	{
		Names += (i > 0 ? TEXT(", ") : TEXT("")) + Geometry.Describe(OutCodes[i]);
	}
	if (OutCodes.Num() > 12)
	{
		Names += FString::Printf(TEXT(" ... (+%d)"), OutCodes.Num() - 12);
	}
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Zone '%s' -> %d province(s): %s"), *GetZoneLabel(), OutCodes.Num(), *Names);
	return true;
}

bool ADTGeoImporter::ParseRoad(const TSharedPtr<FJsonValue>& Value, FRoadData& Out, int32& OutBadPoints, FString& OutError) const
{
	const TSharedPtr<FJsonObject>* RoadPtr = nullptr;
	if (!Value.IsValid() || !Value->TryGetObject(RoadPtr) || !RoadPtr || !RoadPtr->IsValid())
	{
		OutError = TEXT("entry is not a JSON object");
		return false;
	}
	const TSharedPtr<FJsonObject>& Road = *RoadPtr;

	Road->TryGetStringField(TEXT("id"), Out.Id);
	if (!Road->TryGetStringField(TEXT("name_th"), Out.Name) || Out.Name.IsEmpty())
	{
		Road->TryGetStringField(TEXT("name_en"), Out.Name);
	}
	if (!Road->TryGetStringField(TEXT("highway"), Out.Highway) || Out.Highway.IsEmpty())
	{
		OutError = TEXT("missing 'highway'");
		return false;
	}
	Road->TryGetBoolField(TEXT("oneway"), Out.bOneWay);

	double Number = 0.0;
	if (Road->TryGetNumberField(TEXT("lanes"), Number) && FMath::IsFinite(Number))
	{
		Out.Lanes = FMath::Clamp(static_cast<int32>(Number), 1, 8);
	}
	if (Road->TryGetNumberField(TEXT("speed_kmh"), Number) && FMath::IsFinite(Number) && Number > 0.0)
	{
		Out.SpeedKmh = static_cast<float>(FMath::Min(Number, 200.0));
	}

	const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
	if (!Road->TryGetArrayField(TEXT("points"), Points) || !Points)
	{
		OutError = TEXT("missing 'points' array");
		return false;
	}

	OutBadPoints = 0;
	Out.XY.Reserve(Points->Num());
	Out.Levels.Reserve(Points->Num());
	for (const TSharedPtr<FJsonValue>& PointValue : *Points)
	{
		const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
		double X = 0.0, Y = 0.0, Level = 0.0;
		const bool bOk = PointValue.IsValid() && PointValue->TryGetArray(Coords) && Coords && Coords->Num() >= 2
			&& (*Coords)[0].IsValid() && (*Coords)[0]->TryGetNumber(X)
			&& (*Coords)[1].IsValid() && (*Coords)[1]->TryGetNumber(Y)
			&& FMath::IsFinite(X) && FMath::IsFinite(Y)
			&& FMath::Abs(X) < FDTProvinceGeometry::MaxAbsCoordinateCm && FMath::Abs(Y) < FDTProvinceGeometry::MaxAbsCoordinateCm;
		if (!bOk)
		{
			++OutBadPoints;
			continue;
		}
		if (Coords->Num() > 2 && (*Coords)[2].IsValid())
		{
			(*Coords)[2]->TryGetNumber(Level);
		}
		Out.XY.Add(FVector2D(X, Y));
		Out.Levels.Add(FMath::Clamp(FMath::IsFinite(Level) ? static_cast<int32>(Level) : 0, 0, 10));
	}
	return true;
}

bool ADTGeoImporter::ParsePoint2D(const TSharedPtr<FJsonValue>& Value, FVector2D& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
	double X = 0.0, Y = 0.0;
	if (Value.IsValid() && Value->TryGetArray(Coords) && Coords && Coords->Num() >= 2
		&& (*Coords)[0].IsValid() && (*Coords)[0]->TryGetNumber(X)
		&& (*Coords)[1].IsValid() && (*Coords)[1]->TryGetNumber(Y)
		&& FMath::IsFinite(X) && FMath::IsFinite(Y)
		&& FMath::Abs(X) < FDTProvinceGeometry::MaxAbsCoordinateCm && FMath::Abs(Y) < FDTProvinceGeometry::MaxAbsCoordinateCm)
	{
		Out = FVector2D(X, Y);
		return true;
	}
	return false;
}

bool ADTGeoImporter::LoadGeoFile(const FString& RelativePath, const TCHAR* Kind, const TArray<FName>& ZoneCodes, bool bAll, TSharedPtr<FJsonObject>& OutRoot) const
{
	const UDTGeoSettings* Settings = GetDefault<UDTGeoSettings>();
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), RelativePath));
	const int64 FileSize = IFileManager::Get().FileSize(*Path);
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Reading %s file %s (%s)"), Kind, *Path, FileSize >= 0 ? *FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0) : TEXT("not found"));

	FString Json;
	if (FileSize < 0 || !FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: cannot read the %s file. Paths are relative to Content/, e.g. _Downtown/Data/Roads/Roads_TestArea.json"), Kind);
		return false;
	}
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot) || !OutRoot.IsValid())
	{
		UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: %s is not valid JSON. Re-run the Tools/GeoPipeline script that made it."), *RelativePath);
		return false;
	}

	// Projection sanity check: the file must have been generated with the same settings.
	const TSharedPtr<FJsonObject>* Config = nullptr;
	if (OutRoot->TryGetObjectField(TEXT("config"), Config) && Config && Config->IsValid())
	{
		double FileScale = 0.0, FileLat = 0.0, FileLon = 0.0, FileVertical = 0.0;
		(*Config)->TryGetNumberField(TEXT("horizontal_scale"), FileScale);
		(*Config)->TryGetNumberField(TEXT("origin_lat"), FileLat);
		(*Config)->TryGetNumberField(TEXT("origin_lon"), FileLon);
		(*Config)->TryGetNumberField(TEXT("vertical_scale"), FileVertical);
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] %s projection: origin %.4f N %.4f E, 1:%.0f, vertical 1:%.0f"), Kind, FileLat, FileLon, FileScale, FileVertical);
		if (!FMath::IsNearlyEqual(FileScale, Settings->HorizontalScale) || !FMath::IsNearlyEqual(FileLat, Settings->OriginLat) || !FMath::IsNearlyEqual(FileLon, Settings->OriginLon))
		{
			UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: projection mismatch - file 1:%.1f origin (%.3f, %.3f) vs Project Settings 1:%.1f (%.3f, %.3f). Re-run the pipeline or fix Project Settings > Game > Downtown Geo."),
				FileScale, FileLat, FileLon, Settings->HorizontalScale, Settings->OriginLat, Settings->OriginLon);
			return false;
		}
		if (FileVertical > 0.0 && !FMath::IsNearlyEqual(FileVertical, Settings->VerticalScale))
		{
			UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] %s file was built with vertical 1:%.0f but Project Settings use 1:%.0f (heights/heightmaps may not match)."), Kind, FileVertical, Settings->VerticalScale);
		}
	}
	else
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] %s file has no 'config' block - cannot verify it was built with the same scale/origin."), Kind);
	}

	// Which provinces does the file cover? Warn early if the zone is outside it.
	const TArray<TSharedPtr<FJsonValue>>* AreaValues = nullptr;
	if (OutRoot->TryGetArrayField(TEXT("area"), AreaValues) && AreaValues)
	{
		TArray<FName> FileArea;
		for (const TSharedPtr<FJsonValue>& V : *AreaValues)
		{
			FString Code;
			if (V.IsValid() && V->TryGetString(Code))
			{
				FileArea.Add(FName(*Code));
			}
		}
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] %s file covers: %s"), Kind, *DTGeoImport::JoinCodes(FileArea));
		if (!bAll)
		{
			TArray<FName> NotCovered;
			for (const FName& Code : ZoneCodes)
			{
				if (!FileArea.Contains(Code))
				{
					NotCovered.Add(Code);
				}
			}
			if (NotCovered.Num() == ZoneCodes.Num() && ZoneCodes.Num() > 0)
			{
				UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: none of the zone's provinces are in the %s file. Build one with --area %s"),
					Kind, *DTGeoImport::JoinCodes(ZoneCodes, 77).Replace(TEXT(" "), TEXT("")));
				return false;
			}
			if (NotCovered.Num() > 0)
			{
				UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] Zone provinces NOT in the %s file (nothing will appear there): %s"), Kind, *DTGeoImport::JoinCodes(NotCovered));
			}
		}
	}
	return true;
}

template <typename T>
T* ADTGeoImporter::SpawnTagged(TSubclassOf<T> Class, const FVector& Location, const FRotator& Rotation, const FString& Folder, const TArray<FName>& ExtraTags)
{
	UWorld* World = GetWorld();
	if (!Class || !World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	T* Actor = World->SpawnActor<T>(Class, Location, Rotation, Params);
	if (Actor)
	{
		Actor->Tags.Add(DTGeoImport::ImportedTag);
		Actor->Tags.Append(ExtraTags);
#if WITH_EDITOR
		Actor->SetFolderPath(FName(*Folder));
		Actor->MarkPackageDirty();
#endif
	}
	return Actor;
}

FVector ADTGeoImporter::ToWorld(double X, double Y, int32 Level, FImportStats* Stats) const
{
	double Z = BaseZ;
	if (bTraceGround && GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DTGeoImportGround), false);
		if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, 500000.0), FVector(X, Y, -500000.0), ECC_WorldStatic, Params))
		{
			Z = Hit.ImpactPoint.Z;
			if (Stats) { ++Stats->GroundHits; }
		}
		else if (Stats)
		{
			++Stats->GroundMisses;
		}
	}
	return FVector(X, Y, Z + 10.0 + Level * static_cast<double>(GetDefault<UDTGeoSettings>()->RoadLevelHeightCm));
}

TArray<TArray<FVector>> ADTGeoImporter::SplitByLength(const TArray<FVector>& Points, float MaxLength)
{
	TArray<TArray<FVector>> Pieces;
	TArray<FVector> Current;
	const double MaxLen = FMath::Max(1000.0, static_cast<double>(MaxLength));
	double Length = 0.0;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		if (Current.Num() > 0)
		{
			double SegLen = FVector::Dist(Current.Last(), Points[i]);
			// Insert intermediate points on very long segments (rural roads) so pieces stay short.
			while (SegLen > 0.0 && Length + SegLen > MaxLen)
			{
				const double Alpha = (MaxLen - Length) / SegLen;
				const FVector Cut = FMath::Lerp(Current.Last(), Points[i], Alpha);
				Current.Add(Cut);
				Pieces.Add(Current);
				Current = { Cut };
				SegLen = FVector::Dist(Cut, Points[i]);
				Length = 0.0;
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

int32 ADTGeoImporter::LinkLanes(const TArray<ADTTrafficLane*>& Lanes) const
{
	// Spatial hash of lane starts.
	const double Cell = FMath::Max(100.0, static_cast<double>(ConnectRadiusCm) * 2.0);
	TMultiMap<FIntPoint, ADTTrafficLane*> Starts;
	auto CellOf = [Cell](const FVector& P) { return FIntPoint(FMath::FloorToInt32(P.X / Cell), FMath::FloorToInt32(P.Y / Cell)); };
	for (ADTTrafficLane* Lane : Lanes)
	{
		Starts.Add(CellOf(Lane->GetSpline()->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World)), Lane);
	}

	int32 Links = 0;
	for (ADTTrafficLane* Lane : Lanes)
	{
		const USplineComponent* LaneSpline = Lane->GetSpline();
		const int32 Last = LaneSpline->GetNumberOfSplinePoints() - 1;
		const FVector End = LaneSpline->GetLocationAtSplinePoint(Last, ESplineCoordinateSpace::World);
		const FVector EndDir = LaneSpline->GetDirectionAtSplinePoint(Last, ESplineCoordinateSpace::World).GetSafeNormal2D();
		const FIntPoint EndCell = CellOf(End);

		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				TArray<ADTTrafficLane*> Candidates;
				Starts.MultiFind(EndCell + FIntPoint(dx, dy), Candidates);
				for (ADTTrafficLane* Other : Candidates)
				{
					if (Other == Lane)
					{
						continue;
					}
					const USplineComponent* OtherSpline = Other->GetSpline();
					const FVector Start = OtherSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
					if (FVector::Dist2D(End, Start) > ConnectRadiusCm || FMath::Abs(End.Z - Start.Z) > 300.0)
					{
						continue;
					}
					// No U-turns onto the opposite carriageway.
					const FVector StartDir = OtherSpline->GetDirectionAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal2D();
					if (FVector::DotProduct(EndDir, StartDir) < -0.3)
					{
						continue;
					}
					Lane->AddNextLane(Other);
					++Links;
				}
			}
		}
	}
	return Links;
}

// ---------------------------------------------------------------------------------------------------------------- import

void ADTGeoImporter::RunImport(bool bSpawn)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] No world - place the importer in a level first."));
		return;
	}

	const double StartTime = FPlatformTime::Seconds();
	const int32 RunId = bSpawn ? LastRunId + 1 : LastRunId;
	const FString Mode = bSpawn ? FString::Printf(TEXT("IMPORT run #%d"), RunId) : FString(TEXT("VALIDATE (dry run, nothing is spawned)"));
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] ===== %s | importer %s | zone '%s' ====="), *Mode, *GetName(), *GetZoneLabel());
	if (World->IsGameWorld())
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] Running in a game/PIE world: spawned actors disappear when PIE ends. Use the editor level to keep them."));
	}

	// --- Projection --------------------------------------------------------------------------------------------
	const UDTGeoSettings* Settings = GetDefault<UDTGeoSettings>();
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Projection: origin %.4f N %.4f E = world (0,0); horizontal 1:%.0f (1 km in game = %.0f km real, 100 km real = %.2f km in game); vertical 1:%.0f; road level %.0f cm"),
		Settings->OriginLat, Settings->OriginLon, Settings->HorizontalScale, Settings->HorizontalScale,
		100.0 / Settings->HorizontalScale, Settings->VerticalScale, Settings->RoadLevelHeightCm);

	// --- Zone --------------------------------------------------------------------------------------------------
	const bool bZoneAll = GetZoneLabel() == TEXT("all");
	FDTProvinceGeometry Geometry;
	const bool bHaveGeometry = LoadProvinceGeometry(Geometry, !bZoneAll);
	TArray<FName> ZoneCodes;
	bool bAll = bZoneAll;
	if (!bZoneAll)
	{
		if (!bHaveGeometry || !ResolveZoneFilter(Geometry, ZoneCodes, bAll))
		{
			UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: zone filtering needs a valid ProvinceTable and ZoneFilter (or set ZoneFilter to 'all')."));
			return;
		}
	}
	else
	{
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Zone 'all': every road and sign in the file is imported."));
	}
	const TSet<FName> ZoneSet(ZoneCodes);
	auto InZone = [&](const FVector2D& P, FName& Hint) -> bool
	{
		if (bAll)
		{
			return true;
		}
		Hint = Geometry.FindAt(P, Hint, &ZoneSet);
		return !Hint.IsNone();
	};

	// --- Files -------------------------------------------------------------------------------------------------
	const bool bUseRoadsFile = !RoadsFile.TrimStartAndEnd().IsEmpty() && (bImportRoads || bImportTrafficLanes || bImportSigns);
	const bool bUseCoastFile = bImportCoast && !CoastFile.TrimStartAndEnd().IsEmpty();
	if (!bUseRoadsFile && !bUseCoastFile)
	{
		UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: nothing to import - set RoadsFile and/or CoastFile."));
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedPtr<FJsonObject> CoastRoot;
	if (bUseRoadsFile && !LoadGeoFile(RoadsFile, TEXT("roads"), ZoneCodes, bAll, Root))
	{
		return;
	}
	if (bUseCoastFile && !LoadGeoFile(CoastFile, TEXT("coast"), ZoneCodes, bAll, CoastRoot))
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* RoadValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* CrossingValues = nullptr;
	if (Root.IsValid())
	{
		Root->TryGetArrayField(TEXT("roads"), RoadValues);
		Root->TryGetArrayField(TEXT("crossings"), CrossingValues);
		if (!RoadValues && !CrossingValues)
		{
			UE_LOG(LogDTGeo, Error, TEXT("[GeoImport] Aborted: %s has neither 'roads' nor 'crossings'."), *RoadsFile);
			return;
		}
	}

	// --- Clear previous results ----------------------------------------------------------------------------------
	if (bSpawn)
	{
		switch (ClearBeforeImport)
		{
		case EDTGeoClearMode::SameZone: ClearZone(); break;
		case EDTGeoClearMode::All:      ClearImported(); break;
		default: break;
		}
		LastRunId = RunId;
	}
	const TArray<FName> ExtraTags = { DTGeoImport::RunTag(RunId), DTGeoImport::ZoneTag(GetZoneLabel()) };

	// --- Roads + lanes -------------------------------------------------------------------------------------------
	FImportStats Stats;
	TArray<ADTTrafficLane*> NewLanes;
	FName ZoneHint = NAME_None;

	if (RoadValues && (bImportRoads || bImportTrafficLanes))
	{
		Stats.RoadsInFile = RoadValues->Num();
		for (int32 RoadIndex = 0; RoadIndex < RoadValues->Num(); ++RoadIndex)
		{
			FRoadData Road;
			int32 BadPoints = 0;
			FString Error;
			if (!ParseRoad((*RoadValues)[RoadIndex], Road, BadPoints, Error))
			{
				++Stats.RoadsMalformed;
				Warn(Stats, FString::Printf(TEXT("road #%d skipped: %s"), RoadIndex, *Error));
				continue;
			}
			if (BadPoints > 0)
			{
				Stats.PointsInvalid += BadPoints;
				Warn(Stats, FString::Printf(TEXT("road #%d (%s '%s'): %d empty/invalid/out-of-range point(s) skipped"), RoadIndex, *Road.Id, *Road.Name, BadPoints));
			}
			if (Road.XY.Num() < 2)
			{
				++Stats.RoadsTooShort;
				Warn(Stats, FString::Printf(TEXT("road #%d (%s '%s') skipped: fewer than 2 valid points"), RoadIndex, *Road.Id, *Road.Name));
				continue;
			}

			// Zone filter: keep runs of points inside the zone (+1 point on each side for continuity).
			TArray<TArray<int32>> Runs;
			{
				TArray<bool> Inside;
				Inside.SetNumZeroed(Road.XY.Num());
				int32 InsideCount = 0;
				for (int32 i = 0; i < Road.XY.Num(); ++i)
				{
					Inside[i] = InZone(Road.XY[i], ZoneHint);
					InsideCount += Inside[i] ? 1 : 0;
				}
				if (InsideCount == 0)
				{
					++Stats.RoadsOutsideZone;
					continue;
				}
				if (!bTrimRoadsToZone || InsideCount == Road.XY.Num())
				{
					TArray<int32> All;
					for (int32 i = 0; i < Road.XY.Num(); ++i) { All.Add(i); }
					Runs.Add(MoveTemp(All));
				}
				else
				{
					++Stats.RoadsTrimmed;
					TArray<int32> Current;
					for (int32 i = 0; i < Road.XY.Num(); ++i)
					{
						const bool bKeep = Inside[i] || (i > 0 && Inside[i - 1]) || (i + 1 < Road.XY.Num() && Inside[i + 1]);
						if (bKeep)
						{
							Current.Add(i);
						}
						else if (Current.Num() > 0)
						{
							Runs.Add(MoveTemp(Current));
							Current.Reset();
						}
					}
					if (Current.Num() > 0)
					{
						Runs.Add(MoveTemp(Current));
					}
				}
			}

			const float CarriagewayWidth = LaneWidthCm * static_cast<float>(Road.Lanes);
			const bool bLink = Road.Highway.EndsWith(TEXT("_link"));
			const bool bTraffic = bImportTrafficLanes && TrafficClasses.Contains(Road.Highway.Replace(TEXT("_link"), TEXT("")));

			for (const TArray<int32>& Run : Runs)
			{
				if (Run.Num() < 2)
				{
					continue;
				}
				TArray<FVector> Points;
				Points.Reserve(Run.Num());
				for (const int32 i : Run)
				{
					Points.Add(ToWorld(Road.XY[i].X, Road.XY[i].Y, Road.Levels[i], &Stats));
				}
				if (Road.bOneWay && !bLink && bSeparateDualCarriageways)
				{
					Points = OffsetPolyline(Points, CarriagewayWidth * 0.5f + MedianWidthCm * 0.5f);
				}
				for (const FVector& P : Points)
				{
					Stats.Bounds += P;
				}

				if (bImportRoads)
				{
					for (const TArray<FVector>& Piece : SplitByLength(Points, MaxRoadPieceCm))
					{
						++Stats.RoadPieces;
						if (!bSpawn)
						{
							continue;
						}
						if (ADTGeoRoad* GeoRoad = SpawnTagged<ADTGeoRoad>(RoadClass, Piece[0], FRotator::ZeroRotator, TEXT("Geo/Roads/") + Road.Highway, ExtraTags))
						{
							GeoRoad->RoadName = Road.Name;
							GeoRoad->Highway = Road.Highway;
							GeoRoad->Lanes = Road.Lanes;
							GeoRoad->bOneWay = Road.bOneWay;
							GeoRoad->Build(Piece, CarriagewayWidth, RoadMesh);
#if WITH_EDITOR
							GeoRoad->SetActorLabel(FString::Printf(TEXT("Road_%s_%d"), *Road.Highway, Stats.RoadPieces));
#endif
						}
					}
				}

				if (bTraffic)
				{
					// Thailand drives on the LEFT. One AI lane per direction, in the middle of that direction's half.
					TArray<TArray<FVector>> Directions;
					if (Road.bOneWay)
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
						++Stats.Lanes;
						if (!bSpawn)
						{
							continue;
						}
						if (ADTTrafficLane* Lane = SpawnTagged<ADTTrafficLane>(LaneClass, DirPoints[0], FRotator::ZeroRotator, TEXT("Geo/TrafficLanes"), ExtraTags))
						{
							Lane->InitializeFromPoints(DirPoints, Road.SpeedKmh);
							NewLanes.Add(Lane);
						}
					}
				}
			}
		}
	}

	if (NewLanes.Num() > 0)
	{
		Stats.LaneLinks = LinkLanes(NewLanes);
	}

	// --- Border signs -------------------------------------------------------------------------------------------
	if (CrossingValues && bImportSigns)
	{
		Stats.CrossingsInFile = CrossingValues->Num();
		for (int32 Index = 0; Index < CrossingValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* CrossingPtr = nullptr;
			const TSharedPtr<FJsonValue>& Value = (*CrossingValues)[Index];
			double X = 0.0, Y = 0.0, Heading = 0.0;
			FString From, To, RoadName;
			const bool bOk = Value.IsValid() && Value->TryGetObject(CrossingPtr) && CrossingPtr && CrossingPtr->IsValid()
				&& (*CrossingPtr)->TryGetNumberField(TEXT("x"), X) && (*CrossingPtr)->TryGetNumberField(TEXT("y"), Y)
				&& (*CrossingPtr)->TryGetStringField(TEXT("to"), To) && !To.IsEmpty()
				&& FMath::IsFinite(X) && FMath::IsFinite(Y)
				&& FMath::Abs(X) < FDTProvinceGeometry::MaxAbsCoordinateCm && FMath::Abs(Y) < FDTProvinceGeometry::MaxAbsCoordinateCm;
			if (!bOk)
			{
				++Stats.CrossingsMalformed;
				Warn(Stats, FString::Printf(TEXT("crossing #%d skipped: missing/invalid x, y or 'to'"), Index));
				continue;
			}
			(*CrossingPtr)->TryGetNumberField(TEXT("heading"), Heading);
			(*CrossingPtr)->TryGetStringField(TEXT("from"), From);
			(*CrossingPtr)->TryGetStringField(TEXT("road"), RoadName);
			if (!FMath::IsFinite(Heading))
			{
				Heading = 0.0;
			}

			if (!bAll && !ZoneSet.Contains(FName(*To)) && !ZoneSet.Contains(FName(*From)))
			{
				++Stats.CrossingsOutsideZone;
				continue;
			}

			const FVector Forward = FRotator(0.0, Heading, 0.0).Vector();
			const FVector Left(Forward.Y, -Forward.X, 0.0);
			// Slightly before the border, on the driver's left, facing oncoming (approaching) traffic.
			const FVector Spot = FVector(X, Y, 0.0) - Forward * 1500.0 + Left * static_cast<double>(SignSideOffsetCm);
			const FVector Location = ToWorld(Spot.X, Spot.Y, 0, &Stats);
			Stats.Bounds += Location;
			++Stats.Signs;
			if (!bSpawn)
			{
				continue;
			}
			if (ADTProvinceSign* Sign = SpawnTagged<ADTProvinceSign>(SignClass, Location, FRotator(0.0, Heading + 180.0, 0.0), TEXT("Geo/ProvinceSigns"), ExtraTags))
			{
				Sign->ProvinceCode = FName(*To);
				Sign->FromProvinceCode = FName(*From);
				Sign->RoadName = RoadName;
			}
		}
	}

	// --- Coastline -----------------------------------------------------------------------------------------------
	if (CoastRoot.IsValid())
	{
		double SeaLevel = 0.0;
		CoastRoot->TryGetNumberField(TEXT("sea_level_z"), SeaLevel);
		const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
		if (!CoastRoot->TryGetArrayField(TEXT("lines"), Lines) || !Lines)
		{
			Warn(Stats, FString::Printf(TEXT("coast file %s has no 'lines' array"), *CoastFile));
		}
		else
		{
			const FBox2D ZoneBox = bAll ? FBox2D(ForceInit) : Geometry.GetBounds(ZoneCodes).ExpandBy(50000.0);
			Stats.CoastLinesInFile = Lines->Num();
			for (int32 Index = 0; Index < Lines->Num(); ++Index)
			{
				const TSharedPtr<FJsonObject>* LinePtr = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* PointValues = nullptr;
				if (!(*Lines)[Index].IsValid() || !(*Lines)[Index]->TryGetObject(LinePtr) || !LinePtr || !LinePtr->IsValid()
					|| !(*LinePtr)->TryGetArrayField(TEXT("points"), PointValues) || !PointValues)
				{
					++Stats.CoastLinesMalformed;
					Warn(Stats, FString::Printf(TEXT("coast line #%d skipped: not an object with 'points'"), Index));
					continue;
				}
				bool bClosed = false;
				(*LinePtr)->TryGetBoolField(TEXT("closed"), bClosed);

				TArray<FVector> Points;
				bool bTouchesZone = bAll;
				int32 BadPoints = 0;
				for (const TSharedPtr<FJsonValue>& PV : *PointValues)
				{
					FVector2D P;
					if (!ParsePoint2D(PV, P))
					{
						++BadPoints;
						continue;
					}
					bTouchesZone = bTouchesZone || ZoneBox.IsInside(P);
					Points.Add(FVector(P.X, P.Y, SeaLevel + static_cast<double>(CoastZOffsetCm)));
				}
				Stats.PointsInvalid += BadPoints;
				if (Points.Num() < 2)
				{
					++Stats.CoastLinesMalformed;
					Warn(Stats, FString::Printf(TEXT("coast line #%d skipped: fewer than 2 valid points (%d invalid)"), Index, BadPoints));
					continue;
				}
				if (!bTouchesZone)
				{
					++Stats.CoastLinesOutsideZone;
					continue;
				}

				const TArray<TArray<FVector>> Pieces = SplitByLength(Points, MaxRoadPieceCm);
				for (const TArray<FVector>& Piece : Pieces)
				{
					for (const FVector& P : Piece)
					{
						Stats.Bounds += P;
					}
					++Stats.CoastPieces;
					if (!bSpawn)
					{
						continue;
					}
					if (ADTGeoCoastline* Coast = SpawnTagged<ADTGeoCoastline>(CoastClass, Piece[0], FRotator::ZeroRotator, TEXT("Geo/Coast"), ExtraTags))
					{
						Coast->Build(Piece, bClosed && Pieces.Num() == 1);
					}
				}
			}
		}
	}

	// --- Summary ------------------------------------------------------------------------------------------------
	const TCHAR* Verb = bSpawn ? TEXT("spawned") : TEXT("would spawn");
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Roads in file %d -> %s %d road pieces, %d traffic lanes (%d junction links)"),
		Stats.RoadsInFile, Verb, Stats.RoadPieces, Stats.Lanes, Stats.LaneLinks);
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport]   skipped roads: %d outside zone, %d malformed, %d with < 2 valid points; %d roads trimmed at the zone border; %d invalid points dropped"),
		Stats.RoadsOutsideZone, Stats.RoadsMalformed, Stats.RoadsTooShort, Stats.RoadsTrimmed, Stats.PointsInvalid);
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Crossings in file %d -> %s %d province signs (%d outside zone, %d malformed)"),
		Stats.CrossingsInFile, Verb, Stats.Signs, Stats.CrossingsOutsideZone, Stats.CrossingsMalformed);
	if (CoastRoot.IsValid())
	{
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Coast lines in file %d -> %s %d coastline pieces at sea level (%d outside zone, %d malformed)"),
			Stats.CoastLinesInFile, Verb, Stats.CoastPieces, Stats.CoastLinesOutsideZone, Stats.CoastLinesMalformed);
	}

	if (bTraceGround)
	{
		const int32 Traces = Stats.GroundHits + Stats.GroundMisses;
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Ground trace: %d/%d points hit the ground, %d used BaseZ=%.0f"), Stats.GroundHits, Traces, Stats.GroundMisses, BaseZ);
		if (Traces > 0 && Stats.GroundHits == 0)
		{
			UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] No ground under ANY point: the landscape is missing, does not cover this zone (use Preview Zone), or its World Partition cells are not loaded."));
		}
	}

	if (Stats.Bounds.IsValid)
	{
		const double Scale = Settings->HorizontalScale;
		const FVector Size = Stats.Bounds.GetSize();
		const FVector2D LLMin = UDTGeoLibrary::WorldToLatLon(Stats.Bounds.Min);
		const FVector2D LLMax = UDTGeoLibrary::WorldToLatLon(Stats.Bounds.Max);
		UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] Result bounds X %.0f..%.0f, Y %.0f..%.0f, Z %.0f..%.0f cm = %s x %s in game (%.0f x %.0f km real) | lat %.4f..%.4f, lon %.4f..%.4f"),
			Stats.Bounds.Min.X, Stats.Bounds.Max.X, Stats.Bounds.Min.Y, Stats.Bounds.Max.Y, Stats.Bounds.Min.Z, Stats.Bounds.Max.Z,
			*DTGeoImport::Km(Size.X), *DTGeoImport::Km(Size.Y), Size.X / 100000.0 * Scale, Size.Y / 100000.0 * Scale,
			LLMin.X, LLMax.X, LLMin.Y, LLMax.Y);
	}
	else
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] Nothing inside zone '%s' - check ZoneFilter, the file area and bTrimRoadsToZone."), *GetZoneLabel());
	}

	if (Stats.WarningsSuppressed > 0)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[GeoImport] %d more warning(s) not shown (MaxDetailedWarnings = %d)."), Stats.WarningsSuppressed, MaxDetailedWarnings);
	}

	const double Elapsed = FPlatformTime::Seconds() - StartTime;
	LastRunSummary = FString::Printf(TEXT("%s | zone '%s' | %d roads, %d lanes, %d signs, %d coast | %d skipped | %.2f s"),
		*Mode, *GetZoneLabel(), Stats.RoadPieces, Stats.Lanes, Stats.Signs, Stats.CoastPieces,
		Stats.RoadsMalformed + Stats.RoadsTooShort + Stats.CrossingsMalformed + Stats.CoastLinesMalformed, Elapsed);
	UE_LOG(LogDTGeo, Display, TEXT("[GeoImport] ===== done in %.2f s%s ====="), Elapsed,
		bSpawn ? *FString::Printf(TEXT(" | undo with 'Clear Last Run' (tag %s) or 'Clear Zone' (tag %s)"), *ExtraTags[0].ToString(), *ExtraTags[1].ToString()) : TEXT(""));
}
