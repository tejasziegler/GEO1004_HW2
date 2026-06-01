
"""
Synthetic CityJSON 2.0 generator for testing 3D geomatics algorithms.

Each generated building has analytically exact ground truth stored as attributes:
  ground_area  – footprint area in m²
  roof_area    – total roof surface area in m² (slant area for sloped roofs)
  volume       – enclosed volume in m³

These serve as reference values when testing your own implementation of:
  - LOD simplification
  - Surface triangulation
  - Volume computation via the divergence theorem
  - Roof area computation from triangulated meshes

Supported building shapes
--------------------------
  flat_box   : rectangular footprint, flat roof          (LOD 1.2)
  gabled     : rectangular footprint, two-slope roof     (LOD 2.2)
  hip        : rectangular footprint, four-slope roof    (LOD 2.2)
  l_shape    : L-shaped footprint, flat roof             (LOD 1.2)

Geometry contract
-----------------
  • All faces are planar polygons — NO triangulation is applied.
  • Winding order follows the right-hand rule (outward normals via cross product
    of consecutive edge vectors).
  • Vertex coordinates are stored as integers with scale [0.001, 0.001, 0.001],
    giving 1 mm precision.  Decode with: x_m = x_int * 0.001

Usage (recreate report results)
-----
  python generate_synthetic_city.py -n 20 --seed 7
  python generate_synthetic_city.py -n 20 --seed 13
  python generate_synthetic_city.py -n 20 --seed 42
  python generate_synthetic_city.py -n 20 --seed 69
"""

import json
import math
import random
import argparse
from pathlib import Path
from typing import List, Tuple

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SCALE     = [0.001, 0.001, 0.001]   # 1 integer unit == 1 mm
TRANSLATE = [0.0, 0.0, 0.0]

BUILDING_TYPES = ["flat_box", "gabled", "hip", "l_shape"]


# ---------------------------------------------------------------------------
# Vertex pool
# ---------------------------------------------------------------------------

class VertexPool:
    """
    Manages the flat CityJSON vertex list with deduplication.

    CityJSON stores all vertices globally as integer triples.  Faces then
    reference vertices by index.  Deduplication avoids redundant entries when
    different faces share a corner.

    Decode to metres:  x_m = x_int * scale[0] + translate[0]
    """

    def __init__(self, scale: list, translate: list):
        self.scale = scale
        self.translate = translate
        self._verts: List[List[int]] = []
        self._lookup: dict = {}

    def add(self, x: float, y: float, z: float) -> int:
        ix = round((x - self.translate[0]) / self.scale[0])
        iy = round((y - self.translate[1]) / self.scale[1])
        iz = round((z - self.translate[2]) / self.scale[2])
        key = (ix, iy, iz)
        if key not in self._lookup:
            self._lookup[key] = len(self._verts)
            self._verts.append([ix, iy, iz])
        return self._lookup[key]

    def add_many(self, pts: List[Tuple[float, float, float]]) -> List[int]:
        return [self.add(*p) for p in pts]

    @property
    def vertices(self) -> List[List[int]]:
        return self._verts


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def _solid_geometry(faces: List[List[int]],
                    surf_types: List[dict],
                    face_surf_idx: List[int],
                    lod: str) -> dict:
    """
    Assemble a CityJSON Solid geometry object.

    faces          : list of vertex-index rings (outer ring only, no holes)
    surf_types     : list of surface descriptors, e.g. [{"type":"RoofSurface"}]
    face_surf_idx  : for each face, which index in surf_types it belongs to
    lod            : LOD string ("1.2", "2.2", …)

    CityJSON Solid boundary structure:
      boundaries[shell][face][ring][vertex_index]
      One shell, no inner rings → boundaries = [[ [face_ring], … ]]
    """
    return {
        "type": "Solid",
        "lod": lod,
        "boundaries": [[[f] for f in faces]],
        "semantics": {
            "surfaces": surf_types,
            "values":   [face_surf_idx],
        },
    }


