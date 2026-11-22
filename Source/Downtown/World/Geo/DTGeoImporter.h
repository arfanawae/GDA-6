#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Province/DTProvinceGeometry.h"
#include "DTGeoImporter.generated.h"

class ADTGeoRoad;
class ADTTrafficLane;
class ADTProvinceSign;
class ADTGeoCoastline;
class UStaticMesh;
class UBillboardComponent;
class UDataTable;
class FJsonObject;
class FJsonValue;

/** What to remove before a new import. */
UENUM()
enum class EDTGeoClearMode : uint8
{
	/** Keep everything (stack several zones in one level). */
	None,
	/** Remove actors from a previous import of the same zone (default: quick re-test of one zone). */
	SameZone,
	/** Remove every actor any importer spawned. */
	All
};

/**
 * Drop one into the level, point it at a Roads_<Area>.json from Tools/GeoPipeline/build_roads.py
 * and press "Import" in the Details panel. Spawns:
 *   - ADTGeoRoad pieces (visual, chunked for World Partition)
 *   - ADTTrafficLane graph for the existing traffic AI (left-hand traffic!)
 *   - ADTProvinceSign at every road/province-border crossing
 *   - ADTGeoCoastline splines from CoastFile (optional, build_heightmap.py --sea)
 *
 * Testing workflow (see Docs/ThailandMap.md):
 *   1. Set ZoneFilter ("test-area", "TH-12", "Nonthaburi", "north", empty = everything in the file)
 *   2. Validate Data   -> parses everything, logs what WOULD be spawned (nothing changes)
 *   3. Preview Zone    -> draws the zone's province outlines in the viewport + logs bounds
 *   4. Import          -> spawns; every actor is tagged with the run number and zone
 *   5. Clear Last Run / Clear Zone / Clear Imported -> undo quickly and try again
 * All output goes to the LogDTGeo category of the Output Log.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, HLOD, Input, Actor, Cooking, Replication, Networking))
class DOWNTOWN_API ADTGeoImporter : public AActor
{
	GENERATED_BODY()

public:
	ADTGeoImporter();

	/** Parse RoadsFile and spawn roads, lanes and signs for ZoneFilter. */
	UFUNCTION(CallInEditor, Category = "Geo Import")
	void Import();

	/** Same checks as Import (file, projection, zone, coordinates) but spawns nothing. */
	UFUNCTION(CallInEditor, Category = "Geo Import")
	void ValidateData();

	/** Logs the provinces of ZoneFilter and draws their outlines in the viewport for PreviewSeconds. */
	UFUNCTION(CallInEditor, Category = "Geo Import")
	void PreviewZone();

	/** Removes the actors spawned by the most recent Import of this importer. */
	UFUNCTION(CallInEditor, Category = "Geo Import")
	void ClearLastRun();

	/** Removes imported actors that belong to the current ZoneFilter. */
	UFUNCTION(CallInEditor, Category = "Geo Import")
	void ClearZone();

	/** Removes every actor spawned by any DTGeoImporter (in loaded World Partition cells). */
	UFUNCTION(CallInEditor, Category = "Geo Import")
	void ClearImported();

	/** Path relative to the project's Content folder. Empty = no roads/lanes/signs (e.g. coast only). */
	UPROPERTY(EditAnywhere, Category = "Geo Import")
	FString RoadsFile = TEXT("_Downtown/Data/Roads/Roads_TestArea.json");

	/** Optional Coast_<name>.json (relative to Content) from build_heightmap.py --sea. */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Coast")
	FString CoastFile;

	UPROPERTY(EditAnywhere, Category = "Geo Import|Coast") bool bImportCoast = true;
	UPROPERTY(EditAnywhere, Category = "Geo Import|Coast") TSubclassOf<ADTGeoCoastline> CoastClass;

	/** Height of the coastline splines above sea level (world Z 0 by the pipeline convention). */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Coast") float CoastZOffsetCm = 0.f;

	/**
	 * Which provinces to import. Empty or "all" = everything in RoadsFile.
	 * Examples: "test-area", "bangkok-metro", "TH-12", "Nonthaburi", "TH-10,TH-13", "north".
	 */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Zone")
	FString ZoneFilter = TEXT("test-area");

	/** DT_Provinces (row struct DTProvinceRow). Needed for zone filtering and Preview Zone. */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Zone", meta = (RequiredAssetDataTags = "RowStructure=/Script/Downtown.DTProvinceRow"))
	TSoftObjectPtr<UDataTable> ProvinceTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/_Downtown/Data/Provinces/DT_Provinces.DT_Provinces")));

	/** Cut roads at the zone border (true) or keep whole roads that touch the zone (false). */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Zone")
	bool bTrimRoadsToZone = true;

	UPROPERTY(EditAnywhere, Category = "Geo Import|Testing")
	EDTGeoClearMode ClearBeforeImport = EDTGeoClearMode::SameZone;

	/** Log and count everything but spawn nothing. */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Testing")
	bool bDryRun = false;

	/** Per-item warnings logged in full before they are only counted. */
	UPROPERTY(EditAnywhere, Category = "Geo Import|Testing", meta = (ClampMin = "0"))
	int32 MaxDetailedWarnings = 10;

	UPROPERTY(EditAnywhere, Category = "Geo Import|Testing", meta = (ClampMin = "1"))
	float PreviewSeconds = 30.f;

	/** Result of the last Import / Validate (also in the Output Log). */
	UPROPERTY(VisibleAnywhere, Category = "Geo Import|Testing")
	FString LastRunSummary;

	UPROPERTY(VisibleAnywhere, Category = "Geo Import|Testing")
	int32 LastRunId = 0;

	UPROPERTY(EditAnywhere, Category = "Geo Import") bool bImportRoads = true;
	UPROPERTY(EditAnywhere, Category = "Geo Import") bool bImportTrafficLanes = true;
	UPROPERTY(EditAnywhere, Category = "Geo Import") bool bImportSigns = true;

	/** Only these OSM classes become traffic lanes (the rest are visual only). */
	UPROPERTY(EditAnywhere, Category = "Geo Import")
	TArray<FString> TrafficClasses = { TEXT("motorway"), TEXT("trunk"), TEXT("primary") };

	UPROPERTY(EditAnywhere, Category = "Geo Import") TSubclassOf<ADTGeoRoad> RoadClass;
	UPROPERTY(EditAnywhere, Category = "Geo Import") TSubclassOf<ADTTrafficLane> LaneClass;
	UPROPERTY(EditAnywhere, Category = "Geo Import") TSubclassOf<ADTProvinceSign> SignClass;

	/** Optional road surface mesh (1 m wide along Y, along +X). Empty = splines only. */
	UPROPERTY(EditAnywhere, Category = "Geo Import") TObjectPtr<UStaticMesh> RoadMesh;

	/** Lane width stays REAL size (cars are real size; only distances are scaled). */
	UPROPERTY(EditAnywhere, Category = "Geo Import") float LaneWidthCm = 350.f;

	/**
	 * OSM maps divided highways as two one-way lines ~20 m apart; at 1:20 they collapse to ~1 m and overlap.
	 * When on, every one-way (non-ramp) carriageway is pushed to its own LEFT by half its width + half the
	 * median - with left-hand traffic the two directions separate naturally.
	 */
	UPROPERTY(EditAnywhere, Category = "Geo Import") bool bSeparateDualCarriageways = true;
	UPROPERTY(EditAnywhere, Category = "Geo Import") float MedianWidthCm = 300.f;

	/** Split roads into pieces no longer than this so World Partition can stream them. */
	UPROPERTY(EditAnywhere, Category = "Geo Import") float MaxRoadPieceCm = 25600.f;

	/** Lane ends closer than this to another lane start are linked (junctions). */
	UPROPERTY(EditAnywhere, Category = "Geo Import") float ConnectRadiusCm = 1500.f;

	/** Trace down to put roads on the landscape; otherwise use BaseZ. */
	UPROPERTY(EditAnywhere, Category = "Geo Import") bool bTraceGround = true;
	UPROPERTY(EditAnywhere, Category = "Geo Import") float BaseZ = 0.f;

	/** Signs stand this far to the LEFT of the carriageway (Thailand drives on the left). */
	UPROPERTY(EditAnywhere, Category = "Geo Import") float SignSideOffsetCm = 1200.f;

