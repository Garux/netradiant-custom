"""A small reader of Quake III IBSP version 46 files, for the regression assertions. Standard library only.

It answers the four questions a regression case asks of a compiled map, each from the BSP alone:

  leaf_at(point)        the leaf the point falls in, walking the node tree (front when on the plane)
  contents_at(point)    the CONTENTS_* bits of every brush in that leaf that contains the point
                        (within `eps`), plus whether the leaf is opaque (cluster -1). This is what the
                        engine's collision model and the visibility tree see.
  drawn_at(point, n)    a drawn triangle of a planar or triangle-soup surface whose plane is within
                        `max_angle` of `n`, within `plane_tol` of the point, and whose interior covers
                        the point's projection. Patches are reported separately (`patches()`): the BSP
                        stores their control points, the engine tessellates them.
  shader_of(surface)    the shader name and its surface/content flags.

The lump layout is id's `qfiles.h` for BSP_VERSION 46 (17 lumps). Coordinates are map units.
"""

import math
import struct

LUMPS = ("entities", "shaders", "planes", "nodes", "leafs", "leafsurfaces", "leafbrushes", "models", "brushes",
         "brushsides", "drawverts", "drawindexes", "fogs", "surfaces", "lightmaps", "lightgrid", "visibility")
CONTENTS = {"solid": 0x1, "lava": 0x8, "slime": 0x10, "water": 0x20, "fog": 0x40,
            "playerclip": 0x10000, "monsterclip": 0x20000, "detail": 0x8000000, "structural": 0x10000000,
            "trans": 0x20000000}
SURF_NODRAW = 0x80
MST = {1: "planar", 2: "patch", 3: "triangle_soup", 4: "flare"}


class BspError(Exception):
    pass


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