def _ground_ring_from_solid(geom: dict) -> List[int]:
    """
    Return the footprint ring (CCW from above) from a Solid geometry.

    The GroundSurface face is stored CCW when viewed from *below* (outward
    normal points down).  Reversing it gives the conventional CCW-from-above
    footprint used in the LOD 0 MultiSurface.
    """
    surfaces   = geom["semantics"]["surfaces"]
    gnd_surf_i = next(i for i, s in enumerate(surfaces)
                      if s["type"] == "GroundSurface")
    face_sems  = geom["semantics"]["values"][0]
    face_i     = face_sems.index(gnd_surf_i)
    ring       = geom["boundaries"][0][face_i][0]   # outer ring
    return list(reversed(ring))


# ---------------------------------------------------------------------------
# Winding-order derivation
# ---------------------------------------------------------------------------
#
# All buildings use these two rules so normals are consistently outward:
#
#  1. Ground face  – store as reversed CCW-from-above → normal = -z
#  2. Wall faces   – for a CCW footprint (viewed from above), each directed
#                   edge Pi→Pj produces a wall [Pi, Pj, Rj, Ri] where R is
#                   the vertex elevated by `height`.  This gives outward
#                   normals for any simple (convex or concave) polygon.
#  3. Roof face    – same order as the CCW-from-above footprint → normal = +z
#
# Sloped roof faces are verified below with explicit cross-product checks.


# ---------------------------------------------------------------------------
# Building generators
# ---------------------------------------------------------------------------

def make_flat_box(vp: VertexPool,
                  ox: float, oy: float,
                  width: float, length: float,
                  height: float) -> dict:
    """
    Box building: rectangular footprint, flat roof.

    Faces (6):  Ground | Wall×4 | Roof
    LOD:        1.2

    Ground truth
    ────────────
      ground_area = width × length
      roof_area   = width × length
      volume      = width × length × height
    """
    w, l, h = width, length, height

    g = vp.add_many([(ox,   oy,   0), (ox+w, oy,   0),
                     (ox+w, oy+l, 0), (ox,   oy+l, 0)])
    r = vp.add_many([(ox,   oy,   h), (ox+w, oy,   h),
                     (ox+w, oy+l, h), (ox,   oy+l, h)])
    g0,g1,g2,g3 = g
    r0,r1,r2,r3 = r

    faces = [
        [g0, g3, g2, g1],   # Ground (normal −z)
        [g0, g1, r1, r0],   # South  (normal −y)
        [g1, g2, r2, r1],   # East   (normal +x)
        [g2, g3, r3, r2],   # North  (normal +y)
        [g3, g0, r0, r3],   # West   (normal −x)
        [r0, r1, r2, r3],   # Roof   (normal +z)
    ]
    sem = [{"type":"GroundSurface"},{"type":"WallSurface"},{"type":"RoofSurface"}]
    idx = [0, 1,1,1,1, 2]

    return {
        "geometry":    _solid_geometry(faces, sem, idx, "2.2"),
        "ground_area": round(w * l, 6),
        "roof_area":   round(w * l, 6),
        "volume":      round(w * l * h, 6),
        "building_type": "flat_box",
    }


