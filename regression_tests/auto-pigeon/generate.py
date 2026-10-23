#!/usr/bin/env python3
"""Write the authored regression maps of this directory and `cases.json`, the assertions about them.

    python3 regression_tests/auto-pigeon/generate.py           # (re)write every authored file
    python3 regression_tests/auto-pigeon/generate.py --check   # exit 1 if a committed file differs

Every map here is authored by this script, from the coordinates below, for this repository: no
game data, no private map. The three pipe maps of R04-R06 are the exception: they are output of
Auto-Pigeon's own pipe-shape test generator, committed byte for byte (`PIPE_MAPS` names their
SHA-256), and only their assertions are written here.

An assertion states what the SOURCE describes — this point is inside a brush, this point is air,
this face is visible here with this texture coordinate — and never what an earlier compiler wrote.
`ci/auto-pigeon/regress.py` compiles each map and asks the BSP. Facts used throughout:

  * Q3Map2 splits the world on the axial planes x = k*1024, y = k*1024 (`-blocksize`, default 1024)
    before it looks at any brush face. So x = 0 and y = 0 are node planes of every map below whose
    bounds straddle them, which is how a map can put a brush across a known split.
  * ON_EPSILON (0.1) is the distance at which a face counts as crossing a plane when the visible-face
    tree is built (`facebsp.cpp`), CLIP_EPSILON*2 (0.2) is the distance upstream then clips it at.
  * SplitBrush (`brush.cpp`) discarded a split fragment whose volume is below 1 cubic unit, kept a brush
    that reaches less than 0.1 past a plane wholly on its other side, and sent a brush whose section by
    the plane has fewer than three edges longer than 0.2 wholly to the side it reaches furthest into.
  * Brush vertices are rounded to 1/8 unit when the brush is loaded (brush snap); the windings every later
    stage reads are the rounded ones.
  * A portal with fewer than three edges longer than 0.2 (EDGE_LENGTH) is dropped (`portals.cpp`).
  * Drawn vertices are rounded to 1/8 unit (`surface.cpp`, brush snap), so a drawn probe sits more
    than 1/8 from every edge and is matched within 0.2 of its plane.
"""

import hashlib
import json
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
SCHEMA = "auto-pigeon-q3map2-regressions/1"


def num(v):
    s = ("%.6f" % v).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def pt(p):
    return "( %s )" % " ".join(num(v) for v in p)


# ---------------------------------------------------------------- brush syntax

class Face:
    """One brush side: three points (clockwise seen from outside, as Radiant writes them) and a texture."""

    def __init__(self, p1, p2, p3, tex):
        self.p, self.tex = (p1, p2, p3), tex


def box_faces(x0, y0, z0, x1, y1, z1, tex, overrides=None):
    """An axial box as six faces: +x, -x, +y, -y, +z, -z (the order Radiant writes a new brush in)."""
    overrides = overrides or {}
    sides = {
        "+x": ((x1, y0, z0), (x1, y0, z1), (x1, y1, z1)),
        "-x": ((x0, y1, z0), (x0, y1, z1), (x0, y0, z1)),
        "+y": ((x1, y1, z0), (x1, y1, z1), (x0, y1, z1)),
        "-y": ((x0, y0, z0), (x0, y0, z1), (x1, y0, z1)),
        "+z": ((x1, y1, z1), (x1, y0, z1), (x0, y0, z1)),
        "-z": ((x1, y0, z0), (x1, y1, z0), (x0, y1, z0)),
    }
    return [Face(*points, overrides.get(name, tex)) for name, points in sides.items()]


def classic(faces, comment=None):
    """Quake III classic projection: `tex shiftS shiftT rotation scaleS scaleT contents 0 0` (a texture tuple's optional
    fourth element is the content flags, e.g. CONTENTS_DETAIL)."""
    lines = (["// " + comment] if comment else []) + ["{"]
    for f in faces:
        tex, shift, scale, contents = (f.tex + (0,))[:4] if isinstance(f.tex, tuple) else (f.tex, (0, 0), (0.5, 0.5), 0)
        lines.append("%s %s %s %s 0 %s %s %d 0 0" % (" ".join(pt(p) for p in f.p), tex, num(shift[0]), num(shift[1]),
                                                     num(scale[0]), num(scale[1]), contents))
    return "\n".join(lines + ["}"]) + "\n"


def face_normal(f):
    a, b, c = f.p
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    length = math.sqrt(sum(x * x for x in n))
    return [x / length for x in n]


# Q3Map2's nearest-axial projection table (map.cpp baseaxis): normal axis, S axis, T axis
BASEAXIS = [((0, 0, 1), (1, 0, 0), (0, -1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0)),
            ((1, 0, 0), (0, 1, 0), (0, 0, -1)), ((-1, 0, 0), (0, 1, 0), (0, 0, -1)),
            ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((0, -1, 0), (1, 0, 0), (0, 0, -1))]


def classic_st(normal, point, shift, scale, size):
    """st of `point` on a face of `normal` with an unrotated classic texture of `size` (map.cpp QuakeTextureVecs)."""
    best, axes = 0, BASEAXIS[0]
    for entry in BASEAXIS:
        d = sum(a * b for a, b in zip(normal, entry[0]))
        if d > best + 0.0001:
            best, axes = d, entry
    shift = [shift[k] - math.floor(shift[k] / size[k]) * size[k] for k in range(2)]
    return [(shift[k] + sum(a * b for a, b in zip(axes[k + 1], point)) / scale[k]) / size[k] for k in range(2)]


def valve220(faces, comment=None):
    """Valve 220: `tex [ ux uy uz shiftS ] [ vx vy vz shiftT ] rotation scaleS scaleT`, axes given explicitly."""
    lines = (["// " + comment] if comment else []) + ["{"]
    for f in faces:
        tex, shift, scale = f.tex if isinstance(f.tex, tuple) else (f.tex, (0, 0), (0.5, 0.5))
        u, v = valve_axes(face_normal(f))
        lines.append("%s %s [ %s %s ] [ %s %s ] 0 %s %s" % (" ".join(pt(p) for p in f.p), tex, " ".join(num(x) for x in u),
                                                           num(shift[0]), " ".join(num(x) for x in v), num(shift[1]),
                                                           num(scale[0]), num(scale[1])))
    return "\n".join(lines + ["}"]) + "\n"


def valve_axes(normal):
    """The same axes the classic projection would pick, written out (so the three syntaxes describe one mapping)."""
    best, axes = 0, BASEAXIS[0]
    for entry in BASEAXIS:
        d = sum(a * b for a, b in zip(normal, entry[0]))
        if d > best + 0.0001:
            best, axes = d, entry
    return axes[1], axes[2]


def axis_base(normal):
    """Q3Map2's ComputeAxisBase (common/qmath.h): the brush-primitives texture plane basis."""
    if all(abs(a - b) < 1e-6 for a, b in zip(normal, (0, 0, 1))):
        return (0, 1, 0), (1, 0, 0)
    if all(abs(a - b) < 1e-6 for a, b in zip(normal, (0, 0, -1))):
        return (0, 1, 0), (-1, 0, 0)
    up = (0, 0, 1)
    s = (normal[1] * up[2] - normal[2] * up[1], normal[2] * up[0] - normal[0] * up[2], normal[0] * up[1] - normal[1] * up[0])
    ls = math.sqrt(sum(x * x for x in s))
    s = tuple(x / ls for x in s)
    t = (normal[1] * s[2] - normal[2] * s[1], normal[2] * s[0] - normal[0] * s[2], normal[0] * s[1] - normal[1] * s[0])
    lt = math.sqrt(sum(x * x for x in t))
    t = tuple(x / lt for x in t)
    return tuple(-x for x in s), t


def bp_matrix(shift, scale, size):
    """A brush-primitives matrix: s = x/(scaleS*W) + shiftS/W in the face's own (axis_base) coordinates."""
    return ((1 / (scale[0] * size[0]), 0, shift[0] / size[0]), (0, 1 / (scale[1] * size[1]), shift[1] / size[1]))


def brushprim(faces, sizes, comment=None):
    lines = (["// " + comment] if comment else []) + ["{", "brushDef", "{"]
    for f in faces:
        tex, shift, scale = f.tex if isinstance(f.tex, tuple) else (f.tex, (0, 0), (0.5, 0.5))
        m = bp_matrix(shift, scale, sizes[tex])
        lines.append("%s ( ( %s ) ( %s ) ) %s 0 0 0" % (" ".join(pt(p) for p in f.p), " ".join(num(x) for x in m[0]),
                                                      " ".join(num(x) for x in m[1]), tex))
    return "\n".join(lines + ["}", "}"]) + "\n"


def bp_st(normal, point, shift, scale, size):
    s_axis, t_axis = axis_base(normal)
    x = sum(a * b for a, b in zip(point, s_axis))
    y = sum(a * b for a, b in zip(point, t_axis))
    m = bp_matrix(shift, scale, size)
    return [m[0][0] * x + m[0][1] * y + m[0][2], m[1][0] * x + m[1][1] * y + m[1][2]]


