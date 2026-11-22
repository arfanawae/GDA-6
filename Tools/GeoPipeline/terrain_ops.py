"""
Terrain operations on a landscape height grid, used by build_heightmap.py.

Grid convention (same as the Unreal heightmap):
    z[row, col]  world Z in cm; col = landscape local X (+X = north), row = local Y (+Y = east)
    grid = (loc_x, loc_y, quad_cm): vertex (col, row) sits at world (loc_x + col*quad, loc_y + row*quad)

Operations:
    land_mask()        Thai provinces rasterised + any terrain above sea level (neighbouring countries)
    apply_sea()        seabed sloping down from the coast, coastal flats lifted just above Z 0
    coastline()        game-space coastline polylines (province outline where it meets the sea)
    flatten_roads()    grade-limited road beds blended into the terrain
    road_grades()      gradient statistics along roads on a grid (before/after comparison)
"""
from __future__ import annotations

import math

import numpy as np
from PIL import Image, ImageDraw
from shapely.geometry import LineString, Polygon
from shapely.ops import unary_union

LANE_WIDTH_CM = 350.0  # must match ADTGeoImporter::LaneWidthCm
MEDIAN_CM = 300.0      # must match ADTGeoImporter::MedianWidthCm


# ----------------------------------------------------------------------------- helpers
def to_grid(x, y, grid):
    loc_x, loc_y, q = grid
    return (np.asarray(x) - loc_x) / q, (np.asarray(y) - loc_y) / q


def bilinear(z: np.ndarray, col: np.ndarray, row: np.ndarray) -> np.ndarray:
    n_rows, n_cols = z.shape
    col = np.clip(col, 0, n_cols - 1.001)
    row = np.clip(row, 0, n_rows - 1.001)
    c0, r0 = np.floor(col).astype(np.int64), np.floor(row).astype(np.int64)
    fc, fr = col - c0, row - r0
    top = z[r0, c0] * (1 - fc) + z[r0, c0 + 1] * fc
    bottom = z[r0 + 1, c0] * (1 - fc) + z[r0 + 1, c0 + 1] * fc
    return top * (1 - fr) + bottom * fr


def ground_roads(roads: list[dict]) -> list[dict]:
    """Roads that follow the terrain (elevated expressways/bridges have level > 0 somewhere)."""
    return [r for r in roads if r.get("points") and max(p[2] if len(p) > 2 else 0 for p in r["points"]) == 0]