def make_gabled(vp: VertexPool,
                ox: float, oy: float,
                width: float, length: float,
                wall_height: float, ridge_height: float) -> dict:
    """
    Gabled-roof building: rectangular footprint, two equal roof slopes.
    Ridge runs along the y-axis, centred at x = ox + width/2.

    Faces (7):  Ground | South gable (pentagon) | North gable (pentagon)
                      | East wall | West wall | Roof-west | Roof-east
    LOD:        2.2

    Ground truth
    ────────────
      slope_width = √((width/2)² + (ridge_height − wall_height)²)
      roof_area   = 2 × length × slope_width
      volume      = width × length × wall_height
                  + ½ × width × (ridge_height − wall_height) × length
    """
    w, l  = width, length
    hw, hr = wall_height, ridge_height
    cx    = ox + w / 2.0

    A = (ox,   oy,   0);  B = (ox+w, oy,   0)
    C = (ox+w, oy+l, 0);  D = (ox,   oy+l, 0)
    E = (ox,   oy,   hw); F = (ox+w, oy,   hw)
    G = (ox+w, oy+l, hw); H = (ox,   oy+l, hw)
    R0 = (cx,  oy,   hr); R1 = (cx,  oy+l, hr)

    iA,iB,iC,iD = vp.add_many([A,B,C,D])
    iE,iF,iG,iH = vp.add_many([E,F,G,H])
    iR0,iR1     = vp.add_many([R0,R1])

    # Cross-product checks (see module docstring):
    #   South gable [A,B,F,R0,E]: edge B−A=(w,0,0), edge F−B=(0,0,hw)
    #     → (0,−w·hw,0) = −y ✓
    #   North gable [C,D,H,R1,G]: edge D−C=(−w,0,0), edge H−D=(0,0,hw)
    #     → (0,+w·hw,0) = +y ✓
    #   Roof-west [E,R0,R1,H]: edge R0−E=(w/2,0,hr−hw), edge R1−R0=(0,l,0)
    #     → (0·0−(hr−hw)·l, (hr−hw)·0−(w/2)·0, (w/2)·l−0·0) = (−l·dh,0,l·w/2) = (−x,+z) ✓
    #   Roof-east [F,G,R1,R0]: edge G−F=(0,l,0), edge R1−G=(−w/2,−w/2,hr−hw)
    #     → (l·(hr−hw),0,l·w/2) = (+x,+z) ✓

    faces = [
        [iA, iD, iC, iB],           # Ground      (normal −z)
        [iA, iB, iF, iR0, iE],      # South gable (normal −y)
        [iC, iD, iH, iR1, iG],      # North gable (normal +y)
        [iB, iC, iG, iF],           # East wall   (normal +x)
        [iD, iA, iE, iH],           # West wall   (normal −x)
        [iE, iR0, iR1, iH],         # Roof west   (normal −x,+z)
        [iF, iG, iR1, iR0],         # Roof east   (normal +x,+z)
    ]
    sem = [{"type":"GroundSurface"},{"type":"WallSurface"},{"type":"RoofSurface"}]
    idx = [0, 1,1,1,1, 2,2]

    dh = hr - hw
    slope_w = math.sqrt((w / 2) ** 2 + dh ** 2)
    return {
        "geometry":    _solid_geometry(faces, sem, idx, "2.2"),
        "ground_area": round(w * l, 6),
        "roof_area":   round(2 * l * slope_w, 6),
        "volume":      round(w * l * hw + 0.5 * w * dh * l, 6),
        "building_type": "gabled",
    }