def write_map(path, worldspawn_body, extra_keys="", entities=None, header=""):
    entities = entities if entities is not None else STANDARD_ENTITIES
    text = header + "{\n\"classname\" \"worldspawn\"\n" + extra_keys + worldspawn_body + "}\n"
    for classname, origin in entities:
        text += "{\n\"classname\" \"%s\"\n\"origin\" \"%s\"\n}\n" % (classname, " ".join(num(v) for v in origin))
    return {path: text}


# ---------------------------------------------------------------- the standard room

ROOM = (-256, -256, 0, 256, 256, 256)
WALL = 16
CHECK = "apreg/checker"
STANDARD_ENTITIES = (("info_player_start", (-192, -192, 32)), ("light", (-192, -192, 192)))


def room_brushes(tex=CHECK, gap=None):
    """Six walls 16 thick around air x,y in [-256,256], z in [0,256]. `gap` = (width) cuts a full-height slot of
    that width, centred on y = 0, through the +x wall, open to the void outside."""
    x0, y0, z0, x1, y1, z1 = ROOM
    w = WALL
    walls = [("floor", (x0 - w, y0 - w, z0 - w, x1 + w, y1 + w, z0)), ("ceiling", (x0 - w, y0 - w, z1, x1 + w, y1 + w, z1 + w)),
             ("-x wall", (x0 - w, y0, z0, x0, y1, z1)),
             ("-y wall", (x0, y0 - w, z0, x1, y0, z1)), ("+y wall", (x0, y1, z0, x1, y1 + w, z1))]
    if gap is None:
        walls.append(("+x wall", (x1, y0, z0, x1 + w, y1, z1)))
    else:
        walls.append(("+x wall, south of the slot", (x1, y0, z0, x1 + w, -gap / 2, z1)))
        walls.append(("+x wall, north of the slot", (x1, gap / 2, z0, x1 + w, y1, z1)))
    return [(name, box_faces(*b, tex)) for name, b in walls]


def room_probes():
    """What every sealed standard room must answer, whatever is inside it."""
    x0, y0, z0, x1, y1, z1 = ROOM
    return [
        {"kind": "open", "point": [-192, -192, 128], "why": "room air beside the player start"},
        {"kind": "solid", "point": [x0 - 8, -192, 128], "why": "inside the -x wall"},
        {"kind": "solid", "point": [-192, -192, z0 - 8], "why": "inside the floor"},
        {"kind": "drawn", "point": [-200, -200, z0], "normal": [0, 0, 1], "why": "floor, seen from the room"},
        {"kind": "drawn", "point": [-200, -200, z1], "normal": [0, 0, -1], "why": "ceiling, seen from the room"},
        {"kind": "drawn", "point": [x0, -200, 64], "normal": [1, 0, 0], "why": "-x wall, seen from the room"},
        {"kind": "drawn", "point": [-200, y1, 64], "normal": [0, -1, 0], "why": "+y wall, seen from the room"},
    ]


def brushes_text(named, writer=classic, **kw):
    return "".join(writer(faces, comment=name, **kw) for name, faces in named)


# ---------------------------------------------------------------- cases

def case(cid, title, mechanism, map_path, expect_leak, probes, explanation, stages=("bsp",), extra=None):
    entry = {"id": cid, "title": title, "mechanism": mechanism, "map": map_path, "expect": {"leak": expect_leak},
             "probes": probes, "explanation": explanation, "stages": list(stages)}
    entry.update(extra or {})
    return entry


def r01():
    files = write_map("R01_sealed_room/maps/r01_room.map", brushes_text(room_brushes()))
    probes = room_probes() + [{"kind": "open", "point": [0, 0, 128], "why": "the middle of the room, on both block split planes"}]
    return files, [case("R01.room", "sealed room baseline", None, "R01_sealed_room/maps/r01_room.map", False, probes,
                        "Six 16-unit walls around 512 x 512 x 256 of air with a player start and a light. Every compiler "
                        "must seal it, keep its air open, its walls solid and its six inner faces drawn. It is the "
                        "positive control the other cases are variations of.", stages=("bsp", "vis", "light"))]


ROD = (64, -0.125, 0, 64.25, 0.125, 24)


def r02():
    files, cases = {}, []
    for cid, offset, title in (("R02.rod.split", 0.0, "0.25 x 0.25 x 24 rod across the y = 0 split"),
                               ("R02.rod.uncut", 0.25, "the same rod moved 0.25 to +y, wholly on one side of y = 0")):
        x0, y0, z0, x1, y1, z1 = ROD
        y0, y1 = y0 + offset, y1 + offset
        name = "r02_rod_split" if offset == 0 else "r02_rod_uncut"
        body = brushes_text(room_brushes() + [("rod %s..%s" % (num(y0), num(y1)), box_faces(x0, y0, z0, x1, y1, z1, CHECK))])
        path = "R02_rod/maps/%s.map" % name
        files.update(write_map(path, body))
        yc = (y0 + y1) / 2
        probes = room_probes() + [
            {"kind": "solid", "point": [64.125, yc + 0.0625, 12], "why": "inside the rod, +y half"},
            {"kind": "solid", "point": [64.125, yc - 0.0625, 12], "why": "inside the rod, -y half"},
            {"kind": "solid", "point": [64.125, yc + 0.0625, 1], "why": "inside the rod near its foot"},
            {"kind": "solid", "point": [64.125, yc - 0.0625, 23], "why": "inside the rod near its top"},
            {"kind": "open", "point": [64.125, yc, 30], "why": "air 6 units above the rod"},
            {"kind": "open", "point": [66, yc, 12], "why": "air 1.75 units beside the rod"},
            {"kind": "drawn", "point": [64.25, yc, 12], "normal": [1, 0, 0], "why": "the rod's +x face"},
        ]
        cases.append(case(cid, title, "M1" if offset == 0 else None, path, False, probes,
                          "The rod is 0.25 x 0.25 x 24 = 1.5 cubic units. Across y = 0 (a block split plane) it is cut into "
                          "two halves of 0.75 cubic units each; upstream SplitBrush discards a fragment below 1 cubic unit, "
                          "so both halves vanish from the tree: the leaves list no brush, the rod has no collision and the "
                          "player walks through it. The uncut control (one fragment of 1.5) keeps it." if offset == 0 else
                          "Control: the same rod wholly on +y of the y = 0 split is never cut, so no fragment is discarded "
                          "and every compiler keeps it solid.",
                          stages=("bsp", "vis", "light")))
    return files, cases


def r03():
    files, cases = {}, []
    for cid, past, mechanism, title in (
            ("R03.cross.0148", 0.148, "M2", "a step whose top face crosses x = 0 by 0.148: between ON_EPSILON and CLIP_EPSILON*2"),
            ("R03.cross.025", 0.25, None, "control: crossing by 0.25, past CLIP_EPSILON*2"),
            ("R03.cross.005", 0.05, None, "control: crossing by 0.05, inside ON_EPSILON")):
        name = "r03_cross_%s" % num(past).replace(".", "")
        body = brushes_text(room_brushes() + [("step x -64..%s, y -32..32, z 0..32" % num(past), box_faces(-64, -32, 0, past, 32, 32, CHECK))])
        path = "R03_crossing_face/maps/%s.map" % name
        files.update(write_map(path, body))
        mid = past / 2
        probes = room_probes() + [
            {"kind": "open", "point": [mid, 0, 96], "why": "air 64 above the strip of the step past x = 0"},
            {"kind": "open", "point": [mid, 0, 40], "why": "air 8 above the strip of the step past x = 0"},
            {"kind": "open", "point": [2, 0, 96], "why": "air 2 units past the end of the step"},
            {"kind": "solid", "point": [-32, 0, 16], "why": "inside the step"},
            {"kind": "drawn", "point": [-32, 0, 32], "normal": [0, 0, 1], "why": "the step's top face"},
            {"kind": "drawn", "point": [past, 0, 16], "normal": [1, 0, 0], "why": "the step's end face"},
        ]
        cases.append(case(cid, title, mechanism, path, False, probes,
                          "x = 0 is a block split plane, so the step's top face (z = 32, x from -64 to %s) crosses it. "
                          "BuildFaceTree_r classifies the face as crossing when its points lie more than ON_EPSILON (0.1) on "
                          "both sides, then clips it with CLIP_EPSILON*2 (0.2): a part between 0.1 and 0.2 wide is "
                          "classified crossing yet clips to nothing, so the whole face goes to the back. The front subtree "
                          "then has no plane at z = 32, one leaf straddles the step's strip x in [0, %s] and the air above "
                          "it, the step's fragment makes that leaf opaque, and the room air above the strip is filled. "
                          "At 0.148 this happens; at 0.25 the clip keeps a front part, at 0.05 the face is not crossing, "
                          "and both controls stay correct on every build." % (num(past), num(past)),
                          extra={"aliases": ["G04"]} if past == 0.05 else None))
    return files, cases