def resample(points: np.ndarray, step: float):
    seg = np.hypot(*np.diff(points, axis=0).T)
    total = float(seg.sum())
    if total < step:
        return None, None, 0.0
    dist = np.concatenate([[0.0], np.cumsum(seg)])
    n = int(total // step) + 1
    d = np.linspace(0.0, total, n + 1)
    return np.interp(d, dist, points[:, 0]), np.interp(d, dist, points[:, 1]), total / n


# ----------------------------------------------------------------------------- land / sea
def land_mask(provinces: list[dict], grid, n: int, height_m: np.ndarray, land_above_m: float = 1.0) -> np.ndarray:
    """True = land. Thai provinces (rasterised outlines) OR terrain higher than land_above_m (other countries)."""
    img = Image.new("L", (n, n), 0)
    draw = ImageDraw.Draw(img)
    loc_x, loc_y, q = grid
    for r in provinces:
        for ring in r["Rings"]:
            pts = [((p["X"] - loc_x) / q, (p["Y"] - loc_y) / q) for p in ring["Points"]]
            if len(pts) >= 3:
                draw.polygon(pts, fill=1)  # PIL (x, y) = (col, row)
    provinces_mask = np.asarray(img, dtype=bool)
    return provinces_mask | (height_m > land_above_m)


def distance_to_land(land: np.ndarray, max_steps: int) -> np.ndarray:
    """Approximate distance (in vertices) from every sea vertex to the nearest land vertex."""
    dist = np.full(land.shape, np.inf, dtype=np.float32)
    dist[land] = 0.0
    frontier = land.copy()
    reached = land.copy()
    for step in range(1, max_steps + 1):
        grown = frontier.copy()
        grown[1:, :] |= frontier[:-1, :]
        grown[:-1, :] |= frontier[1:, :]
        grown[:, 1:] |= frontier[:, :-1]
        grown[:, :-1] |= frontier[:, 1:]
        if step % 2 == 0:  # alternate 4/8-neighbourhood -> octagonal, close to Euclidean
            grown[1:, 1:] |= frontier[:-1, :-1]
            grown[:-1, :-1] |= frontier[1:, 1:]
            grown[1:, :-1] |= frontier[:-1, 1:]
            grown[:-1, 1:] |= frontier[1:, :-1]
        new = grown & ~reached
        if not new.any():
            break
        dist[new] = step
        reached |= new
        frontier = new
    return dist


def apply_sea(z: np.ndarray, land: np.ndarray, quad_cm: float, shore_depth_cm: float, slope: float,
              max_depth_cm: float, land_min_cm: float) -> dict:
    """Seabed below Z 0 sloping away from the coast; low coastal land lifted to land_min_cm (no z-fighting)."""
    steps = int(math.ceil((max_depth_cm - shore_depth_cm) / max(slope, 1e-6) / quad_cm)) + 1
    dist = distance_to_land(land, steps)
    sea = ~land
    depth = np.minimum(shore_depth_cm + dist * quad_cm * slope, max_depth_cm)
    z[sea] = -depth[sea]
    lifted = int(np.count_nonzero(land & (z < land_min_cm)))
    z[land] = np.maximum(z[land], land_min_cm)
    return {"sea_vertices_pct": round(float(sea.mean()) * 100, 1), "coastal_land_lifted_vertices": lifted,
            "seabed_cm": [round(-float(max_depth_cm)), round(-float(shore_depth_cm))]}


def coastline(provinces: list[dict], land: np.ndarray, grid, simplify_cm: float, search_cm: float = 2500.0) -> list[dict]:
    """
    Province outline segments that touch the sea, as game-space polylines.
    search_cm must exceed the outline simplification of build_provinces.py (10 m game by default),
    otherwise simplified vertices that sit slightly inland are not recognised as coast.
    """
    loc_x, loc_y, q = grid
    search_vertices = max(1, int(math.ceil(search_cm / q)))
    n = land.shape[0]
    sea = ~land
    # near_sea[r, c] = any sea vertex within search_vertices
    near = sea.copy()
    for _ in range(search_vertices):
        g = near.copy()
        g[1:, :] |= near[:-1, :]
        g[:-1, :] |= near[1:, :]
        g[:, 1:] |= near[:, :-1]
        g[:, :-1] |= near[:, 1:]
        near = g

    polys = []
    for r in provinces:
        for ring in r["Rings"]:
            if len(ring["Points"]) >= 3:
                p = Polygon([(pt["X"], pt["Y"]) for pt in ring["Points"]])
                polys.append(p if p.is_valid else p.buffer(0))
    union = unary_union(polys)
    rings = []
    for geom in getattr(union, "geoms", [union]):
        if hasattr(geom, "exterior"):
            rings.append(list(geom.exterior.coords))

    lines = []
    for coords in rings:
        pts = np.array(coords)
        col, row = to_grid(pts[:, 0], pts[:, 1], grid)
        inside = (col >= 0) & (col <= n - 1) & (row >= 0) & (row <= n - 1)
        ci = np.clip(np.round(col).astype(int), 0, n - 1)
        ri = np.clip(np.round(row).astype(int), 0, n - 1)
        coastal = inside & near[ri, ci]
        closed = bool(coastal.all())
        runs, current = [], []
        for i, flag in enumerate(coastal):
            if flag:
                current.append(i)
            elif current:
                runs.append(current)
                current = []
        if current:
            # join a run that wraps around the ring start
            if runs and runs[0][0] == 0 and not closed:
                runs[0] = current + runs[0]
            else:
                runs.append(current)
        for run in runs:
            if len(run) < 2:
                continue
            line = LineString(pts[run]).simplify(simplify_cm)
            if line.length < simplify_cm * 2:
                continue
            lines.append({"closed": closed, "length_km": round(line.length / 1e5, 3),
                          "points": [[round(x), round(y)] for x, y in line.coords]})
    return lines


# ----------------------------------------------------------------------------- roads
def road_half_width_cm(road: dict, shoulder_cm: float) -> float:
    lanes = max(1, int(road.get("lanes") or 1))
    carriageway = lanes * LANE_WIDTH_CM
    if road.get("oneway") and not str(road.get("highway", "")).endswith("_link"):
        # the importer shifts each carriageway to its left by width/2 + median/2: cover both halves of the bed
        return carriageway + MEDIAN_CM / 2 + shoulder_cm
    return carriageway / 2 + shoulder_cm


def flatten_roads(z: np.ndarray, grid, roads: list[dict], max_grade: float, smooth_cm: float,
                  shoulder_cm: float, falloff_cm: float) -> dict:
    """
    Builds a smooth, grade-limited bed for every ground-level road and blends it into the terrain.
    max_grade is a fraction in GAME units (0.15 = 15 %). Modifies z in place.
    """
    loc_x, loc_y, q = grid
    n = z.shape[0]
    z0 = z.copy()
    step = q / 2.0

    cols, rows, heights, half_widths = [], [], [], []
    raw_peak = 0.0
    for road in ground_roads(roads):
        pts = np.array([p[:2] for p in road["points"]], dtype=np.float64)
        x, y, ds = resample(pts, step)
        if x is None:
            continue
        col, row = to_grid(x, y, grid)
        h = bilinear(z0, col, row)
        raw_peak = max(raw_peak, float(np.max(np.abs(np.diff(h)) / ds)) if len(h) > 1 else 0.0)

        window = max(1, int(round(smooth_cm / ds)))
        if window > 1 and len(h) > window:
            pad = window // 2
            h = np.convolve(np.pad(h, pad, mode="edge"), np.ones(window) / window, mode="same")[pad:pad + len(h)]

        # grade limit: average of forward and backward limited profiles (both satisfy |dh| <= g*ds)
        limit = max_grade * ds
        fwd, bwd = h.copy(), h.copy()
        for i in range(1, len(h)):
            fwd[i] = min(max(fwd[i], fwd[i - 1] - limit), fwd[i - 1] + limit)
        for i in range(len(h) - 2, -1, -1):
            bwd[i] = min(max(bwd[i], bwd[i + 1] - limit), bwd[i + 1] + limit)
        h = (fwd + bwd) / 2.0

        cols.append(col)
        rows.append(row)
        heights.append(h)
        half_widths.append(np.full(len(h), road_half_width_cm(road, shoulder_cm)))

    if not cols:
        return {"roads": 0}

    col = np.concatenate(cols)
    row = np.concatenate(rows)
    hgt = np.concatenate(heights)
    hw = np.concatenate(half_widths)

    reach = int(math.ceil((hw.max() + falloff_cm) / q)) + 1
    offsets = [(dc, dr) for dr in range(-reach, reach + 1) for dc in range(-reach, reach + 1)]
    best_d = np.full(n * n, np.inf)
    best_h = np.zeros(n * n)
    best_hw = np.zeros(n * n)
    chunk = max(200, 2_000_000 // len(offsets))
    c_floor, r_floor = np.floor(col).astype(np.int64), np.floor(row).astype(np.int64)

    for start in range(0, len(col), chunk):
        sl = slice(start, start + chunk)
        idx_parts, d_parts, s_parts = [], [], []
        sample_ids = np.arange(start, min(start + chunk, len(col)))
        for dc, dr in offsets:
            ci = c_floor[sl] + dc
            ri = r_floor[sl] + dr
            ok = (ci >= 0) & (ci < n) & (ri >= 0) & (ri < n)
            d = np.hypot(ci - col[sl], ri - row[sl]) * q
            ok &= d <= hw[sl] + falloff_cm
            idx_parts.append((ri * n + ci)[ok])
            d_parts.append(d[ok])
            s_parts.append(sample_ids[ok])
        idx = np.concatenate(idx_parts)
        if idx.size == 0:
            continue
        dist = np.concatenate(d_parts)
        sid = np.concatenate(s_parts)
        order = np.lexsort((dist, idx))  # by vertex, then nearest sample first
        idx, dist, sid = idx[order], dist[order], sid[order]
        first = np.ones(idx.size, dtype=bool)
        first[1:] = idx[1:] != idx[:-1]
        idx, dist, sid = idx[first], dist[first], sid[first]
        better = dist < best_d[idx]
        idx, dist, sid = idx[better], dist[better], sid[better]
        best_d[idx] = dist
        best_h[idx] = hgt[sid]
        best_hw[idx] = hw[sid]

    touched = np.isfinite(best_d)
    t = np.zeros(n * n)
    t[touched] = (best_d[touched] - best_hw[touched]) / falloff_cm
    weight = np.where(t <= 0, 1.0, np.where(t < 1, 1.0 - (3 * t ** 2 - 2 * t ** 3), 0.0))
    weight[~touched] = 0.0
    flat = z.reshape(-1)
    flat[:] = flat * (1 - weight) + best_h * weight

    bed = (weight >= 1.0).reshape(n, n)
    change = (z - z0)[bed]
    return {
        "roads": len(cols),
        "bed_vertices": int(bed.sum()),
        "blended_vertices": int(touched.sum()),
        "max_fill_cm": round(float(change.max()), 1) if change.size else 0.0,
        "max_cut_cm": round(float(-change.min()), 1) if change.size else 0.0,
        "p99_abs_change_cm": round(float(np.percentile(np.abs(change), 99)), 1) if change.size else 0.0,
    }


def road_grades(z: np.ndarray, grid, roads: list[dict], step_cm: float, thresholds=(0.10, 0.15, 0.20)) -> dict:
    """Length-weighted gradient percentiles (game units) along ground-level roads, sampled every step_cm."""
    grades, lengths = [], []
    for road in ground_roads(roads):
        pts = np.array([p[:2] for p in road["points"]], dtype=np.float64)
        x, y, ds = resample(pts, step_cm)
        if x is None:
            continue
        col, row = to_grid(x, y, grid)
        h = bilinear(z, col, row)
        grades.append(np.abs(np.diff(h)) / ds)
        lengths.append(np.full(len(h) - 1, ds))
    if not grades:
        return {}
    g, w = np.concatenate(grades), np.concatenate(lengths)
    order = np.argsort(g)
    cum = np.cumsum(w[order]) / w.sum()

    def pct(p: float) -> float:
        return round(float(g[order][min(np.searchsorted(cum, p), len(g) - 1)]) * 100, 1)

    out = {"km_game": round(float(w.sum()) / 1e5, 1), "p50": pct(0.5), "p90": pct(0.9), "p99": pct(0.99),
           "max": round(float(g.max()) * 100, 1)}
    for t in thresholds:
        out[f"share_over_{int(t * 100)}pct"] = round(float(w[g > t].sum() / w.sum()) * 100, 2)
    return out
