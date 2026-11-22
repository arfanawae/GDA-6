#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DTGeoLibrary.generated.h"

/**
 * Lat/lon <-> world conversion. Spherical Transverse Mercator centred on the UDTGeoSettings origin,
 * identical to Tools/GeoPipeline/geo_projection.py. World axes: +X = north, +Y = east, cm.
 */
UCLASS()
class DOWNTOWN_API UDTGeoLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** WGS84 degrees -> world location (Z = 0). */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FVector LatLonToWorld(double Latitude, double Longitude);

	/** World location -> WGS84 degrees (X = latitude, Y = longitude). */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FVector2D WorldToLatLon(const FVector& WorldLocation);

	/** Distance a player would have travelled in the real country (km), ignoring height. */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static double RealDistanceKm(const FVector& A, const FVector& B);

	/** Real terrain height (m) -> world Z (cm) using the vertical scale. */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static double ElevationToWorldZ(double HeightMeters);

	/** "13.7563°N 100.5018°E" for debug HUDs. */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FString FormatLatLon(const FVector& WorldLocation);
};