PIPE_MAPS = {
    "R04_m4_near_parallel/maps/r04_p08_nearby.map": "28b154f75417475465dab64509cd81c619c5971eb9fe9c3d98b0ff4e2b98895a",
    "R05_p10_shape/maps/r05_p10_base.map": "4cedfb8a128baecdd220b445b58790c77346a5a5f46c752a259bb5027fc81a9f",
    "R06_p08_sealed/maps/r06_p08_base.map": "b585fbde31e97b256393d80e297cb3ca562699a1d7d6acc3302a63652ce9825c",
    "R11_p10_nearby_sliver/maps/r11_p10_nearby.map": "accf8b91c03950ff9e855a262f491bf058dea2d2cc71f3ddff76c286cb3da791",
}
# The face samples that decide these cases, from the source maps' own faces (full precision): the point and the
# outward normal of the face, a point the source says is a visible face of that texture.
P08_NEARBY_FACE = [0.04361935984359557, -0.9990482227834825, 0]
P08_NEARBY = [[-58.6043605, 54.64965349999999, 0], [-61.918280375, 54.504964724999994, 5.371289086187509],
              [-65.23220025, 54.360275949999995, 10.742578172375017], [-61.918280375, 54.504964724999994, -5.371289086187509],
              [-65.23220025, 54.360275949999995, -10.742578172375017], [-55.290440625, 54.79434227499999, -5.371288999999997],
              [-51.97652075, 54.93903104999999, -10.742577999999995], [-55.290440625, 54.79434227499999, 5.371288999999997],
              [-51.97652075, 54.93903104999999, 10.742577999999995]]
P10_BASE_FACE = [0.2707268521928833, 0.6535924505957189, 0.7067700333390031]
P10_BASE = [[-763.0043579471607, 596.1198963907112, -60.99261252911365], [-763.7384788379183, 605.9858021489921, -69.83500112684828],
            [-764.4725997286757, 615.851707907273, -78.67738972458291], [-771.2123884383853, 589.9579509637858, -52.15022395175855],
            [-779.42041892961, 583.7960055368604, -43.30783537440344], [-761.5575573656545, 585.9587890039622, -52.15022389392387],
            [-760.1107567841483, 575.7976816172134, -43.3078352587341], [-755.5090071466847, 602.5770434461048, -69.8350011439239],
            [-748.0136563462086, 609.0341905014983, -78.67738975873415]]
P08_BASE_FACE = [0.08715566128940848, -0.9961947052184156, 0]
P08_BASE = [[6.521783000000001, -77.09929250000002, 0], [6.168146850000001, -77.13023162500001, 7.246933306574652],
            [5.8145107000000005, -77.16117075000001, 14.493866613149304], [6.168146850000001, -77.13023162500001, -7.246933306574652],
            [5.8145107000000005, -77.16117075000001, -14.493866613149304], [6.875419150000001, -77.06835337500002, -7.246933400000002],
            [7.2290553000000015, -77.03741425000003, -14.493866800000005], [6.875419150000001, -77.06835337500002, 7.246933400000002],
            [7.2290553000000015, -77.03741425000003, 14.493866800000005]]


# R11: the P10 pipe with one segment pulled 5 degrees about z near another: three bore faces and five bore points
P10_NEARBY_FACES = [
    ([0.029495038440397386, -0.022977254104576298, 0.9993007998101539], "the floor face of brush 307 (map line 2739)",
     [[65.049, 314.197, -83.871], [52.921, 302.645, -83.779], [40.793, 291.092, -83.687], [73.23, 318.143, -84.022],
      [81.41, 322.089, -84.173], [68.996, 321.804, -83.813], [72.944, 329.411, -83.755]]),
    ([0.02187455133668658, -0.012985732267478742, 0.9996763850173195], "the floor face of brush 308 (map line 2747)",
     [[53.773, 292.462, -83.973], [45.591, 288.517, -83.845], [37.41, 284.572, -83.717], [49.826, 284.855, -83.985],
      [45.88, 277.247, -83.998], [65.9, 304.016, -84.088], [78.027, 315.569, -84.203]]),
    ([-0.43225600093241817, -0.559085263099685, 0.7075156664291394], "a slanted bore face (map line 2769)",
     [[102.874, 357.645, -50.327], [93.583, 350.045, -62.009], [84.291, 342.445, -73.691], [109.652, 359.633, -44.615],
      [116.429, 361.622, -38.903], [105.388, 363.256, -44.357]]),
]
P10_NEARBY_BORE = [[23.650396, 380.542256, -20.127506], [44.572602, 366.388518, -15.415647],
                   [121.599811, 260.853132, -42.55114], [350.118658, 145.887534, -43.12829], [372.514138, 133.520196, -43.12829]]


def pipes():
    drawn = lambda points, normal, why: [{"kind": "drawn", "point": p, "normal": normal, "why": why} for p in points]  # noqa: E731
    return {}, [
        case("R04.m4.p08_nearby", "M4: a near-parallel face discarded as on another plane (pipe P08 with a nearby pull)", "M4",
             "R04_m4_near_parallel/maps/r04_p08_nearby.map", False,
             drawn(P08_NEARBY, P08_NEARBY_FACE, "the bore face of segment brush 101 (map line 919), seen from the bore"),
             "No source-independent reduction of M4 exists yet, so this is the shape that shows it: a 354-brush pipe "
             "whose one segment is pulled 3 units, from Auto-Pigeon's own pipe test generator (no corpus, no game data). "
             "The face of brush 101 at these points is nearly parallel to a neighbouring segment's face; "
             "BuildFaceTree_r treats it as ON that plane and discards it, and its drawn triangles filter into the "
             "neighbour's solid leaves (traced in Auto-Pigeon's compiler investigation). KNOWN FAILURE until NEW_340 "
             "repairs it: P1+P2 does not "
             "touch it, and the two attempted repairs (P3, P4) broke R05 and R06."),
        case("R05.p3.p10_base", "control: the P10 pipe whose face P3 lost", None, "R05_p10_shape/maps/r05_p10_base.map", False,
             drawn(P10_BASE, P10_BASE_FACE, "a slanted bore face of the P10 pipe (map line 820)"),
             "A 590-brush pipe with a bend in all three axes. Upstream draws these 9 points; the rejected M4 repair "
             "P3 (never committed) lost them. Any later M4 repair must keep them drawn.", extra={"aliases": ["G03"]}),
        case("R06.p4.p08_base", "control: the sealed P08 pipe that P4 reported as leaking; its M2 face", "M2",
             "R06_p08_sealed/maps/r06_p08_base.map", False,
             drawn(P08_BASE, P08_BASE_FACE, "the wall face y ~ -77 of brush 179 (map line 1620), seen from the room"),
             "The P08 pipe is sealed: the rejected M4 repair P4 (never committed) reported a leak here, so the leak "
             "verdict is the first assertion. The 9 face points are the M2 defect of R03 on a real shape: a wall "
             "face piece crosses a node plane by between 0.1 and 0.2, upstream sends it wholly to the back, the leaf that "
             "then straddles the wall turns opaque, and the face is not drawn there.", extra={"aliases": ["G04"]}),
        case("R11.m3.p10_nearby", "M3: a split sliver the face tree has no plane for fills a pipe bore (P10, nearby pull)", "M3",
             "R11_p10_nearby_sliver/maps/r11_p10_nearby.map", False,
             [{"kind": "open", "point": p, "why": "declared bore air of the pipe"} for p in P10_NEARBY_BORE]
             + [probe for normal, why, points in P10_NEARBY_FACES for probe in drawn(points, normal, why + ", seen from the bore")],
             "A 630-brush pipe from the same generator. The visible-face tree consumes the bore face of brush 324 (map "
             "line 2878) as ON a neighbouring segment's face plane, every point of its VISIBLE part being within "
             "ON_EPSILON of it. SplitBrush then decides the same question from the brush's full, 1/8-snapped "
             "windings, finds a vertex 0.104 past that plane and splits for real: the 0.1-thick sliver goes to the "
             "bore side, where the tree has no plane bounding it (the face that would have been one was consumed), "
             "lands in one leaf that holds most of the bore and makes it opaque. Five bore points read solid and "
             "three bore faces lose their drawn surfaces."),
    ]


def r07():
    files, cases = {}, []
    for cid, width, leak, mechanism, title in (
            ("R07.open.1", 1.0, True, None, "a one-unit slot through a wall to the void: a real opening"),
            ("R08.slit.0125", 0.125, True, "slit_closure", "a 0.125 slot through a wall to the void")):
        name = "r07_open_1" if width == 1 else "r08_slit_0125"
        directory = "R07_open_enclosure" if width == 1 else "R08_controls"
        body = brushes_text(room_brushes(gap=width))
        path = "%s/maps/%s.map" % (directory, name)
        files.update(write_map(path, body))
        explanation = ("The +x wall is two brushes with a full-height slot of %s units between them, open to the void. "
                       "The map leaks; a compiler that reports it sealed has closed a real opening. P1+P2 must not "
                       "change that: it keeps material, it does not invent it." % num(width)) if width == 1 else (
                       "The same slot 0.125 wide. The source describes an opening, so the honest verdict is a leak, but "
                       "every Q3Map2 build measured closes it: a portal with fewer than three edges longer than 0.2 "
                       "(EDGE_LENGTH) is dropped (portals.cpp MakeNodePortal/SplitNodePortals), and the block split "
                       "y = 0 through this slot halves it again, so a slot centred on a split closes up to 0.375 and "
                       "one elsewhere up to 0.1875 (G06.outer.025 stays open). A representability limit of the portal "
                       "stage, kept as a KNOWN FAILURE rather than rewritten as an expected seal.")
        cases.append(case(cid, title, mechanism, path, leak, [], explanation, extra={"aliases": ["G06"]} if width == 1 else None))
    return files, cases