class Ibsp:
    def __init__(self, path):
        with open(path, "rb") as f:
            data = f.read()
        if data[:4] != b"IBSP":
            raise BspError("%s is not an IBSP file" % path)
        version = struct.unpack_from("<i", data, 4)[0]
        if version != 46:
            raise BspError("%s is IBSP version %d, not 46" % (path, version))
        self.lumps = {}
        for i, name in enumerate(LUMPS):
            offset, length = struct.unpack_from("<ii", data, 8 + 8 * i)
            self.lumps[name] = data[offset:offset + length]
        self.entities = self.lumps["entities"].rstrip(b"\0").decode("latin-1")
        self.shaders = [(raw[:64].split(b"\0", 1)[0].decode("latin-1"),) + struct.unpack_from("<ii", raw, 64)
                        for raw in self._records("shaders", 72)]
        self.planes = [struct.unpack("<4f", raw) for raw in self._records("planes", 16)]
        self.nodes = [struct.unpack("<3i6i", raw) for raw in self._records("nodes", 36)]
        self.leafs = [struct.unpack("<2i6i4i", raw) for raw in self._records("leafs", 48)]
        self.leafbrushes = list(struct.unpack("<%di" % (len(self.lumps["leafbrushes"]) // 4), self.lumps["leafbrushes"]))
        self.models = [struct.unpack("<6f4i", raw) for raw in self._records("models", 40)]
        self.brushes = [struct.unpack("<3i", raw) for raw in self._records("brushes", 12)]
        self.brushsides = [struct.unpack("<2i", raw) for raw in self._records("brushsides", 8)]
        self.verts = [struct.unpack("<3f2f2f3f4B", raw) for raw in self._records("drawverts", 44)]
        self.indexes = list(struct.unpack("<%di" % (len(self.lumps["drawindexes"]) // 4), self.lumps["drawindexes"]))
        self.surfaces = [struct.unpack("<12i3f9f2i", raw) for raw in self._records("surfaces", 104)]

    def _records(self, lump, size):
        raw = self.lumps[lump]
        if len(raw) % size:
            raise BspError("lump %s is %d bytes, not a multiple of %d" % (lump, len(raw), size))
        return [raw[i:i + size] for i in range(0, len(raw), size)]

    # ------------------------------------------------------------------ point queries

    def leaf_at(self, point):
        node = 0
        while node >= 0:
            plane_index, front, back = self.nodes[node][:3]
            nx, ny, nz, dist = self.planes[plane_index]
            node = front if nx * point[0] + ny * point[1] + nz * point[2] - dist >= 0 else back
        return -node - 1

    def brush_contains(self, brush_index, point, eps=0.001):
        first, count, _ = self.brushes[brush_index]
        for side in range(first, first + count):
            nx, ny, nz, dist = self.planes[self.brushsides[side][0]]
            if nx * point[0] + ny * point[1] + nz * point[2] - dist > eps:
                return False
        return True

    def contents_at(self, point, eps=0.001):
        """{'leaf', 'cluster', 'opaque', 'contents' (OR of every containing brush), 'brushes' [indexes]}."""
        leaf = self.leaf_at(point)
        cluster, _area, _a, _b, _c, _d, _e, _f, _fs, _ns, first_brush, num_brushes = self.leafs[leaf]
        contents, inside = 0, []
        for i in range(first_brush, first_brush + num_brushes):
            brush = self.leafbrushes[i]
            if self.brush_contains(brush, point, eps):
                inside.append(brush)
                contents |= self.shaders[self.brushes[brush][2]][2]
        return {"leaf": leaf, "cluster": cluster, "opaque": cluster == -1, "contents": contents, "brushes": inside}

    # ------------------------------------------------------------------ drawn surfaces

    def world_surfaces(self):
        first, count = self.models[0][6], self.models[0][7]
        return range(first, first + count)

    def triangles(self, surface_index):
        s = self.surfaces[surface_index]
        kind, first_vert, first_index, num_indexes = s[2], s[3], s[5], s[6]
        if MST.get(kind) not in ("planar", "triangle_soup"):
            return
        for i in range(first_index, first_index + num_indexes - 2, 3):
            yield tuple(self.verts[first_vert + self.indexes[i + k]] for k in range(3))

    def drawn_at(self, point, normal, plane_tol=0.2, max_angle=5.0, edge_tol=0.01):
        """The first (surface index, shader name, st at the point) of a drawn triangle covering the point, else None."""
        cos_limit = math.cos(math.radians(max_angle))
        for si in self.world_surfaces():
            shader, surface_flags, _ = self.shaders[self.surfaces[si][0]]
            if surface_flags & SURF_NODRAW:
                continue
            for tri in self.triangles(si):
                a, b, c = (v[0:3] for v in tri)
                n = _cross(_sub(b, a), _sub(c, a))
                length = math.sqrt(_dot(n, n))
                if length < 1e-9:
                    continue
                # Quake III winds a front face clockwise, so the cross product's sign is not the facing; the
                # vertex normals the compiler wrote are
                facing = tuple(sum(v[7 + k] for v in tri) for k in range(3))
                sign = 1.0 if _dot(n, facing) >= 0 else -1.0
                n = (sign * n[0] / length, sign * n[1] / length, sign * n[2] / length)
                if _dot(n, normal) < cos_limit or abs(_dot(_sub(point, a), n)) > plane_tol:
                    continue
                weights = self._barycentric(point, a, b, c, n)
                if weights and min(weights) >= -edge_tol:
                    st = tuple(sum(w * v[3 + k] for w, v in zip(weights, tri)) for k in range(2))
                    return {"surface": si, "shader": shader, "st": st, "type": MST[self.surfaces[si][2]]}
        return None

    @staticmethod
    def _barycentric(p, a, b, c, n):
        area = _dot(_cross(_sub(b, a), _sub(c, a)), n)
        if abs(area) < 1e-12:
            return None
        wa = _dot(_cross(_sub(b, p), _sub(c, p)), n) / area
        wb = _dot(_cross(_sub(c, p), _sub(a, p)), n) / area
        return (wa, wb, 1.0 - wa - wb)

    def patches(self):
        """Every world patch: {'surface', 'shader', 'width', 'height', 'points' [(xyz, st)]} in BSP order."""
        out = []
        for si in self.world_surfaces():
            s = self.surfaces[si]
            if MST.get(s[2]) != "patch":
                continue
            first_vert, num_verts = s[3], s[4]
            width, height = s[24], s[25]
            out.append({"surface": si, "shader": self.shaders[s[0]][0], "width": width, "height": height,
                        "points": [(v[0:3], v[3:5]) for v in self.verts[first_vert:first_vert + num_verts]]})
        return out

    def surface_counts(self):
        counts = {}
        for si in self.world_surfaces():
            kind = MST.get(self.surfaces[si][2], "other")
            counts[kind] = counts.get(kind, 0) + 1
        return counts