def make_hip(vp: VertexPool,
             ox: float, oy: float,
             width: float, length: float,
             wall_height: float, ridge_height: float) -> dict:
    """
    Hip-roof building: rectangular footprint, four-slope roof.
    Ridge parallel to y-axis, centred, length = length − width  (requires
    length ≥ width).

    Faces (9):  Ground | Wall×4 | South triangle | North triangle
                      | East trapezoid | West trapezoid
    LOD:        2.2

    Ground truth
    ────────────
    Let dh = ridge_height − wall_height.
    The hip slope distance (from eave corner to ridge end) is the same for
    all four faces:
        s = √((width/2)² + dh²)

    roof_area  = 2 × length × s
                 (two triangles + two trapezoids simplify to this)

    volume     = width × length × wall_height
               + width × (ridge_height − wall_height) × (length/2 − width/6)

    The volume formula is derived by integrating the cross-section area at
    height z ∈ [wall_height, ridge_height]:
        A(t) = W(1−t) × (L − tW),   t = (z−hw)/(hr−hw)
    ∫₀¹ A(t) dt = L/2 − W/6    →    multiply by W·(hr−hw)
    """
    w, l   = width, length
    hw, hr = wall_height, ridge_height
    if l < w:
        raise ValueError(
            f"Hip roof requires length ≥ width, got {l} < {w}. "
            "Swap width and length, or use a gabled roof."
        )

    cx  = ox + w / 2.0
    rs_y = oy + w / 2.0        # ridge start y
    re_y = oy + l - w / 2.0    # ridge end   y

    A = (ox,   oy,   0);  B = (ox+w, oy,   0)
    C = (ox+w, oy+l, 0);  D = (ox,   oy+l, 0)
    E = (ox,   oy,   hw); F = (ox+w, oy,   hw)
    G = (ox+w, oy+l, hw); H = (ox,   oy+l, hw)
    RS = (cx, rs_y, hr)        # ridge south end
    RE = (cx, re_y, hr)        # ridge north end

    iA,iB,iC,iD  = vp.add_many([A,B,C,D])
    iE,iF,iG,iH  = vp.add_many([E,F,G,H])
    iRS,iRE      = vp.add_many([RS,RE])

    # Winding checks for sloped faces:
    #   South triangle [E,F,RS]: edge F−E=(w,0,0), edge RS−F=(−w/2,w/2,dh)
    #     → (0·dh−0·w/2, 0·(−w/2)−w·dh, w·w/2−0·(−w/2)) = (0,−w·dh,w²/2) → −y,+z ✓
    #   North triangle [H,RE,G]: edge RE−H=(w/2,−w/2,dh), edge G−H=(w,0,0)
    #     → (−w/2·0−dh·0, dh·w−w/2·0, w/2·0−(−w/2)·w) = (0,w·dh,w²/2) → +y,+z ✓
    #   East trapezoid [F,G,RE,RS]: edge G−F=(0,l,0), edge RE−G=(−w/2,−w/2,dh)
    #     → (l·dh,0,l·w/2) → +x,+z ✓
    #   West trapezoid [E,RS,RE,H]: edge RS−E=(w/2,w/2,dh), edge RE−RS=(0,l−w,0)
    #     → (w/2·0−dh·(l−w), dh·0−w/2·0, w/2·(l−w)−w/2·0) = (−dh(l−w),0,w(l−w)/2)
    #     → −x,+z ✓  (valid when l > w)

    faces = [
        [iA, iD, iC, iB],        # Ground          (normal −z)
        [iA, iB, iF, iE],        # South wall      (normal −y)
        [iB, iC, iG, iF],        # East wall       (normal +x)
        [iC, iD, iH, iG],        # North wall      (normal +y)
        [iD, iA, iE, iH],        # West wall       (normal −x)
        [iE, iF, iRS],           # South roof △    (normal −y,+z)
        [iH, iRE, iG],           # North roof △    (normal +y,+z)
        [iF, iG, iRE, iRS],      # East roof trap. (normal +x,+z)
        [iE, iRS, iRE, iH],      # West roof trap. (normal −x,+z)
    ]
    sem = [{"type":"GroundSurface"},{"type":"WallSurface"},{"type":"RoofSurface"}]
    idx = [0, 1,1,1,1, 2,2,2,2]

    dh    = hr - hw
    s     = math.sqrt((w / 2) ** 2 + dh ** 2)
    vol   = w * l * hw + w * dh * (l / 2.0 - w / 6.0)
    return {
        "geometry":    _solid_geometry(faces, sem, idx, "2.2"), #changed from 1.2
        "ground_area": round(w * l, 6),
        "roof_area":   round(2 * l * s, 6),
        "volume":      round(vol, 6),
        "building_type": "hip",
    }