def r08():
    files, cases = {}, []
    defs = {
        "touch": ([("box A", (-64, -32, 0, 0, 32, 32)), ("box B, touching A at x = 0", (0, -32, 0, 64, 32, 32))],
                  [{"kind": "solid", "point": [-0.5, 0, 16], "why": "box A at the shared face"},
                   {"kind": "solid", "point": [0.5, 0, 16], "why": "box B at the shared face"},
                   {"kind": "drawn", "point": [-32, 0, 32], "normal": [0, 0, 1], "why": "top of A"},
                   {"kind": "drawn", "point": [32, 0, 32], "normal": [0, 0, 1], "why": "top of B"},
                   {"kind": "open", "point": [0, 0, 40], "why": "air above the seam"}],
                  "Two boxes share the face x = 0, which is also a block split plane. Both stay solid and both tops drawn."),
        "overlap": ([("box A", (-64, -32, 0, 8, 32, 32)), ("box B, overlapping A by 16", (-8, -32, 0, 64, 32, 48))],
                    [{"kind": "solid", "point": [0, 0, 16], "why": "inside both boxes"},
                     {"kind": "drawn", "point": [-32, 0, 32], "normal": [0, 0, 1], "why": "top of A"},
                     {"kind": "drawn", "point": [32, 0, 48], "normal": [0, 0, 1], "why": "top of B"},
                     {"kind": "drawn", "point": [-8, 0, 40], "normal": [-1, 0, 0], "why": "B's -x face above A"},
                     {"kind": "open", "point": [-20, 0, 40], "why": "air above A, beside B"}],
                    "Box B overlaps box A by 16 units and rises above it. The overlap is solid, A's top is drawn up to B, "
                    "B's side is drawn above A."),
        "nearparallel": (None,
                         [{"kind": "drawn", "point": [-32, 0, 32], "normal": [0, 0, 1], "why": "flat top of A"},
                          {"kind": "drawn", "point": [32, 0, 32.1117], "normal": [-0.0034907, 0, 0.9999939], "why": "tilted top of B"},
                          {"kind": "solid", "point": [32, 0, 16], "why": "inside B"},
                          {"kind": "open", "point": [32, 0, 40], "why": "air above B"},
                          {"kind": "open", "point": [-32, 0, 40], "why": "air above A"}],
                         "Box A has a flat top at z = 32 for x in [-64, 0]; box B, from x = 0 to 64, has a top that starts at "
                         "z = 32 at x = 0 and rises 0.2 degrees. The two top planes differ by 0.2 degrees and meet along "
                         "x = 0: a near-parallel pair of planes that are NOT the same plane. Both tops must be drawn."),
        "zeroarea": ([("a zero-thickness brush: x from 32 to 32", (32, -32, 0, 32, 32, 64))],
                     [{"kind": "open", "point": [32, 0, 32], "why": "where the zero-thickness brush is: it has no volume"},
                      {"kind": "open", "point": [40, 0, 32], "why": "air beside it"}],
                     "A brush whose two x faces coincide encloses no volume. The compiler must not crash, must not seal or "
                     "fill the room with it, and leaves its place air."),
    }
    for key, (boxes, probes, explanation) in defs.items():
        if key == "nearparallel":
            t = math.tan(math.radians(0.2))
            tilted = [Face((64, -32, 0), (64, -32, 32 + 64 * t), (64, 32, 32 + 64 * t), CHECK),
                      Face((0, 32, 0), (0, 32, 32), (0, -32, 32), CHECK),
                      Face((64, 32, 0), (64, 32, 32 + 64 * t), (0, 32, 32), CHECK),
                      Face((0, -32, 0), (0, -32, 32), (64, -32, 32 + 64 * t), CHECK),
                      Face((64, 32, 32 + 64 * t), (64, -32, 32 + 64 * t), (0, -32, 32), CHECK),
                      Face((64, -32, 0), (64, 32, 0), (0, 32, 0), CHECK)]
            named = room_brushes() + [("box A, flat top", box_faces(-64, -32, 0, 0, 32, 32, CHECK)),
                                      ("box B, top tilted 0.2 degrees up from x = 0", tilted)]
        else:
            named = room_brushes() + [(name, box_faces(*b, CHECK)) for name, b in boxes]
        path = "R08_controls/maps/r08_%s.map" % key
        files.update(write_map(path, brushes_text(named)))
        cases.append(case("R08.%s" % key, key.replace("nearparallel", "near-parallel").replace("zeroarea", "zero-area"),
                          None, path, False, room_probes() + probes, explanation,
                          extra={"aliases": [{"touch": "G06", "overlap": "G06", "zeroarea": "G07", "nearparallel": "G07"}[key]]}))
    return files, cases


# ---------------------------------------------------------------- G01, G05: near-parallel faces (M4)

M4_TILT = 2.5           # degrees between the wall's face normal and -y
M4_CENTRE = (128, 92)   # the wall is turned and moved about this point


def tilted_box(x0, x1, z0, z1, normal, dist, yback, tex=CHECK):
    """A brush between x0..x1 and z0..z1 whose front face lies on the plane normal . p = dist (normal horizontal,
    pointing to -y), its back face on y = yback."""
    ya, yb = (dist - normal[0] * x0) / normal[1], (dist - normal[0] * x1) / normal[1]
    return [Face((x1, yback, z0), (x1, yback, z1), (x0, yback, z1), tex), Face((x0, ya, z0), (x0, ya, z1), (x1, yb, z1), tex),
            Face((x1, yb, z0), (x1, yb, z1), (x1, yback, z1), tex), Face((x0, yback, z0), (x0, yback, z1), (x0, ya, z1), tex),
            Face((x1, yback, z1), (x1, yb, z1), (x0, ya, z1), tex), Face((x1, yb, z0), (x1, yback, z0), (x0, yback, z0), tex)]


def turned(p, angle, move):
    a = math.radians(angle)
    x, y = p[0] - M4_CENTRE[0], p[1] - M4_CENTRE[1]
    return [M4_CENTRE[0] + x * math.cos(a) - y * math.sin(a) + move[0], M4_CENTRE[1] + x * math.sin(a) + y * math.cos(a) + move[1],
            p[2] + move[2]]


def m4_inset(phase, recess, angle=0.0, move=(0, 0, 0)):
    """A wall of four pieces whose faces share one plane, around an inset piece B whose face is parallel to it and
    `recess` behind it. `phase` places B's face corner at y = 61 + phase, i.e. where the 1/8 vertex snap moves it."""
    t = math.radians(M4_TILT)
    n = (math.sin(t), -math.cos(t), 0)
    d_b = n[0] * 96 + n[1] * (61 + phase)
    d_a = d_b + recess
    parts = [("wall piece west", tilted_box(33, 96, 0, 128, n, d_a, 128)), ("wall piece east", tilted_box(159, 222, 0, 128, n, d_a, 128)),
             ("wall piece below", tilted_box(96, 159, 0, 32, n, d_a, 128)), ("wall piece above", tilted_box(96, 159, 96, 128, n, d_a, 128)),
             ("inset piece B, %s behind the wall's plane" % num(recess), tilted_box(96, 159, 32, 96, n, d_b, 128))]
    parts = [(name, [Face(*[tuple(turned(q, angle, move)) for q in f.p], f.tex) for f in faces]) for name, faces in parts]
    a = math.radians(angle)
    normal = [n[0] * math.cos(a) - n[1] * math.sin(a), n[0] * math.sin(a) + n[1] * math.cos(a), 0]
    on_b = lambda x, z: turned((x, (d_b - n[0] * x) / n[1], z), angle, move)  # noqa: E731
    probes = [{"kind": "drawn", "point": on_b(x, z), "normal": normal, "why": "the inset's face, seen from the room"}
              for x in (112, 128, 144) for z in (48, 64, 80)]
    probes += [{"kind": "solid", "point": turned((128, 100, 64), angle, move), "why": "inside the inset"},
               {"kind": "open", "point": turned((128, (d_b - n[0] * 128) / n[1] - 1, 64), angle, move), "why": "air 1 unit in front of the inset"},
               {"kind": "drawn", "point": turned((64, (d_a - n[0] * 64) / n[1], 64), angle, move), "normal": normal,
                "why": "the west wall piece's face"}]
    return parts, probes


M4_EXPLANATION = ("Four wall pieces share one face plane, tilted %s degrees from the y axis; an inset piece B between them "
                  "has a parallel face %%s behind that plane. When the visible-face tree splits on the wall's plane, "
                  "B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits the "
                  "leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the "
                  "snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase %%s), the "
                  "triangles reach only the opaque leaf behind it, no visible leaf references the surface and it "
                  "is not emitted. The face must be drawn at every phase, offset and orientation." % num(M4_TILT))


