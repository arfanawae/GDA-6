"""
Lat/lon <-> Unreal world coordinates for the scaled Thailand map.

Projection: spherical Transverse Mercator centred on (origin_lat, origin_lon).
  * conformal -> province shapes and road angles are preserved
  * ~0.3% distance error vs the WGS84 ellipsoid, invisible after 1:20 scaling
  * the exact same formulas are implemented in C++ (UDTGeoLibrary), so data
    generated here and positions computed at runtime always agree

Unreal axes (left-handed, Z up):  +X = North, +Y = East, units = cm.
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

EARTH_RADIUS_M = 6371008.8  # mean Earth radius (IUGG)


@dataclass(frozen=True)
class GeoConfig:
    origin_lat: float = 13.0
    origin_lon: float = 101.0
    horizontal_scale: float = 20.0  # 20 -> 100 km real = 5 km in game
    vertical_scale: float = 10.0    # 2x exaggerated vs horizontal: mountains read well, roads stay drivable (see build_heightmap.py --roads)

    @staticmethod
    def load(path: Path | None = None) -> "GeoConfig":
        path = path or Path(__file__).with_name("geo_config.json")
        data = json.loads(path.read_text(encoding="utf-8"))
        return GeoConfig(**{k: v for k, v in data.items() if not k.startswith("_")})


def latlon_to_tm(lat: float, lon: float, cfg: GeoConfig) -> tuple[float, float]:
    """Returns real-world metres (easting, northing) relative to the origin."""
    phi = math.radians(lat)
    dlam = math.radians(lon - cfg.origin_lon)
    phi0 = math.radians(cfg.origin_lat)
    b = math.cos(phi) * math.sin(dlam)
    easting = 0.5 * EARTH_RADIUS_M * math.log((1.0 + b) / (1.0 - b))
    northing = EARTH_RADIUS_M * (math.atan2(math.tan(phi), math.cos(dlam)) - phi0)
    return easting, northing


def tm_to_latlon(easting: float, northing: float, cfg: GeoConfig) -> tuple[float, float]:
    d = northing / EARTH_RADIUS_M + math.radians(cfg.origin_lat)
    xr = easting / EARTH_RADIUS_M
    lat = math.degrees(math.asin(math.sin(d) / math.cosh(xr)))
    lon = cfg.origin_lon + math.degrees(math.atan2(math.sinh(xr), math.cos(d)))
    return lat, lon


def latlon_to_ue(lat: float, lon: float, cfg: GeoConfig) -> tuple[float, float]:
    """Returns Unreal (X=north, Y=east) in centimetres, already scaled down."""
    easting, northing = latlon_to_tm(lat, lon, cfg)
    s = 100.0 / cfg.horizontal_scale
    return northing * s, easting * s


def ue_to_latlon(x_cm: float, y_cm: float, cfg: GeoConfig) -> tuple[float, float]:
    s = cfg.horizontal_scale / 100.0
    return tm_to_latlon(y_cm * s, x_cm * s, cfg)


def elevation_to_ue_z(height_m: float, cfg: GeoConfig) -> float:
    return height_m * 100.0 / cfg.vertical_scale


if __name__ == "__main__":
    cfg = GeoConfig.load()
    # Sanity check: Bangkok (Democracy Monument) -> game -> back
    lat, lon = 13.7567, 100.5018
    x, y = latlon_to_ue(lat, lon, cfg)
    print(f"Bangkok -> UE X={x:.0f} cm, Y={y:.0f} cm  ({x/1e5:.2f} km N, {y/1e5:.2f} km E of origin)")
    print("round trip:", ue_to_latlon(x, y, cfg))
    # Scale check: 1 degree of latitude ~ 111.2 km real -> ~5.56 km in game
    x2, _ = latlon_to_ue(lat + 1.0, lon, cfg)
    print(f"1 deg latitude in game = {(x2 - x)/1e5:.3f} km")
