#include "World/Geo/DTGeoLibrary.h"
#include "World/Geo/DTGeoSettings.h"

namespace DTGeo
{
	static constexpr double EarthRadiusM = 6371008.8; // must match geo_projection.py
}

FVector UDTGeoLibrary::LatLonToWorld(double Latitude, double Longitude)
{
	const UDTGeoSettings* S = GetDefault<UDTGeoSettings>();
	const double Phi = FMath::DegreesToRadians(Latitude);
	const double DLam = FMath::DegreesToRadians(Longitude - S->OriginLon);
	const double Phi0 = FMath::DegreesToRadians(S->OriginLat);

	const double B = FMath::Cos(Phi) * FMath::Sin(DLam);
	const double Easting = 0.5 * DTGeo::EarthRadiusM * FMath::Loge((1.0 + B) / (1.0 - B));
	const double Northing = DTGeo::EarthRadiusM * (FMath::Atan2(FMath::Tan(Phi), FMath::Cos(DLam)) - Phi0);

	const double ToGame = 100.0 / S->HorizontalScale;
	return FVector(Northing * ToGame, Easting * ToGame, 0.0);
}

FVector2D UDTGeoLibrary::WorldToLatLon(const FVector& WorldLocation)
{
	const UDTGeoSettings* S = GetDefault<UDTGeoSettings>();
	const double ToReal = S->HorizontalScale / 100.0;
	const double Easting = WorldLocation.Y * ToReal;
	const double Northing = WorldLocation.X * ToReal;

	const double D = Northing / DTGeo::EarthRadiusM + FMath::DegreesToRadians(S->OriginLat);
	const double Xr = Easting / DTGeo::EarthRadiusM;
	const double Lat = FMath::RadiansToDegrees(FMath::Asin(FMath::Sin(D) / FMath::Cosh(Xr)));
	const double Lon = S->OriginLon + FMath::RadiansToDegrees(FMath::Atan2(FMath::Sinh(Xr), FMath::Cos(D)));
	return FVector2D(Lat, Lon);
}

double UDTGeoLibrary::RealDistanceKm(const FVector& A, const FVector& B)
{
	return FVector::Dist2D(A, B) / 100.0 * GetDefault<UDTGeoSettings>()->HorizontalScale / 1000.0;
}

double UDTGeoLibrary::ElevationToWorldZ(double HeightMeters)
{
	return HeightMeters * 100.0 / GetDefault<UDTGeoSettings>()->VerticalScale;
}

FString UDTGeoLibrary::FormatLatLon(const FVector& WorldLocation)
{
	const FVector2D LL = WorldToLatLon(WorldLocation);
	return FString::Printf(TEXT("%.4f°%s %.4f°%s"), FMath::Abs(LL.X), LL.X >= 0 ? TEXT("N") : TEXT("S"), FMath::Abs(LL.Y), LL.Y >= 0 ? TEXT("E") : TEXT("W"));
}