def g01_g05():
    files, cases = {}, []
    rows = [("G01.m4.inset", "M4: an inset face 0.0885 behind a near-coplanar wall, its corners snapped further behind",
             "M4", dict(phase=0.0859, recess=0.0885), "g01_m4_inset")]
    for label, kw, title in (
            ("phase0547", dict(phase=0.0547, recess=0.0885), "the inset at snap phase 0.0547 (the snap leaves it within 0.1)"),
            ("phase0625", dict(phase=0.0625, recess=0.0885), "the inset at snap phase 0.0625 (first phase the snap carries past 0.1)"),
            ("phase1094", dict(phase=0.1094, recess=0.0885), "the inset at snap phase 0.1094 (last such phase)"),
            ("phase1172", dict(phase=0.1172, recess=0.0885), "the inset at snap phase 0.1172 (the snap leaves it within 0.1)"),
            ("recess0050", dict(phase=0.0859, recess=0.05), "the inset 0.05 behind the wall's plane"),
            ("recess0095", dict(phase=0.0859, recess=0.095), "the inset 0.095 behind the wall's plane"),
            ("recess0110", dict(phase=0.0859, recess=0.11), "the inset 0.11 behind: past ON_EPSILON, a plane of its own"),
            ("proud0885", dict(phase=0.0859, recess=-0.0885), "the inset 0.0885 in FRONT of the wall's plane"),
            ("moved", dict(phase=0.0859, recess=0.0885, move=(-37.3, -21.45, 8.2)), "G01 moved by (-37.3, -21.45, 8.2)"),
            ("turn15", dict(phase=0.0859, recess=0.0885, angle=15), "G01 turned 15 degrees about z"),
            ("turn45", dict(phase=0.0859, recess=0.0885, angle=45), "G01 turned 45 degrees about z"),
            ("turn15.moved", dict(phase=0.0859, recess=0.0885, angle=15, move=(-37.3, -21.45, 8.2)), "G01 turned 15 degrees and moved"),
            ("turn45.moved", dict(phase=0.0859, recess=0.0885, angle=45, move=(-37.3, -21.45, 8.2)), "G01 turned 45 degrees and moved")):
        rows.append(("G05.m4.%s" % label, title, None, kw, "g05_m4_%s" % label.replace(".", "_")))
    for cid, title, mechanism, kw, name in rows:
        parts, probes = m4_inset(**kw)
        path = "G01_G05_near_parallel/maps/%s.map" % name
        files.update(write_map(path, brushes_text(room_brushes() + parts)))
        cases.append(case(cid, title, mechanism, path, False, room_probes() + probes,
                          M4_EXPLANATION % (num(kw["recess"]), num(kw["phase"]))))
    return files, cases


# ---------------------------------------------------------------- G02, G07: sections and fragments smaller than 0.2 (M3)

def g02():
    files, cases = {}, []
    for cid, w, title, mechanism in (
            ("G02.m3.rod_along", 0.1875, "M3: a 3/16 rod lying along y across the y = 0 split", "M3"),
            ("G02.rod_along.025", 0.25, "control: the same rod 1/4 thick (its section is not small)", None)):
        x0, x1, z0, z1 = 64, 64 + w, 8, 8 + w
        name = "g02_rod_along_%s" % num(w).replace(".", "")
        body = brushes_text(room_brushes() + [("rod x %s..%s, y -4..8, z %s..%s" % (num(x0), num(x1), num(z0), num(z1)),
                                                box_faces(x0, -4, z0, x1, 8, z1, CHECK))])
        path = "G02_G07_small_sections/maps/%s.map" % name
        files.update(write_map(path, body))
        xc, zc = (x0 + x1) / 2, (z0 + z1) / 2
        probes = room_probes() + [
            {"kind": "solid", "point": [xc, -2, zc], "why": "the rod's -y part, 2 units from the split"},
            {"kind": "solid", "point": [xc, -3.5, zc], "why": "the rod's -y part near its end"},
            {"kind": "solid", "point": [xc, 4, zc], "why": "the rod's +y part, 4 units from the split"},
            {"kind": "open", "point": [xc, -2, zc + 4], "why": "air 4 above the rod's -y part"},
            {"kind": "open", "point": [xc, -6, zc], "why": "air past the rod's -y end"},
        ]
        cases.append(case(cid, title, mechanism, path, False, probes,
                          "The rod is %s x %s in section and runs from y = -4 to y = 8 across the block split y = 0. Its "
                          "section by y = 0 has only two edges longer than 0.2, so upstream SplitBrush called the brush "
                          "'not really split' and sent it whole to the side it reaches furthest into: the 4 units on -y "
                          "kept no brush in their leaves (no collision, and contents read as air). The whole rod must be "
                          "solid; a 1/4 rod (four long edges) was always split and is the control." % (num(w), num(w))))
    return files, cases


def g07():
    files, cases = {}, []
    for cid, s, title, mechanism in (("G07.m3.cube.025", 0.25, "M3: a 1/4 cube across x = 0 and y = 0 (four fragments)", "M3"),
                                     ("G07.cube.05", 0.5, "control: a 1/2 cube across x = 0 and y = 0", None)):
        name = "g07_cube_%s" % num(s).replace(".", "")
        body = brushes_text(room_brushes() + [("cube of %s centred on x = 0, y = 0" % num(s), box_faces(-s / 2, -s / 2, 8, s / 2, s / 2, 8 + s, CHECK))])
        path = "G02_G07_small_sections/maps/%s.map" % name
        files.update(write_map(path, body))
        q = s / 4
        probes = room_probes() + [{"kind": "solid", "point": [sx * q, sy * q, 8 + s / 2], "why": "the cube's %s%s quarter" % (
            "+x" if sx > 0 else "-x", "+y" if sy > 0 else "-y")} for sx in (-1, 1) for sy in (-1, 1)] + [
            {"kind": "open", "point": [0, 0, 8 + s + 1], "why": "air 1 above the cube"}]
        cases.append(case(cid, title, mechanism, path, False, probes,
                          "A cube of %s centred on the two block split planes x = 0 and y = 0 is cut into four quarters of "
                          "positive volume. After the first split each half's section by the second plane is %s x %s; "
                          "below 0.2 on a side, upstream sent each half whole to one side of the second plane and two "
                          "quarters kept no brush. Every quarter must be solid." % (num(s), num(s / 2), num(s))))
    return files, cases


# ---------------------------------------------------------------- G06: openings, slits and pockets

def g06():
    files, cases = {}, []
    for cid, w in (("G06.slit.1", 1.0), ("G06.slit.05", 0.5)):
        name = "g06_slit_%s" % num(w).replace(".", "")
        body = brushes_text(room_brushes() + [("block west", box_faces(32, -64, 0, 96, 64, 64, CHECK)),
                                              ("block east, %s from the west block" % num(w), box_faces(96 + w, -64, 0, 160 + w, 64, 64, CHECK))])
        path = "G06_openings/maps/%s.map" % name
        files.update(write_map(path, body))
        probes = room_probes() + [{"kind": "open", "point": [96 + w / 2, 0, 32], "why": "air in the slit"},
                                  {"kind": "solid", "point": [64, 0, 32], "why": "the west block"},
                                  {"kind": "solid", "point": [128 + w, 0, 32], "why": "the east block"},
                                  {"kind": "drawn", "point": [96, 0, 32], "normal": [1, 0, 0], "why": "the west block's face in the slit"},
                                  {"kind": "drawn", "point": [96 + w, 0, 32], "normal": [-1, 0, 0], "why": "the east block's face in the slit"}]
        cases.append(case(cid, "a reachable slit %s wide between two blocks inside the room" % num(w), None, path, False, probes,
                          "Two blocks %s apart inside the sealed room; the slit is open to the room above. Its air must stay "
                          "open and both faces in it drawn: no closure of a reachable slit, no fill." % num(w)))
    x0, y0, z0, x1, y1, z1, t = 48, -48, 0, 144, 48, 96, 8
    walls = [("pocket floor", (x0, y0, z0, x1, y1, z0 + t)), ("pocket roof", (x0, y0, z1 - t, x1, y1, z1)),
             ("pocket -x", (x0, y0, z0 + t, x0 + t, y1, z1 - t)), ("pocket +x", (x1 - t, y0, z0 + t, x1, y1, z1 - t)),
             ("pocket -y", (x0 + t, y0, z0 + t, x1 - t, y0 + t, z1 - t)), ("pocket +y", (x0 + t, y1 - t, z0 + t, x1 - t, y1, z1 - t))]
    path = "G06_openings/maps/g06_pocket.map"
    files.update(write_map(path, brushes_text(room_brushes() + [(n, box_faces(*b, CHECK)) for n, b in walls])))
    cases.append(case("G06.pocket", "an enclosed pocket: a hollow block sealed from the room", None, path, False,
                      room_probes() + [{"kind": "solid", "point": [x0 + 4, 0, 48], "why": "the pocket's -x wall"},
                                       {"kind": "drawn", "point": [x1, 0, 48], "normal": [1, 0, 0], "why": "the block's outside face"},
                                       {"kind": "open", "point": [x1 + 8, 0, 48], "why": "room air beside the block"}],
                      "Six 8-unit walls around 80 x 80 x 80 of air no entity can reach. The map is sealed, the walls solid "
                      "and the room around them open. Nothing is asserted about the pocket's own air: it is unreachable, "
                      "and Q3Map2 may fill it; a player-sized passage is the slit case, not this one."))
    files_r, cases_r = {}, []
    gap, centre = 0.25, 100.0
    x0r, y0r, z0r, x1r, y1r, z1r = ROOM
    named = [b for b in room_brushes() if b[0] != "+x wall"] + [
        ("+x wall, south of the slot", box_faces(x1r, y0r, z0r, x1r + WALL, centre - gap / 2, z1r, CHECK)),
        ("+x wall, north of the slot", box_faces(x1r, centre + gap / 2, z0r, x1r + WALL, y1r, z1r, CHECK))]
    path = "G06_openings/maps/g06_outer_025.map"
    files.update(write_map(path, brushes_text(named)))
    cases.append(case("G06.outer.025", "a 0.25 slot to the void at y = 100: the narrowest opening Q3Map2 keeps", None, path, True, [],
                      "A full-height slot 0.25 wide through the +x wall, centred on y = 100 (no split plane through it). "
                      "The map leaks. Portals with fewer than three edges longer than 0.2 are dropped (portals.cpp, "
                      "EDGE_LENGTH), so 0.25 is the narrowest opening measured to stay open; R08.slit.0125 is the "
                      "known failure below that limit."))
    return files, cases


