#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTGeoImporter.generated.h"

class ADTGeoRoad;
class ADTTrafficLane;
class ADTProvinceSign;
class UStaticMesh;
class UBillboardComponent;

/**
 * Drop one into the level, point it at a Roads_<Area>.json from Tools/GeoPipeline/build_roads.py
 * and press "Import" in the Details panel. Spawns:
 *   - ADTGeoRoad pieces (visual, chunked for World Partition)
 *   - ADTTrafficLane graph for the existing traffic AI (left-hand traffic!)
 *   - ADTProvinceSign at every road/province-border crossing
 * "Clear Imported" removes everything it spawned.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, HLOD, Input, Actor, Cooking, Replication, Networking))
class DOWNTOWN_API ADTGeoImporter : public AActor
{
	GENERATED_BODY()

public:
	ADTGeoImporter();

	UFUNCTION(CallInEditor, Category = "Geo Import")
	void Import();

	UFUNCTION(CallInEditor, Category = "Geo Import")
	void ClearImported();

	/** Path relative to the project's Content folder. */
	UPROPERTY(EditAnywhere, Category = "Geo Import")
	FString RoadsFile = TEXT("_Downtown/Data/Roads/Roads_TestArea.json");

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
	FVector ToWorld(double X, double Y, int32 Level) const;
	static TArray<TArray<FVector>> SplitByLength(const TArray<FVector>& Points, float MaxLength);
	static TArray<FVector> OffsetPolyline(const TArray<FVector>& Points, float LeftOffset);
	void LinkLanes(const TArray<ADTTrafficLane*>& Lanes) const;
	template <typename T> T* SpawnTagged(TSubclassOf<T> Class, const FVector& Location, const FRotator& Rotation, const FString& Folder);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