def make_l_shape(vp: VertexPool,
                 ox: float, oy: float,
                 full_width: float, full_length: float,
                 notch_x: float, notch_y: float,
                 height: float) -> dict:
    """
    L-shaped building: rectangular bounding box with an NE notch, flat roof.

    Full bounding box : (ox, oy) → (ox+W, oy+L)
    Notch             : (ox+notch_x, oy+notch_y) → (ox+W, oy+L)

    Six footprint corners (CCW from above):
      P0 = (ox,         oy         )  SW
      P1 = (ox+W,       oy         )  SE
      P2 = (ox+W,       oy+notch_y )  notch SE corner
      P3 = (ox+notch_x, oy+notch_y )  inner corner
      P4 = (ox+notch_x, oy+L       )  notch N
      P5 = (ox,         oy+L       )  NW

    For each directed edge Pi→Pj of the CCW footprint the wall face is:
      [Pi, Pj, Rj, Ri]   (R = vertex elevated by height)
    This formula gives outward normals for any simple polygon — concave or convex.

    Faces (8):  Ground | Wall×6 | Roof
    LOD:        1.2

    Ground truth
    ────────────
      ground_area = W×L − (W−notch_x)×(L−notch_y)
      roof_area   = ground_area
      volume      = ground_area × height
    """
    W, L  = full_width, full_length
    nx, ny = notch_x, notch_y
    h     = height

    fp2d = [
        (ox,    oy   ),
        (ox+W,  oy   ),
        (ox+W,  oy+ny),
        (ox+nx, oy+ny),
        (ox+nx, oy+L ),
        (ox,    oy+L ),
    ]
    n  = len(fp2d)
    gi = vp.add_many([(x, y, 0) for x, y in fp2d])
    ri = vp.add_many([(x, y, h) for x, y in fp2d])

    ground_face = [gi[n-1-i] for i in range(n)]          # reversed → normal −z
    wall_faces  = [[gi[i], gi[(i+1)%n], ri[(i+1)%n], ri[i]] for i in range(n)]
    roof_face   = list(ri)                                 # same order as fp2d → normal +z

    faces = [ground_face] + wall_faces + [roof_face]
    sem   = [{"type":"GroundSurface"},{"type":"WallSurface"},{"type":"RoofSurface"}]
    idx   = [0] + [1]*n + [2]

    area = W * L - (W - nx) * (L - ny)
    return {
        "geometry":    _solid_geometry(faces, sem, idx, "2.2"), #changed from 1.2
        "ground_area": round(area, 6),
        "roof_area":   round(area, 6),
        "volume":      round(area * h, 6),
        "building_type": "l_shape",
    }


# ---------------------------------------------------------------------------
# CityJSON assembly
# ---------------------------------------------------------------------------

def assemble_cityjson(building_specs: List[dict], seed: int = 42) -> dict:
    """
    Build a complete CityJSON 2.0 document from a list of building specs.

    Each spec must contain:
      building_type : one of BUILDING_TYPES
      ox, oy        : origin in metres
      … plus the shape-specific parameters listed for each make_* function.

    The vertex pool is shared across all buildings (global deduplication).
    Each building produces one Building (with LOD-0 footprint) and one
    BuildingPart (with the full Solid geometry).
    """
    vp = VertexPool(SCALE, TRANSLATE)
    city_objects: dict = {}

    for spec in building_specs:
        bid   = spec["id"]
        btype = spec["building_type"]
        ox    = spec["ox"]
        oy    = spec["oy"]

        if btype == "flat_box":
            result = make_flat_box(vp, ox, oy,
                                   spec["width"], spec["length"], spec["height"])
        elif btype == "gabled":
            result = make_gabled(vp, ox, oy,
                                  spec["width"], spec["length"],
                                  spec["wall_height"], spec["ridge_height"])
        elif btype == "hip":
            result = make_hip(vp, ox, oy,
                               spec["width"], spec["length"],
                               spec["wall_height"], spec["ridge_height"])
        elif btype == "l_shape":
            result = make_l_shape(vp, ox, oy,
                                   spec["full_width"], spec["full_length"],
                                   spec["notch_x"], spec["notch_y"],
                                   spec["height"])
        else:
            raise ValueError(f"Unknown building_type: {btype!r}")

        geom     = result["geometry"]
        part_id  = f"{bid}-0"
        fp_ring  = _ground_ring_from_solid(geom)

        # LOD 0: footprint as MultiSurface (used by the Building parent object)
        footprint_geom = {
            "type":       "MultiSurface",
            "lod":        "0",
            "boundaries": [[fp_ring]],
        }

        # Building (parent): carries the ground-truth attributes
        city_objects[bid] = {
            "type":     "Building",
            "children": [part_id],
            "geometry": [footprint_geom],
            "attributes": {
                "id":            bid,
                "building_type": result["building_type"],
                "ground_area":   result["ground_area"],
                "roof_area":     result["roof_area"],
                "volume":        result["volume"],
                "lod":           geom["lod"],
                "note": (
                    "ground_area/roof_area/volume are analytically exact; "
                    "use them as ground truth for algorithm validation."
                ),
            },
        }

        # BuildingPart (child): carries the 3D Solid geometry
        # BuildingPart (child): carries the 3D Solid geometry
        city_objects[part_id] = {
            "type": "BuildingPart",
            "parents": [bid],
            "geometry": [geom],
            "attributes": {
                "roof_area": result["roof_area"],
                "volume": result["volume"],
                "ground_area": result["ground_area"],
            },
        }

    return {
        "type":      "CityJSON",
        "version":   "2.0",
        "transform": {"scale": SCALE, "translate": TRANSLATE},
        "metadata":  {
            "referenceSystem": "https://www.opengis.net/def/crs/OGC/1.3/CRS84",
        },
        "CityObjects": city_objects,
        "vertices":    vp.vertices,
    }