# ---------------------------------------------------------------- S: narrow openings (NEW_340A)

CONTENTS_DETAIL = 0x8000000
SLOT_CENTRE = 100.0          # off every block split plane
WINDOW = (36, 64, 164, 192)  # y0 z0 y1 z1 of the +x wall's window for a turned slot: centre (100, 128), off the splits


def slot_wall(y_lo, y_hi):
    """The standard room with its +x wall cut by a full-height slot from y_lo to y_hi, open to the void."""
    x0, y0, z0, x1, y1, z1 = ROOM
    named = [b for b in room_brushes() if b[0] != "+x wall"]
    if y_hi > y_lo:
        named += [("+x wall, south of the slot", box_faces(x1, y0, z0, x1 + WALL, y_lo, z1, CHECK)),
                  ("+x wall, north of the slot", box_faces(x1, y_hi, z0, x1 + WALL, y1, z1, CHECK))]
    return named


def slot_route(y_lo, y_hi):
    """The leak route must cross the slot itself: the slab of the slot's opening through the wall."""
    x1 = ROOM[3]
    return [{"min": [x1 - 1, y_lo - 0.01, ROOM[2]], "max": [x1 + WALL + 1, y_hi + 0.01, ROOM[5]], "why": "through the slot"}]


def clip_polygon(points, normal, dist):
    """Sutherland-Hodgman: keep the part of a 2D convex polygon with normal . p >= dist."""
    out = []
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        da, db = normal[0] * a[0] + normal[1] * a[1] - dist, normal[0] * b[0] + normal[1] * b[1] - dist
        if da >= 0:
            out.append(a)
        if (da >= 0) != (db >= 0):
            t = da / (da - db)
            out.append((a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])))
    return out


def oriented(p1, p2, p3, outward):
    """Three points of a face ordered the way the map format reads them: (p1 - p2) x (p3 - p2) is the outward normal."""
    a = [p1[i] - p2[i] for i in range(3)]
    b = [p3[i] - p2[i] for i in range(3)]
    n = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    return (p1, p2, p3) if sum(n[i] * outward[i] for i in range(3)) > 0 else (p3, p2, p1)


def prism_x(polygon, xa, xb, tex=CHECK):
    """A convex polygon of the yz plane (counter-clockwise) extruded from x = xa to x = xb, as brush faces."""
    faces = []
    cy = sum(p[0] for p in polygon) / len(polygon)
    cz = sum(p[1] for p in polygon) / len(polygon)
    for i, a in enumerate(polygon):
        b = polygon[(i + 1) % len(polygon)]
        ey, ez = b[0] - a[0], b[1] - a[1]
        out = (0, ez, -ey) if (ez * (a[0] - cy) - ey * (a[1] - cz)) > 0 else (0, -ez, ey)
        faces.append(Face(*oriented((xa, a[0], a[1]), (xb, a[0], a[1]), (xb, b[0], b[1]), out), tex))
    p = polygon
    faces.append(Face(*oriented((xb, p[0][0], p[0][1]), (xb, p[1][0], p[1][1]), (xb, p[2][0], p[2][1]), (1, 0, 0)), tex))
    faces.append(Face(*oriented((xa, p[0][0], p[0][1]), (xa, p[1][0], p[1][1]), (xa, p[2][0], p[2][1]), (-1, 0, 0)), tex))
    return faces


def turned_slot_wall(width, angle, move=(0.0, 0.0)):
    """The +x wall with a window closed by two plugs leaving a slot `width` wide through the window centre, turned
    `angle` degrees about x (0 = a horizontal slot), the whole window and plugs moved by `move` (y, z)."""
    x0, y0, z0, x1, y1, z1 = ROOM
    wy0, wz0, wy1, wz1 = WINDOW[0] + move[0], WINDOW[1] + move[1], WINDOW[2] + move[0], WINDOW[3] + move[1]
    cy, cz = (wy0 + wy1) / 2, (wz0 + wz1) / 2
    named = [b for b in room_brushes() if b[0] != "+x wall"] + [
        ("+x wall, south of the window", box_faces(x1, y0, z0, x1 + WALL, wy0, z1, CHECK)),
        ("+x wall, north of the window", box_faces(x1, wy1, z0, x1 + WALL, y1, z1, CHECK)),
        ("+x wall, below the window", box_faces(x1, wy0, z0, x1 + WALL, wy1, wz0, CHECK)),
        ("+x wall, above the window", box_faces(x1, wy0, wz1, x1 + WALL, wy1, z1, CHECK))]
    a = math.radians(angle)
    n = (-math.sin(a), math.cos(a))
    d = n[0] * cy + n[1] * cz
    rect = [(wy0, wz0), (wy1, wz0), (wy1, wz1), (wy0, wz1)]
    upper = clip_polygon(rect, n, d + width / 2)
    lower = clip_polygon(rect, (-n[0], -n[1]), -(d - width / 2))
    named += [("plug above the slot", prism_x(upper, x1, x1 + WALL)), ("plug below the slot", prism_x(lower, x1, x1 + WALL))]
    route = [{"min": [x1 - 1, cy - 2, cz - 2], "max": [x1 + WALL + 1, cy + 2, cz + 2], "why": "through the slot at the window centre"}]
    return named, route


