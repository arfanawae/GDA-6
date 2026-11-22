#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DTGeoSettings.generated.h"

/**
 * Projection constants for the scaled Thailand map (Project Settings -> Game -> Downtown Geo).
 * MUST match Tools/GeoPipeline/geo_config.json, otherwise imported data and runtime
 * lat/lon conversions disagree.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Downtown Geo"))
class DOWNTOWN_API UDTGeoSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDTGeoSettings() { CategoryName = TEXT("Game"); }

	/** World origin (0,0,0) in WGS84 degrees. Centre of Thailand keeps coordinates small. */
	UPROPERTY(Config, EditAnywhere, Category = "Projection") double OriginLat = 13.0;
	UPROPERTY(Config, EditAnywhere, Category = "Projection") double OriginLon = 101.0;

	/** Real metres per game metre horizontally (20 -> 100 km real = 5 km in game). */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = "1")) double HorizontalScale = 20.0;

	/**
	 * Real metres per game metre for terrain height. Smaller than HorizontalScale so mountains still read as
	 * mountains, but every slope is steepened by HorizontalScale / VerticalScale (10 -> x2). Measured on the
	 * Chiang Mai / Lamphun / Lampang main roads: 1:5 makes 14% of road length steeper than 15%, 1:10 makes 5%.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Projection", meta = (ClampMin = "1")) double VerticalScale = 10.0;

	/** Height of one OSM "layer" (elevated expressway level). Structures keep real size, so this is NOT scaled. */
	UPROPERTY(Config, EditAnywhere, Category = "Roads") float RoadLevelHeightCm = 700.f;
};
