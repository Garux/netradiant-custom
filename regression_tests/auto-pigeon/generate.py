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
  * SplitBrush (`brush.cpp`) discarded a split fragment whose volume is below 1 cubic unit.
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
    """Quake III classic projection: `tex shiftS shiftT rotation scaleS scaleT 0 0 0`."""
    lines = (["// " + comment] if comment else []) + ["{"]
    for f in faces:
        tex, shift, scale = f.tex if isinstance(f.tex, tuple) else (f.tex, (0, 0), (0.5, 0.5))
        lines.append("%s %s %s %s 0 %s %s 0 0 0" % (" ".join(pt(p) for p in f.p), tex, num(shift[0]), num(shift[1]),
                                                    num(scale[0]), num(scale[1])))
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
                          "and both controls stay correct on every build." % (num(past), num(past))))
    return files, cases


PIPE_MAPS = {
    "R04_m4_near_parallel/maps/r04_p08_nearby.map": "28b154f75417475465dab64509cd81c619c5971eb9fe9c3d98b0ff4e2b98895a",
    "R05_p10_shape/maps/r05_p10_base.map": "4cedfb8a128baecdd220b445b58790c77346a5a5f46c752a259bb5027fc81a9f",
    "R06_p08_sealed/maps/r06_p08_base.map": "b585fbde31e97b256393d80e297cb3ca562699a1d7d6acc3302a63652ce9825c",
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
             "P3 (never committed) lost them. Any later M4 repair must keep them drawn."),
        case("R06.p4.p08_base", "control: the sealed P08 pipe that P4 reported as leaking; its M2 face", "M2",
             "R06_p08_sealed/maps/r06_p08_base.map", False,
             drawn(P08_BASE, P08_BASE_FACE, "the wall face y ~ -77 of brush 179 (map line 1620), seen from the room"),
             "The P08 pipe is sealed: the rejected M4 repair P4 (never committed) reported a leak here, so the leak "
             "verdict is the first assertion. The 9 face points are the M2 defect of R03 on a real shape: a wall "
             "face piece crosses a node plane by between 0.1 and 0.2, upstream sends it wholly to the back, the leaf that "
             "then straddles the wall turns opaque, and the face is not drawn there."),
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
                       "every Q3Map2 build measured (upstream, the shipping pin, P1+P2) closes a slot of 0.25 or less "
                       "through an opaque leaf with no brush. Recorded as a KNOWN FAILURE owned by NEW_340 rather than "
                       "rewritten as an expected seal.")
        cases.append(case(cid, title, mechanism, path, leak, [], explanation))
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
                          None, path, False, room_probes() + probes, explanation))
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
                        stages=("bsp", "vis", "light"))]


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
    for part in (r01, r02, r03, pipes, r07, r08, r09, r10):
        f, c = part()
        files.update(f)
        cases.extend(c)
    for directory in sorted({c["map"].split("/", 1)[0] for c in cases}):
        lines = ["%s — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)" % directory, ""]
        for c in (c for c in cases if c["map"].startswith(directory + "/")):
            lines += ["%s: %s" % (c["id"], c["title"]), "  map: %s" % c["map"].split("/", 1)[1],
                      "  mechanism: %s" % (c["mechanism"] or "none (control)"),
                      "  the source %s" % ("leaks" if c["expect"]["leak"] else "is sealed"),
                      "  stages: %s" % " + ".join(c["stages"]), ""]
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