def s_cases():
    files, cases = {}, []

    def add(cid, title, path, named, leak, probes, explanation, mechanism=None, stages=("bsp",), extra=None, entities=None):
        files.update(write_map(path, brushes_text(named), entities=entities))
        cases.append(case(cid, title, mechanism, path, leak, probes, explanation, stages=stages, extra=extra))

    d = "S02_S12_narrow_openings/maps/"
    # S02: off-split slots (0.25 is G06.outer.025)
    for w in (0.125, 0.1875, 0.5):
        lo, hi = SLOT_CENTRE - w / 2, SLOT_CENTRE + w / 2
        add("S02.off.%s" % num(w).replace("0.", ""), "a %s slot to the void at y = 100, off every split" % num(w),
            d + "s02_off_%s.map" % num(w).replace(".", ""), slot_wall(lo, hi), True, [],
            "A full-height slot %s wide through the +x wall, centred on y = 100 where no block split passes. The map leaks "
            "and the route must cross the slot. Before NEW_340A the slot's leaf existed but its portals to the room and to "
            "the void (%s x 256 rectangles) were dropped as 'tiny' (fewer than three edges longer than 0.2): the map read "
            "sealed below 0.25." % (num(w), num(w)), extra={"leak_route": slot_route(lo, hi)})
    # S03: split-centred slots (0.125 is R08.slit.0125)
    for w in (0.1875, 0.25, 0.5):
        add("S03.split.%s" % num(w).replace("0.", ""), "a %s slot to the void centred on the block split y = 0" % num(w),
            d + "s03_split_%s.map" % num(w).replace(".", ""), slot_wall(-w / 2, w / 2), True, [],
            "A full-height slot %s wide centred on y = 0, a block split plane. The map leaks through the slot. Before "
            "NEW_340A the forced block split consumed both slot faces lying within ON_EPSILON of it (a half up to 0.1), "
            "or split the slot into halves whose portals were dropped as 'tiny' (a half up to 0.2): sealed up to 0.375."
            % num(w), extra={"leak_route": slot_route(-w / 2, w / 2)})
    # S04: a 0.125 slot through a window, horizontal and turned 15 and 45 degrees about x
    for angle in (0, 15, 45):
        named, route = turned_slot_wall(0.125, angle)
        add("S04.turned.%d" % angle, "a 0.125 slot through two plugs in a window, turned %d degrees" % angle,
            d + "s04_turned_%d.map" % angle, named, True, [],
            "The +x wall has a 128 x 128 window centred on (y, z) = (100, 128); two plugs fill it except a slot 0.125 wide "
            "through its centre, turned %d degrees about x (0 is horizontal). The map leaks through the slot, with no "
            "grid or epsilon change: the verdict follows the opening." % angle, extra={"leak_route": route})
    # S05: the same openings moved across the split and rounding phases
    for cid, lo, hi, why in (("S05.moved.on_split", 0.0, 0.125, "one face ON the block split y = 0"),
                             ("S05.moved.straddle", -0.09375, 0.03125, "both faces within ON_EPSILON of the block split, unevenly")):
        add(cid, "a 0.125 slot with %s" % why, d + cid.lower().replace(".", "_") + ".map", slot_wall(lo, hi), True, [],
            "A full-height slot 0.125 wide from y = %s to %s: %s. The map leaks through the slot wherever the world's "
            "block boundary falls." % (num(lo), num(hi), why), extra={"leak_route": slot_route(lo, hi)})
    named, route = turned_slot_wall(0.125, 45, move=(0.3125, 0.1875))
    add("S05.moved.turned45", "the 45-degree 0.125 slot moved by (0, 0.3125, 0.1875)", d + "s05_moved_turned45.map", named, True,
        [], "S04.turned.45 with its window, plugs and slot moved 0.3125 in y and 0.1875 in z, off the 1/8 grid phase it had.",
        extra={"leak_route": route})
    # S06: touching slabs seal
    x0, y0, z0, x1, y1, z1 = ROOM
    for cid, at in (("S06.touch.split", 0.0), ("S06.touch.off", SLOT_CENTRE)):
        add(cid, "the +x wall as two slabs touching at y = %s" % num(at), d + cid.lower().replace(".", "_") + ".map",
            slot_wall(at, at) + [("+x wall, south slab", box_faces(x1, y0, z0, x1 + WALL, at, z1, CHECK)),
                                 ("+x wall, north slab", box_faces(x1, at, z0, x1 + WALL, y1, z1, CHECK))], False,
            room_probes() + [{"kind": "solid", "point": [x1 + 8, at - 0.03, 128], "why": "south slab at the seam"},
                             {"kind": "solid", "point": [x1 + 8, at + 0.03, 128], "why": "north slab at the seam"},
                             {"kind": "drawn", "point": [x1, at + 4, 128], "normal": [-1, 0, 0], "why": "+x wall beside the seam, seen from the room"}],
            "The +x wall is two brushes sharing the face y = %s exactly: no opening, so the map is sealed and the seam "
            "solid. A portal predicate that kept a zero-width crack would leak here." % num(at))
    # S07: overlap, a closed thin pocket, a blind slot
    add("S07.overlap", "the +x wall as two slabs overlapping by 0.125 at y = 0", d + "s07_overlap.map",
        slot_wall(0, 0) + [("+x wall, south slab", box_faces(x1, y0, z0, x1 + WALL, 0.0625, z1, CHECK)),
                           ("+x wall, north slab", box_faces(x1, -0.0625, z0, x1 + WALL, y1, z1, CHECK))], False,
        room_probes() + [{"kind": "solid", "point": [x1 + 8, 0, 128], "why": "inside the overlap"}],
        "Two wall slabs overlap by 0.125 across the block split: sealed, no invented route outside.")
    lo, hi = SLOT_CENTRE - 0.0625, SLOT_CENTRE + 0.0625
    add("S07.pocket", "a 0.125 slot closed at both ends inside the +x wall", d + "s07_pocket.map",
        slot_wall(lo, hi)[:-2] + [("+x wall, south", box_faces(x1, y0, z0, x1 + WALL, lo, z1, CHECK)),
                                  ("+x wall, north", box_faces(x1, hi, z0, x1 + WALL, y1, z1, CHECK)),
                                  ("room-side cap over the slot", box_faces(x1, lo - 4, z0, x1 + 4, hi + 4, z1, CHECK)),
                                  ("void-side cap over the slot", box_faces(x1 + 12, lo - 4, z0, x1 + WALL, hi + 4, z1, CHECK))],
        False, room_probes() + [{"kind": "solid", "point": [x1 + 2, SLOT_CENTRE, 128], "why": "the room-side cap"},
                                {"kind": "solid", "point": [x1 + 14, SLOT_CENTRE, 128], "why": "the void-side cap"}],
        "A 0.125 slot inside the +x wall, capped on the room side and on the void side: a closed thin pocket nothing can "
        "reach. Sealed; the pocket is not a leak and nothing is asserted about its own air.")
    add("S07.blind", "a 0.125 slot from the room into the +x wall, closed on the void side", d + "s07_blind.map",
        slot_wall(lo, hi)[:-2] + [("+x wall, south", box_faces(x1, y0, z0, x1 + WALL, lo, z1, CHECK)),
                                  ("+x wall, north", box_faces(x1, hi, z0, x1 + WALL, y1, z1, CHECK)),
                                  ("void-side cap over the slot", box_faces(x1 + 12, lo - 4, z0, x1 + WALL, hi + 4, z1, CHECK))],
        False, room_probes() + [{"kind": "open", "point": [x1 + 6, SLOT_CENTRE, 128], "why": "air in the slot, reached from the room"},
                                {"kind": "solid", "point": [x1 + 14, SLOT_CENTRE, 128], "why": "the void-side cap"}],
        "A 0.125 slot from the room 12 units into the +x wall, capped on the void side. Sealed, and the slot's air is "
        "reachable from the room, so it must stay open (before NEW_340A its portal to the room was dropped and the slot "
        "filled as unreachable).")
    # S08: an opening inside the ON_EPSILON band - a known failure that says what it is
    lo, hi = SLOT_CENTRE - 0.03125, SLOT_CENTRE + 0.03125
    add("S08.band.00625", "a 0.0625 slot to the void: inside the ON_EPSILON band", d + "s08_band_00625.map", slot_wall(lo, hi),
        True, [], "A slot 0.0625 wide, off every split. The source leaks. Its two faces lie closer than ON_EPSILON (0.1): "
        "the face tree consumes the second one as ON the first one's plane, so no leaf lies between them. The portal "
        "stage is not involved. Kept as a KNOWN FAILURE naming that band, not rewritten as a seal.",
        mechanism="on_epsilon_band", extra={"leak_route": slot_route(lo, hi)})
    # S11: nonstructural surfaces over a slot do not seal it
    lo, hi = SLOT_CENTRE - 0.0625, SLOT_CENTRE + 0.0625
    add("S11.detail_over", "a 0.125 slot behind a detail brush", d + "s11_detail_over.map",
        slot_wall(lo, hi) + [("detail brush over the slot, void side",
                              box_faces(x1 + WALL, lo - 8, z0, x1 + WALL + 8, hi + 8, z1, (CHECK, (0, 0), (0.5, 0.5), CONTENTS_DETAIL)))],
        True, [], "A 0.125 slot whose outer end is covered by a DETAIL brush. Detail does not seal: the map leaks.",
        extra={"leak_route": slot_route(lo, hi)})
    rows = "\n".join("( %s )" % " ".join("( %s )" % " ".join(num(v) for v in (x1 + WALL + 2, lo - 8 + 8 * c, 128 * r, c * 0.5, r * 0.5))
                                          for r in range(3)) for c in range(3))
    patch = "// a flat patch over the slot's outer end\n{\npatchDef2\n{\n%s\n( 3 3 0 0 0 )\n(\n%s\n)\n}\n}\n" % (PATCH, rows)
    files.update(write_map(d + "s11_patch_over.map", brushes_text(slot_wall(lo, hi)) + patch))
    cases.append(case("S11.patch_over", "a 0.125 slot behind a patch", None, d + "s11_patch_over.map", True, [],
                      "A 0.125 slot whose outer end is covered by a flat patch. A patch does not seal: the map leaks.",
                      extra={"leak_route": slot_route(lo, hi)}))
    # S12: a sealed room in two halves joined by a narrow passage
    px = 64
    named = room_brushes() + [("partition, south of the passage", box_faces(px, y0, z0, px + WALL, lo, z1, CHECK)),
                              ("partition, north of the passage", box_faces(px, hi, z0, px + WALL, y1, z1, CHECK))]
    add("S12.passage", "a sealed room in two halves joined by a 0.125 passage", d + "s12_passage.map", named, False,
        room_probes() + [{"kind": "open", "point": [160, -100, 128], "why": "air in the east half, reached through the passage"},
                         {"kind": "drawn", "point": [x1, -100, 128], "normal": [-1, 0, 0], "why": "+x wall, seen from the east half"},
                         {"kind": "drawn", "point": [px + WALL, -100, 128], "normal": [1, 0, 0], "why": "partition, seen from the east half"},
                         {"kind": "open", "point": [px + 8, SLOT_CENTRE, 128], "why": "air in the passage"},
                         {"kind": "solid", "point": [px + 8, SLOT_CENTRE - 1, 128], "why": "the partition beside the passage"}],
        "A sealed room split by a full partition at x = 64..80 with one full-height passage 0.125 wide at y = 100. The "
        "player and the light are in the west half; the east half is reached through the passage, so its air stays "
        "open, its walls are drawn and NORMAL -vis and -light finish. Before NEW_340A the passage's portals were "
        "dropped and the east half was filled as unreachable.", stages=("bsp", "vis", "light"))
    return files, cases


