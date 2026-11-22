#pragma once

#include "CoreMinimal.h"
#include "World/Province/DTProvinceTypes.h"

class UDataTable;

/**
 * Plain C++ (non-UObject) province polygon index built from DT_Provinces.
 * Shared by UDTProvinceSubsystem (runtime) and ADTGeoImporter (editor), so zone filtering
 * and province detection always use the same rules.
 *
 * Zone strings (case-insensitive, comma separated):
 *   "" / "all"                -> every province in the table
 *   "bangkok-metro"           -> TH-10, TH-11, TH-12, TH-13, TH-73, TH-74
 *   "test-area"               -> TH-10, TH-12, TH-13
 *   "north" ... "south"       -> a region (6-region system)
 *   "TH-12", "TH12", "12"     -> a province code
 *   Thai or English name      -> matched against NameTH / NameEN (spaces, '-', '.' ignored)
 */
struct DOWNTOWN_API FDTProvinceGeometry
{
	struct FEntry
	{
		FDTProvinceRow Row;
		FBox2D Bounds = FBox2D(ForceInit);
		TArray<FBox2D> RingBounds;
	};

	/** Coordinates beyond this (cm) are treated as broken data (Thailand at 1:20 is about +-45 km). */
	static constexpr double MaxAbsCoordinateCm = 10000000.0; // 100 km

	/**
	 * Rebuilds the index. Invalid rows/rings (too few points, NaN, far outside the map) are skipped
	 * and described in OutWarnings. Returns the number of provinces loaded.
	 */
	int32 Build(const UDataTable* Table, TArray<FString>& OutWarnings);

	void Reset() { Entries.Reset(); }
	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.Num() == 0; }

	const FEntry* Find(FName Code) const { return Entries.Find(Code); }
	const TMap<FName, FEntry>& GetEntries() const { return Entries; }

	bool Contains(FName Code, const FVector2D& Point) const;

	/** Current/hinted province first, then its neighbours, then everything (optionally limited to Allowed). */
	FName FindAt(const FVector2D& Point, FName Hint = NAME_None, const TSet<FName>* Allowed = nullptr) const;

	/** Union of the provinces' bounds (cm). Invalid box if none are known. */
	FBox2D GetBounds(const TArray<FName>& Codes) const;

	/** Resolves a zone string to province codes. Returns false with a readable error for unknown tokens. */
	bool ResolveZone(const FString& Zone, TArray<FName>& OutCodes, FString& OutError) const;

	/** "TH-12 Nonthaburi" for logs (ASCII-only, safe in any console). */
	FString Describe(FName Code) const;

	/** Names of the built-in zone presets, for help/log output. */
	static FString GetZoneHelp();

	static bool PointInRing(const TArray<FVector2D>& Ring, const FVector2D& Point);
	static FString RegionToString(EDTThaiRegion Region);

private:
	static FString NormalizeName(const FString& In);
	static FName NormalizeCode(const FString& Token);

	TMap<FName, FEntry> Entries;
};