# ---------------------------------------------------------------------------
# Random building specification generator
# ---------------------------------------------------------------------------

def random_building_spec(bid: str, btype: str,
                          ox: float, oy: float,
                          rng: random.Random) -> dict:
    """
    Return a building spec dict with plausible random dimensions.

    Dimension ranges are chosen to produce buildings that are easy to
    reason about (whole-metre sizes, realistic proportions).
    """
    if btype == "flat_box":
        w = rng.randint(5, 20)
        l = rng.randint(5, 20)
        h = rng.randint(3, 12)
        return dict(id=bid, building_type=btype, ox=ox, oy=oy,
                    width=float(w), length=float(l), height=float(h))

    elif btype == "gabled":
        w  = rng.randint(6, 16)
        l  = rng.randint(8, 24)
        hw = rng.randint(3, 6)
        hr = hw + rng.randint(2, 5)
        return dict(id=bid, building_type=btype, ox=ox, oy=oy,
                    width=float(w), length=float(l),
                    wall_height=float(hw), ridge_height=float(hr))

    elif btype == "hip":
        w  = rng.randint(6, 14)
        l  = w + rng.randint(2, 14)   # ensure l ≥ w
        hw = rng.randint(3, 6)
        hr = hw + rng.randint(2, 5)
        return dict(id=bid, building_type=btype, ox=ox, oy=oy,
                    width=float(w), length=float(l),
                    wall_height=float(hw), ridge_height=float(hr))

    elif btype == "l_shape":
        W  = rng.randint(10, 22)
        L  = rng.randint(10, 22)
        nx = rng.randint(4, W - 3)
        ny = rng.randint(4, L - 3)
        h  = rng.randint(3, 10)
        return dict(id=bid, building_type=btype, ox=ox, oy=oy,
                    full_width=float(W), full_length=float(L),
                    notch_x=float(nx), notch_y=float(ny),
                    height=float(h))

    raise ValueError(f"Unknown type: {btype!r}")


def generate_grid(n_buildings: int,
                  types: List[str],
                  seed: int = 42,
                  spacing: float = 5.0) -> List[dict]:
    """
    Place `n_buildings` buildings on a square grid with `spacing` m between them.

    Buildings are assigned types in round-robin order so every requested type
    appears (as long as n_buildings ≥ len(types)).
    """
    rng   = random.Random(seed)
    specs = []
    cols  = math.ceil(math.sqrt(n_buildings))

    for i in range(n_buildings):
        btype = types[i % len(types)]
        # Estimate footprint size to set grid cell spacing
        max_dim = 25.0
        cell    = max_dim + spacing
        row, col = divmod(i, cols)
        ox = col * cell
        oy = row * cell
        bid  = f"building_{i:03d}"
        spec = random_building_spec(bid, btype, ox, oy, rng)
        specs.append(spec)

    return specs