PATCH = "apreg/patch"
# 3 x 3 control points of a bowed quad over the floor, as patchDef2 lists them, (x y z s t); the middle bulges up
PATCH_POINTS = [[(-128 + 64 * c, -96 + 64 * r, 16 + (32 if r == 1 else 0) + (16 if c == 1 else 0), 0.5 * c, 0.5 * r)
                 for c in range(3)] for r in range(3)]


def r09():
    rows = "\n".join("( %s )" % " ".join("( %s )" % " ".join(num(v) for v in p) for p in row) for row in PATCH_POINTS)
    patch = "// a 3 x 3 quadratic patch, textured apreg/patch with authored st\n{\npatchDef2\n{\n%s\n( 3 3 0 0 0 )\n(\n%s\n)\n}\n}\n" % (PATCH, rows)
    box = ("box beside the patch, classic checker shift 8 4 scale 0.5 1", box_faces(64, -32, 0, 128, 32, 64, ("apreg/checker", (8, 4), (0.5, 1))))
    path = "R09_patch/maps/r09_patch.map"
    files = write_map(path, brushes_text(room_brushes() + [box]) + patch)
    probes = room_probes() + [
        {"kind": "patch", "index": 0, "shader": "textures/" + PATCH, "width": 3, "height": 3,
         # patchDef2 lists the control points column by column (ParsePatch: outer index = width); the BSP stores
         # them row by row, so the BSP's point i*3 + j is the map's list j, point i
         "points": [list(PATCH_POINTS[j][i]) for i in range(3) for j in range(3)],
         "why": "the authored control points and st, unchanged"},
        {"kind": "drawn", "point": [96, 0, 64], "normal": [0, 0, 1], "shader": "textures/apreg/checker",
         "st": classic_st((0, 0, 1), (96, 0, 64), (8, 4), (0.5, 1), (64, 64)), "why": "the box's top, classic projection"},
        {"kind": "drawn", "point": [128, 8, 40], "normal": [1, 0, 0], "shader": "textures/apreg/checker",
         "st": classic_st((1, 0, 0), (128, 8, 40), (8, 4), (0.5, 1), (64, 64)), "why": "the box's +x side"},
        {"kind": "open", "point": [-64, -32, 64], "why": "air above the patch"},
    ]
    return files, [case("R09.patch", "a patch with authored UV checker beside an ordinary brush", None, path, False, probes,
                        "A 3 x 3 patchDef2 bowed over the floor, every control point with its own st, and a box with a "
                        "non-default classic texture shift and scale. The BSP must keep the patch's control points and st "
                        "exactly as authored (the engine tessellates them) and draw the box with the texture coordinates "
                        "Q3Map2's nearest-axial projection gives for that shift and scale.",
                        stages=("bsp", "vis", "light"), extra={"aliases": ["G08"]})]


SIZES = {"apreg/checker": (64, 64), "apreg/wide": (64, 32), "apreg/water": (64, 64), "apreg/clip": (64, 64),
         "common/caulk": (64, 64)}


def r10():
    """The same room in classic, Valve 220 and brush primitives syntax, with the same intended texture mapping."""
    wide = ("apreg/wide", (8, 4), (0.5, 1))
    content = [("textured box, its bottom caulk", box_faces(-128, -32, 0, -64, 32, 64, wide, {"-z": "common/caulk"})),
               ("water box", box_faces(64, -64, 0, 128, 64, 48, "apreg/water")),
               ("player clip box", box_faces(160, -64, 0, 192, 64, 96, "apreg/clip"))]
    files, cases = {}, []
    samples = [((0, 0, 1), (-96, 8, 64)), ((1, 0, 0), (-64, 8, 40)), ((0, -1, 0), (-100, -32, 24))]
    for syntax in ("classic", "valve220", "brushprim"):
        named = room_brushes() + content
        if syntax == "classic":
            body, extra = brushes_text(named), ""
            st = lambda n, p: classic_st(n, p, (8, 4), (0.5, 1), (64, 32))  # noqa: E731
        elif syntax == "valve220":
            body, extra = brushes_text(named, valve220), "\"mapversion\" \"220\"\n"
            st = lambda n, p: classic_st(n, p, (8, 4), (0.5, 1), (64, 32))  # noqa: E731
        else:
            body, extra = brushes_text(named, brushprim, sizes=SIZES), ""
            st = lambda n, p: bp_st(n, p, (8, 4), (0.5, 1), (64, 32))  # noqa: E731
        path = "R10_syntax/maps/r10_%s.map" % syntax
        files.update(write_map(path, body, extra_keys=extra))
        probes = room_probes() + [
            {"kind": "drawn", "point": list(p), "normal": list(n), "shader": "textures/apreg/wide", "st": st(n, p),
             "why": "the textured box, %s" % syntax} for n, p in samples] + [
            {"kind": "not_drawn", "point": [-96, 8, 0], "normal": [0, 0, -1], "why": "the box's caulk bottom: nodraw"},
            {"kind": "solid", "point": [-96, 0, 32], "why": "inside the textured box"},
            {"kind": "contents", "point": [96, 0, 24], "has": ["water"], "lacks": ["solid"], "why": "inside the water box"},
            {"kind": "contents", "point": [176, 0, 48], "has": ["playerclip"], "lacks": ["solid"], "why": "inside the clip box"},
            {"kind": "not_drawn", "point": [176, 0, 96], "normal": [0, 0, 1], "why": "the clip box's top: nodraw"},
        ]
        cases.append(case("R10.%s" % syntax, "%s brush syntax: geometry, texture mapping and material contents" % syntax, None,
                          path, False, probes,
                          "One room, written three ways. Classic and Valve 220 describe the same nearest-axial mapping "
                          "(Valve 220 spells the axes out); brush primitives carry a texture matrix in each face's own basis "
                          "(ComputeAxisBase). Each must compile to the same solids, with the water box CONTENTS_WATER and "
                          "not solid, the clip box player clip and not drawn, the caulk face not drawn, and the box's texture "
                          "coordinates exactly what its syntax describes."))
    return files, cases


def upstream_cases():
    """Upstream's own regression maps (../q3map2): each must compile without a crash. They are visual tests; their
    README says what to look at. Asserting more here would be asserting what one compiler emitted."""
    out = []
    for directory in sorted((HERE.parent / "q3map2").iterdir()):
        for m in sorted((directory / "maps").glob("*.map")):
            out.append({"id": "U.%s" % directory.name, "title": "upstream regression map compiles", "mechanism": None,
                        "map": "../q3map2/%s/maps/%s" % (directory.name, m.name), "gameroot": "../q3map2/%s" % directory.name,
                        "expect": {"leak": None}, "probes": [], "stages": ["bsp"],
                        "explanation": "upstream's %s; compile-only (exit status 0 and a BSP)" % directory.name})
    return out


def wrap(text, width=100):
    lines, line = [], ""
    for word in text.split():
        if line and len(line) + 1 + len(word) > width:
            lines.append(line)
            line = word
        else:
            line = (line + " " + word).strip()
    return lines + ([line] if line else [])


def build():
    files, cases = {}, []
    for part in (r01, r02, r03, pipes, r07, r08, r09, r10, g01_g05, g02, g06, g07, s_cases):
        f, c = part()
        files.update(f)
        cases.extend(c)
    for directory in sorted({c["map"].split("/", 1)[0] for c in cases}):
        lines = ["%s — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)" % directory, ""]
        for c in (c for c in cases if c["map"].startswith(directory + "/")):
            lines += ["%s: %s" % (c["id"], c["title"]), "  map: %s" % c["map"].split("/", 1)[1],
                      "  mechanism: %s" % (c["mechanism"] or "none (control)"),
                      "  the source %s" % ("leaks" if c["expect"]["leak"] else "is sealed"),
                      "  stages: %s" % " + ".join(c["stages"])] + (["  also: %s" % ", ".join(c["aliases"])] if c.get("aliases") else []) + [""]
            lines += ["  " + line for line in wrap(c["explanation"])] + [""]
            for p in c["probes"]:
                lines.append("  - %s %s: %s" % (p["kind"], p.get("point", ""), p["why"]))
            lines.append("")
        files[directory + "/README.txt"] = "\n".join(lines).rstrip() + "\n"
    cases.extend(upstream_cases())
    cases.sort(key=lambda c: (c["id"].startswith("U."), c["id"]))
    document = {"schema": SCHEMA,
                "comment": "Written by generate.py; run by ci/auto-pigeon/regress.py. Known failures: known-failures.json.",
                "gameroot": "gameroot", "pipe_maps": PIPE_MAPS, "cases": cases}
    files["cases.json"] = json.dumps(document, indent=1, sort_keys=True) + "\n"
    return files


def main():
    check = "--check" in sys.argv[1:]
    files = build()
    bad = []
    for rel, text in sorted(files.items()):
        path = HERE / rel
        if check:
            if not path.is_file() or path.read_text(encoding="utf-8") != text:
                bad.append(rel)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8", newline="\n")
    for rel, digest in PIPE_MAPS.items():
        path = HERE / rel
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            bad.append(rel + " (sha256)")
    if bad:
        sys.exit("generate: out of date or altered: " + ", ".join(bad))
    print("generate: %s %d files" % ("checked" if check else "wrote", len(files)))


if __name__ == "__main__":
    main()