protected:
	/** One parsed road from the JSON (game XY in cm + OSM level per point). */
	struct FRoadData
	{
		FString Id;
		FString Name;
		FString Highway;
		bool bOneWay = false;
		int32 Lanes = 1;
		float SpeedKmh = 50.f;
		TArray<FVector2D> XY;
		TArray<int32> Levels;
	};

	/** Counters for one Import / Validate run. */
	struct FImportStats
	{
		int32 RoadsInFile = 0;
		int32 RoadsMalformed = 0;
		int32 RoadsTooShort = 0;
		int32 RoadsOutsideZone = 0;
		int32 RoadsTrimmed = 0;
		int32 PointsInvalid = 0;
		int32 RoadPieces = 0;
		int32 Lanes = 0;
		int32 LaneLinks = 0;
		int32 CrossingsInFile = 0;
		int32 CrossingsMalformed = 0;
		int32 CrossingsOutsideZone = 0;
		int32 Signs = 0;
		int32 CoastLinesInFile = 0;
		int32 CoastLinesMalformed = 0;
		int32 CoastLinesOutsideZone = 0;
		int32 CoastPieces = 0;
		int32 GroundHits = 0;
		int32 GroundMisses = 0;
		int32 WarningsLogged = 0;
		int32 WarningsSuppressed = 0;
		FBox Bounds = FBox(ForceInit);
	};

	void RunImport(bool bSpawn);
	bool LoadProvinceGeometry(FDTProvinceGeometry& OutGeometry, bool bRequired) const;
	bool ResolveZoneFilter(const FDTProvinceGeometry& Geometry, TArray<FName>& OutCodes, bool& bOutAll) const;
	FString GetZoneLabel() const;
	bool ParseRoad(const TSharedPtr<FJsonValue>& Value, FRoadData& Out, int32& OutBadPoints, FString& OutError) const;
	static bool ParsePoint2D(const TSharedPtr<FJsonValue>& Value, FVector2D& Out);

	/** Reads, parses and validates a pipeline JSON (projection + area coverage). False = abort the run. */
	bool LoadGeoFile(const FString& RelativePath, const TCHAR* Kind, const TArray<FName>& ZoneCodes, bool bAll, TSharedPtr<FJsonObject>& OutRoot) const;
	void Warn(FImportStats& Stats, const FString& Message) const;
	int32 ClearByTag(FName Tag, const TCHAR* What);

	FVector ToWorld(double X, double Y, int32 Level, FImportStats* Stats) const;
	static TArray<TArray<FVector>> SplitByLength(const TArray<FVector>& Points, float MaxLength);
	static TArray<FVector> OffsetPolyline(const TArray<FVector>& Points, float LeftOffset);
	int32 LinkLanes(const TArray<ADTTrafficLane*>& Lanes) const;
	template <typename T> T* SpawnTagged(TSubclassOf<T> Class, const FVector& Location, const FRotator& Rotation, const FString& Folder, const TArray<FName>& ExtraTags);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