# ---------------------------------------------------------------------------
# Predefined example buildings (for unit-test / documentation purposes)
# ---------------------------------------------------------------------------

EXAMPLE_SPECS: List[dict] = [
    # A 10×10 cube — trivial ground truth
    dict(id="cube_10",         building_type="flat_box",
         ox=0,   oy=0,   width=10.0, length=10.0, height=10.0),
    # 8×12 flat box
    dict(id="box_8x12",        building_type="flat_box",
         ox=15,  oy=0,   width=8.0,  length=12.0, height=5.0),
    # Gabled roof: 10×15, wall=4m, ridge=7m
    dict(id="gabled_10x15",    building_type="gabled",
         ox=0,   oy=15,  width=10.0, length=15.0,
         wall_height=4.0, ridge_height=7.0),
    # Hip roof: 8×14, wall=3m, ridge=6m
    dict(id="hip_8x14",        building_type="hip",
         ox=15,  oy=15,  width=8.0,  length=14.0,
         wall_height=3.0, ridge_height=6.0),
    # L-shape: 14×14 bounding box, 6×6 notch
    dict(id="l_shape_14x14",   building_type="l_shape",
         ox=30,  oy=0,   full_width=14.0, full_length=14.0,
         notch_x=8.0, notch_y=8.0, height=4.0),
]


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _print_summary(data: dict) -> None:
    """Print a human-readable summary of the generated buildings."""
    objects = data["CityObjects"]
    buildings = {k: v for k, v in objects.items() if v["type"] == "Building"}
    print(f"\n  {'ID':<22}  {'type':<12}  {'ground_area':>12}  {'roof_area':>12}  {'volume':>14}  {'lod'}")
    print("  " + "─" * 85)
    for bid, obj in sorted(buildings.items()):
        a = obj["attributes"]
        print(f"  {bid:<22}  {a['building_type']:<12}  "
              f"{a['ground_area']:>11.3f}m²  {a['roof_area']:>11.3f}m²  "
              f"{a['volume']:>13.3f}m³  {a['lod']}")
    print(f"\n  Total buildings : {len(buildings)}")
    print(f"  Total vertices  : {len(data['vertices'])}\n")


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-n", "--buildings", type=int, default=8,
        help="Number of buildings to generate (default: 8)",
    )
    parser.add_argument(
        "-t", "--types", nargs="+", choices=BUILDING_TYPES,
        default=BUILDING_TYPES,
        metavar="TYPE",
        help=(
            f"Building types to include: {{{', '.join(BUILDING_TYPES)}}}. "
            "Types are cycled if -n > number of types. (default: all types)"
        ),
    )
    parser.add_argument(
        "--preset", choices=["examples"],
        help=(
            "Use a predefined building set instead of random generation. "
            "'examples' creates 5 buildings with clean, hand-crafted dimensions."
        ),
    )
    parser.add_argument(
        "--seed", type=int, default=42,
        help="Random seed for reproducibility (default: 42)",
    )
    parser.add_argument(
        "--spacing", type=float, default=5.0,
        help="Gap in metres between buildings on the grid (default: 5.0)",
    )
    parser.add_argument(
        "-o", "--output", type=str, default="synthetic_city.city.json",
        help="Output file path (default: synthetic_city.city.json)",
    )
    parser.add_argument(
        "--indent", type=int, default=None,
        help="JSON indentation level. Omit for compact output.",
    )
    args = parser.parse_args()

    if args.preset == "examples":
        specs = EXAMPLE_SPECS
        print("Using predefined example buildings.")
    else:
        specs = generate_grid(
            n_buildings=args.buildings,
            types=args.types,
            seed=args.seed,
            spacing=args.spacing,
        )

    data    = assemble_cityjson(specs)
    outpath = Path(f"synthetic_city_{args.seed}.city.json")
    outpath.write_text(json.dumps(data, indent=args.indent))

    _print_summary(data)
    print(f"  Written to: {outpath.resolve()}\n")


if __name__ == "__main__":
    main()
