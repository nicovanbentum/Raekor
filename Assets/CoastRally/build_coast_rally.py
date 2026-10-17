import bpy
import bmesh
import math
import random
import sys
from pathlib import Path
from mathutils import Matrix, Euler, Vector

import numpy as np


GRID = 32
SWATCH = 32
SIZE = GRID * SWATCH
SHADES = [0.62, 0.74, 0.86, 1.0, 1.1, 1.2, 1.32, 1.46]

FAMILIES = [
    ("Grass",      (0.24, 0.46, 0.13), 0.0),
    ("GrassDry",   (0.50, 0.52, 0.20), 0.0),
    ("Sand",       (0.84, 0.74, 0.50), 0.0),
    ("Rock",       (0.40, 0.38, 0.36), 0.0),
    ("Dirt",       (0.40, 0.29, 0.18), 0.0),
    ("Asphalt",    (0.13, 0.13, 0.14), 0.0),
    ("Paint",      (0.86, 0.86, 0.82), 0.0),
    ("Yellow",     (0.95, 0.72, 0.08), 0.0),
    ("Red",        (0.76, 0.10, 0.08), 0.0),
    ("CarBlue",    (0.05, 0.20, 0.68), 0.0),
    ("White",      (0.86, 0.86, 0.86), 0.0),
    ("Rubber",     (0.04, 0.04, 0.045), 0.0),
    ("Steel",      (0.55, 0.56, 0.60), 0.0),
    ("Glass",      (0.05, 0.08, 0.12), 0.0),
    ("Bark",       (0.30, 0.20, 0.12), 0.0),
    ("Leaf",       (0.18, 0.42, 0.12), 0.0),
    ("Pine",       (0.09, 0.28, 0.13), 0.0),
    ("Palm",       (0.33, 0.56, 0.16), 0.0),
    ("Wood",       (0.45, 0.30, 0.16), 0.0),
    ("Cream",      (0.85, 0.78, 0.62), 0.0),
    ("Roof",       (0.60, 0.17, 0.11), 0.0),
    ("Teal",       (0.17, 0.48, 0.50), 0.0),
    ("Orange",     (0.95, 0.36, 0.04), 0.0),
    ("Blue",       (0.10, 0.28, 0.72), 0.0),
    ("Hay",        (0.82, 0.66, 0.30), 0.0),
    ("Concrete",   (0.56, 0.55, 0.53), 0.0),
    ("Iron",       (0.12, 0.12, 0.13), 0.0),
    ("Flower",     (0.90, 0.28, 0.52), 0.0),
    ("GlowWhite",  (1.00, 0.95, 0.85), 1.0),
    ("GlowRed",    (1.00, 0.12, 0.08), 0.3),
    ("GlowAmber",  (1.00, 0.62, 0.18), 0.6),
    ("GlowWindow", (1.00, 0.85, 0.55), 0.1),
]

FAMILY_INDEX = {name: index for index, (name, _, _) in enumerate(FAMILIES)}

TO_BLENDER = Matrix.Rotation(math.pi * 0.5, 4, "X")

ROAD_HALF = 5.0
ROAD_SPACING = 2.0
WAYPOINT_STRIDE = 4
SKIRT_DEPTH = 0.7
TERRAIN_SINK = 0.3
EDGE_SINK = 0.06

ROAD_POINTS = [
    (-10, -150), (50, -152), (105, -130), (140, -85), (150, -25), (168, 35), (150, 95), (100, 135), (40, 140),
    (-15, 112), (-55, 135), (-105, 110), (-150, 60), (-145, 0), (-118, -45), (-140, -95), (-85, -140),
]

PAD_X, PAD_Z = 35.0, -45.0
PAD_HX, PAD_HZ = 40.0, 26.0

TERRAIN_HALF = 260.0
TERRAIN_CELL = 2.5

HILL_RNG = np.random.default_rng(11)
HILL_WAVES = [(HILL_RNG.uniform(0.8, 2.0), 2.0 * math.pi / HILL_RNG.uniform(70.0, 190.0), HILL_RNG.uniform(0.0, math.pi), HILL_RNG.uniform(0.0, math.tau)) for _ in range(6)]
PATCH_WAVES = [(1.0, 2.0 * math.pi / HILL_RNG.uniform(25.0, 60.0), HILL_RNG.uniform(0.0, math.pi), HILL_RNG.uniform(0.0, math.tau)) for _ in range(4)]

MOUNTAINS = [(-70.0, 55.0, 30.0, 48.0), (115.0, 55.0, 12.0, 38.0), (-40.0, -85.0, 7.0, 35.0)]


def to_srgb(value):
    return value * 12.92 if value <= 0.0031308 else 1.055 * pow(value, 1.0 / 2.4) - 0.055


def write_palettes(directory):
    albedo = [0.0] * (SIZE * SIZE * 4)
    emissive = [0.0] * (SIZE * SIZE * 4)

    for index in range(SIZE * SIZE):
        albedo[index * 4 + 3] = 1.0
        emissive[index * 4 + 3] = 1.0

    for row, (name, color, glow) in enumerate(FAMILIES):
        for column, shade in enumerate(SHADES):
            factor = 0.75 + 0.25 * shade if glow else shade
            linear = [min(channel * factor, 1.0) for channel in color]
            srgb = [to_srgb(channel) for channel in linear]
            emit = [channel * glow for channel in linear]

            for y in range(row * SWATCH, (row + 1) * SWATCH):
                image_row = SIZE - 1 - y
                for x in range(column * SWATCH, (column + 1) * SWATCH):
                    index = (image_row * SIZE + x) * 4
                    albedo[index:index + 3] = srgb
                    emissive[index:index + 3] = emit

    for name, pixels in (("Palette", albedo), ("PaletteEmissive", emissive)):
        image = bpy.data.images.new(name, SIZE, SIZE, alpha=True)
        image.colorspace_settings.name = "Non-Color"
        image.pixels.foreach_set(pixels)
        image.filepath_raw = str(directory / f"{name}.png")
        image.file_format = "PNG"
        image.save()


class Part:
    def __init__(self, name, joint, rng):
        self.name = name
        self.joint = Vector(joint)
        self.rng = rng
        self.bm = bmesh.new()
        self.family = self.bm.faces.layers.int.new("family")
        self.shade = self.bm.faces.layers.int.new("shade")
        self.recalc_faces = []

    def tag(self, verts, family, shade, variation):
        faces = dict.fromkeys(face for vert in verts for face in vert.link_faces)
        for face in faces:
            face[self.family] = FAMILY_INDEX[family]
            value = shade + (self.rng.randint(-variation, variation) if variation else 0)
            face[self.shade] = max(0, min(len(SHADES) - 1, value))
        return verts

    def face(self, coords, family, shade, facing):
        verts = [self.bm.verts.new(coord) for coord in coords]
        face = self.bm.faces.new(verts)
        face.normal_update()
        if face.normal.dot(Vector(facing)) < 0.0:
            face.normal_flip()
        face[self.family] = FAMILY_INDEX[family]
        face[self.shade] = max(0, min(len(SHADES) - 1, shade))
        return face


def matrix(loc, rot, scale):
    return Matrix.LocRotScale(Vector(loc), Euler(rot), Vector(scale))


AXIS_ROTATIONS = {"x": (0.0, math.pi * 0.5, 0.0), "y": (-math.pi * 0.5, 0.0, 0.0), "z": (0.0, 0.0, 0.0)}


def box(p, family, loc, size, rot=(0, 0, 0), shade=3, var=1):
    result = bmesh.ops.create_cube(p.bm, size=1.0, matrix=matrix(loc, rot, size))
    return p.tag(result["verts"], family, shade, var)


def cyl(p, family, loc, r1, r2, depth, seg=8, axis="y", rot=None, scale=(1, 1, 1), shade=3, var=1):
    transform = matrix(loc, rot if rot is not None else (0, 0, 0), (1, 1, 1)) @ Matrix.LocRotScale(Vector(), Euler(AXIS_ROTATIONS[axis]), Vector(scale))
    result = bmesh.ops.create_cone(p.bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=depth, matrix=transform)
    return p.tag(result["verts"], family, shade, var)


def ico(p, family, loc, radius, scale=(1, 1, 1), rot=(0, 0, 0), jitter=0.0, flatten_below=None, seed=0, subdiv=1, shade=3, var=1):
    result = bmesh.ops.create_icosphere(p.bm, subdivisions=subdiv, radius=radius, matrix=Matrix.Identity(4))
    verts = result["verts"]
    rng = random.Random(seed)
    for vert in verts:
        vert.co *= 1.0 + rng.uniform(-jitter, jitter)
    bmesh.ops.transform(p.bm, matrix=matrix(loc, rot, scale), verts=verts)
    if flatten_below is not None:
        for vert in verts:
            vert.co.y = max(vert.co.y, flatten_below)
    return p.tag(verts, family, shade, var)


def prism(p, family, outline, axis, start, end, shade=3, var=1):
    def point(a, b, c):
        if axis == "x":
            return Vector((c, b, a))
        if axis == "z":
            return Vector((a, b, c))
        return Vector((a, c, b))

    count = len(outline)
    ca = sum(a for a, _ in outline) / count
    cb = sum(b for _, b in outline) / count
    near = [p.bm.verts.new(point(a, b, start)) for a, b in outline]
    far = [p.bm.verts.new(point(a, b, end)) for a, b in outline]
    near_center = p.bm.verts.new(point(ca, cb, start))
    far_center = p.bm.verts.new(point(ca, cb, end))
    for i in range(count):
        j = (i + 1) % count
        p.recalc_faces.append(p.bm.faces.new((near[i], near[j], near_center)))
        p.recalc_faces.append(p.bm.faces.new((far[j], far[i], far_center)))
        p.recalc_faces.append(p.bm.faces.new((near[j], near[i], far[i], far[j])))
    return p.tag(near + far + [near_center, far_center], family, shade, var)


def assign_uv(face, uv_layer, family, shade):
    normal = face.normal
    if normal.length < 1e-6:
        normal = Vector((0, 0, 1))
    tangent = normal.orthogonal().normalized()
    bitangent = normal.cross(tangent)
    center = face.calc_center_median()
    coords = [((loop.vert.co - center).dot(tangent), (loop.vert.co - center).dot(bitangent)) for loop in face.loops]
    extent = max(max(abs(x), abs(y)) for x, y in coords) or 1e-6
    scale = (SWATCH * 0.3 / SIZE) / extent
    u0 = (shade + 0.5) / GRID
    v0 = (family + 0.5) / GRID
    for loop, (x, y) in zip(face.loops, coords):
        loop[uv_layer].uv = (u0 + x * scale, v0 + y * scale)


def build_asset(name, parts, offset, material):
    empty = bpy.data.objects.new(name, None)
    empty.location = offset
    bpy.context.scene.collection.objects.link(empty)

    for part in parts:
        bm = part.bm
        if part.recalc_faces:
            bmesh.ops.recalc_face_normals(bm, faces=[face for face in part.recalc_faces if face.is_valid])

        joint = TO_BLENDER @ part.joint
        if joint.length < 1e-4:
            joint = Vector((0.0, 0.0, 0.001))

        bmesh.ops.transform(bm, matrix=TO_BLENDER, verts=bm.verts)
        bmesh.ops.translate(bm, vec=-joint, verts=bm.verts)
        bm.normal_update()

        uv_layer = bm.loops.layers.uv.new("UVMap")
        for face in bm.faces:
            assign_uv(face, uv_layer, face[part.family], face[part.shade])

        mesh = bpy.data.meshes.new(f"{name}_{part.name}")
        bm.to_mesh(mesh)
        bm.free()

        for polygon in mesh.polygons:
            polygon.use_smooth = False

        mesh.materials.append(material)

        obj = bpy.data.objects.new(f"{name}_{part.name}", mesh)
        obj.location = joint
        obj.parent = empty
        bpy.context.scene.collection.objects.link(obj)


def single(rng, function, *args):
    main = Part("Main", (0, 0, 0), rng)
    function(main, rng, *args)
    return [main]


def with_collision(rng, function, collider):
    main = Part("Main", (0, 0, 0), rng)
    collision = Part("Collision", (0, 0, 0), rng)
    function(main, rng)
    collider(collision)
    return [main, collision]


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def waves(x, z, table):
    h = np.zeros_like(x)
    for amp, freq, angle, phase in table:
        h += amp * np.sin((x * math.cos(angle) + z * math.sin(angle)) * freq + phase)
    return h


def coast_distance(x, z):
    theta = np.arctan2(z, x)
    coast = 210.0 + 14.0 * np.sin(3.0 * theta + 0.7) + 9.0 * np.sin(5.0 * theta + 2.1) + 5.0 * np.sin(9.0 * theta + 0.3)
    return np.hypot(x, z) / coast


def base_height(x, z):
    d = coast_distance(x, z)
    h = 6.0 + waves(x, z, HILL_WAVES)
    for mx, mz, height, radius in MOUNTAINS:
        h += height * np.exp(-((x - mx) ** 2 + (z - mz) ** 2) / (radius * radius))
    land = 1.0 - smoothstep(0.78, 1.0, d)
    return h * land - 10.0 * smoothstep(0.93, 1.15, d) + 0.4, d


def catmull_rom_loop(points, steps=48):
    out = []
    n = len(points)
    for i in range(n):
        p0, p1, p2, p3 = (np.array(points[(i + k) % n], dtype=np.float64) for k in (-1, 0, 1, 2))
        for s in range(steps):
            t = s / steps
            out.append(0.5 * (2.0 * p1 + (p2 - p0) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t + (3.0 * p1 - p0 - 3.0 * p2 + p3) * t * t * t))
    return np.array(out)


def resample_loop(points, spacing):
    closed = np.vstack([points, points[:1]])
    lengths = np.linalg.norm(np.diff(closed, axis=0), axis=1)
    cumulative = np.concatenate([[0.0], np.cumsum(lengths)])
    count = int(round(cumulative[-1] / spacing))
    s = np.arange(count) * (cumulative[-1] / count)
    return np.stack([np.interp(s, cumulative, closed[:, 0]), np.interp(s, cumulative, closed[:, 1])], axis=1)


def smooth_loop(values, window):
    kernel = np.ones(window) / window
    padded = np.concatenate([values[-window:], values, values[:window]])
    return np.convolve(padded, kernel, mode="same")[window:-window]


class Road:
    def __init__(self):
        self.xz = resample_loop(catmull_rom_loop(ROAD_POINTS), ROAD_SPACING)
        self.count = len(self.xz)
        forward = np.roll(self.xz, -1, axis=0) - np.roll(self.xz, 1, axis=0)
        self.tangent = forward / np.linalg.norm(forward, axis=1, keepdims=True)
        self.left = np.stack([self.tangent[:, 1], -self.tangent[:, 0]], axis=1)
        self.yaw = np.arctan2(self.tangent[:, 0], self.tangent[:, 1])

        turn = np.angle(np.exp(1j * (np.roll(self.yaw, -1) - np.roll(self.yaw, 1))))
        self.curvature = smooth_loop(turn / (2.0 * ROAD_SPACING), 5)

        heights, _ = base_height(self.xz[:, 0], self.xz[:, 1])
        heights = np.maximum(heights, 1.8)
        for _ in range(4):
            heights = smooth_loop(heights, 31)
        self.height = np.maximum(heights, 1.8)

        grades = np.abs(np.roll(self.height, -1) - self.height) / ROAD_SPACING
        print(f"road: {self.count} samples, {self.count * ROAD_SPACING:.0f} m, max grade {grades.max() * 100:.1f}%, min radius {1.0 / max(np.abs(self.curvature).max(), 1e-6):.1f} m")

    def nearest(self, x, z):
        x = np.asarray(x, dtype=np.float64).ravel()
        z = np.asarray(z, dtype=np.float64).ravel()
        distance = np.empty(len(x))
        index = np.empty(len(x), dtype=np.int64)
        for start in range(0, len(x), 4096):
            dx = x[start:start + 4096, None] - self.xz[None, :, 0]
            dz = z[start:start + 4096, None] - self.xz[None, :, 1]
            squared = dx * dx + dz * dz
            index[start:start + 4096] = np.argmin(squared, axis=1)
            distance[start:start + 4096] = np.sqrt(squared[np.arange(len(squared)), index[start:start + 4096]])
        return distance, index

    def point(self, index, side=0.0, height=0.0):
        index %= self.count
        xz = self.xz[index] + self.left[index] * side
        return (float(xz[0]), float(self.height[index] + height), float(xz[1]))


ROAD = None
PAD_HEIGHT = 0.0


def pad_distance(x, z):
    return np.maximum(np.abs(x - PAD_X) - PAD_HX, np.abs(z - PAD_Z) - PAD_HZ)


def edge_sink(inside_distance):
    return EDGE_SINK + (TERRAIN_SINK - EDGE_SINK) * smoothstep(0.5, 3.0, inside_distance)


def final_height(x, z):
    x = np.asarray(x, dtype=np.float64)
    z = np.asarray(z, dtype=np.float64)
    h, d = base_height(x, z)
    pad = pad_distance(x, z)
    h = h + (PAD_HEIGHT - edge_sink(-pad) - h) * (1.0 - smoothstep(0.0, 20.0, pad))
    distance, index = ROAD.nearest(x, z)
    distance = distance.reshape(x.shape)
    road_height = ROAD.height[index].reshape(x.shape) - edge_sink(ROAD_HALF - distance)
    h = h + (road_height - h) * (1.0 - smoothstep(ROAD_HALF + 0.4, ROAD_HALF + 16.0, distance))
    return h, d, distance


def terrain(rng):
    p = Part("Main", (0, 0, 0), rng)
    cells = int(TERRAIN_HALF * 2.0 / TERRAIN_CELL)
    axis = np.linspace(-TERRAIN_HALF, TERRAIN_HALF, cells + 1)
    gx, gz = np.meshgrid(axis, axis)
    heights, _, _ = final_height(gx, gz)
    patches = waves(gx, gz, PATCH_WAVES)

    verts = [[p.bm.verts.new((float(gx[j, i]), float(heights[j, i]), float(gz[j, i]))) for i in range(cells + 1)] for j in range(cells + 1)]

    cx = (gx[:-1, :-1] + gx[1:, 1:]) * 0.5
    cz = (gz[:-1, :-1] + gz[1:, 1:]) * 0.5
    ch, cd, croad = final_height(cx, cz)
    cpad = pad_distance(cx, cz)
    cpatch = (patches[:-1, :-1] + patches[1:, 1:]) * 0.5
    slope_x = (heights[:-1, 1:] - heights[:-1, :-1] + heights[1:, 1:] - heights[1:, :-1]) * 0.5 / TERRAIN_CELL
    slope_z = (heights[1:, :-1] - heights[:-1, :-1] + heights[1:, 1:] - heights[:-1, 1:]) * 0.5 / TERRAIN_CELL
    slope = np.hypot(slope_x, slope_z)

    for j in range(cells):
        for i in range(cells):
            h = ch[j, i]
            if h < -0.4:
                family, shade = "Sand", rng.randint(0, 2)
            elif h < 2.3 and cd[j, i] > 0.72:
                family, shade = "Sand", rng.randint(2, 5)
            elif slope[j, i] > 0.55 or h > 24.0:
                family, shade = "Rock", rng.randint(1, 5)
            elif croad[j, i] < ROAD_HALF + 2.5 or cpad[j, i] < 2.0:
                family, shade = "Dirt", rng.randint(2, 4)
            elif cpatch[j, i] > 1.2:
                family, shade = "GrassDry", rng.randint(2, 5)
            else:
                family, shade = "Grass", rng.randint(1, 5)

            a, b, c, d = verts[j][i], verts[j + 1][i], verts[j + 1][i + 1], verts[j][i + 1]
            for tri in (((a, b, c), (a, c, d)) if (i + j) % 2 == 0 else ((a, b, d), (b, c, d))):
                face = p.bm.faces.new(tri)
                face[p.family] = FAMILY_INDEX[family]
                face[p.shade] = shade
    return [p]


def water(rng):
    p = Part("Main", (0, 0, 0), rng)
    half = 1200.0
    p.face([(-half, 0.0, -half), (-half, 0.0, half), (half, 0.0, half), (half, 0.0, -half)], "Blue", 3, (0, 1, 0))
    return [p]


def road_strip(p, i0, i1, inner, outer, lift, family, shade):
    a = ROAD.point(i0, inner, lift)
    b = ROAD.point(i1, inner, lift)
    c = ROAD.point(i1, outer, lift)
    d = ROAD.point(i0, outer, lift)
    p.face([a, b, c, d], family, shade, (0, 1, 0))


def road(rng):
    p = Part("Main", (0, 0, 0), rng)
    curb = np.abs(ROAD.curvature) > 1.0 / 55.0

    for i in range(ROAD.count):
        j = (i + 1) % ROAD.count
        road_strip(p, i, j, -ROAD_HALF, ROAD_HALF, 0.0, "Asphalt", rng.randint(2, 4))

        for side in (-1.0, 1.0):
            road_strip(p, i, j, side * (ROAD_HALF - 0.55), side * (ROAD_HALF - 0.35), 0.012, "Paint", 4)

            edge = ROAD_HALF + (0.9 if curb[i] and curb[j] else 0.0)
            top_a, top_b = ROAD.point(i, side * edge, 0.0), ROAD.point(j, side * edge, 0.0)
            low_a, low_b = ROAD.point(i, side * edge, -SKIRT_DEPTH), ROAD.point(j, side * edge, -SKIRT_DEPTH)
            outward = (float(ROAD.left[i][0] * side), 0.0, float(ROAD.left[i][1] * side))
            p.face([top_a, top_b, low_b, low_a], "Dirt", 2, outward)

            if curb[i] and curb[j]:
                road_strip(p, i, j, side * ROAD_HALF, side * (ROAD_HALF + 0.9), 0.0, "Red" if i % 2 == 0 else "White", 3)

        if (i // 2) % 3 == 0:
            road_strip(p, i, j, -0.1, 0.1, 0.012, "Paint", 4)

    for row in range(2):
        for index in range(10):
            inner = -ROAD_HALF + index
            road_strip(p, row, row + 1, inner, inner + 1.0, 0.02, "White" if (index + row) % 2 == 0 else "Rubber", 4 if (index + row) % 2 == 0 else 2)
    return [p]


def pad(rng):
    p = Part("Main", (0, 0, 0), rng)
    y = PAD_HEIGHT
    x0, x1, z0, z1 = PAD_X - PAD_HX, PAD_X + PAD_HX, PAD_Z - PAD_HZ, PAD_Z + PAD_HZ
    step = 4.0
    xs = np.arange(x0, x1 + 1e-3, step)
    zs = np.arange(z0, z1 + 1e-3, step)

    for i in range(len(xs) - 1):
        for j in range(len(zs) - 1):
            p.face([(xs[i], y, zs[j]), (xs[i], y, zs[j + 1]), (xs[i + 1], y, zs[j + 1]), (xs[i + 1], y, zs[j])], "Concrete", rng.randint(1, 3), (0, 1, 0))

    corners = [(x0, z0), (x0, z1), (x1, z1), (x1, z0)]
    for index in range(4):
        (ax, az), (bx, bz) = corners[index], corners[(index + 1) % 4]
        outward = (bz - az, 0.0, ax - bx)
        center = Vector(((ax + bx) * 0.5 - PAD_X, 0.0, (az + bz) * 0.5 - PAD_Z))
        if Vector(outward).dot(center) < 0.0:
            outward = (-outward[0], 0.0, -outward[2])
        p.face([(ax, y, az), (bx, y, bz), (bx, y - SKIRT_DEPTH, bz), (ax, y - SKIRT_DEPTH, az)], "Concrete", 0, outward)

        inset = 1.2
        dx, dz = bx - ax, bz - az
        length = math.hypot(dx, dz)
        nx, nz = -outward[0] / max(math.hypot(outward[0], outward[2]), 1e-6), -outward[2] / max(math.hypot(outward[0], outward[2]), 1e-6)
        a0 = (ax + nx * inset, az + nz * inset)
        b0 = (bx + nx * inset, bz + nz * inset)
        a1 = (a0[0] + nx * 0.25, a0[1] + nz * 0.25)
        b1 = (b0[0] + nx * 0.25, b0[1] + nz * 0.25)
        p.face([(a0[0], y + 0.012, a0[1]), (b0[0], y + 0.012, b0[1]), (b1[0], y + 0.012, b1[1]), (a1[0], y + 0.012, a1[1])], "Paint", 4, (0, 1, 0))

    ring_x, ring_z, radius = PAD_X + 24.0, PAD_Z - 14.0, 8.0
    segments = 40
    for index in range(segments):
        a0 = index / segments * math.tau
        a1 = (index + 1) / segments * math.tau
        quad = []
        for angle, r in ((a0, radius), (a1, radius), (a1, radius + 0.4), (a0, radius + 0.4)):
            quad.append((ring_x + math.cos(angle) * r, y + 0.012, ring_z + math.sin(angle) * r))
        p.face(quad, "Paint", 4, (0, 1, 0))
    return [p]


def car(rng):
    body = Part("Body", (0, 0, 0), rng)
    brake = Part("Brake", (0, 0, 0), rng)

    box(body, "Rubber", (0, -0.30, 0.0), (1.74, 0.16, 3.7), shade=2, var=0)
    box(body, "CarBlue", (0, -0.03, 0.0), (1.82, 0.44, 4.0), var=0)
    box(body, "CarBlue", (0, 0.22, 1.32), (1.72, 0.1, 1.3), rot=(0.07, 0, 0), shade=4, var=0)
    box(body, "CarBlue", (0, 0.2, -1.72), (1.72, 0.08, 0.56), shade=4, var=0)

    prism(body, "Glass", [(-1.42, 0.2), (0.98, 0.2), (0.32, 0.8), (-1.2, 0.82)], "x", -0.76, 0.76, var=0)
    box(body, "CarBlue", (0, 0.83, -0.44), (1.46, 0.05, 1.5), shade=4, var=0)
    for side in (-1, 1):
        box(body, "CarBlue", (side * 0.755, 0.5, -0.32), (0.04, 0.58, 0.14), var=0)
        box(body, "CarBlue", (side * 0.75, 0.5, 0.66), (0.05, 0.6, 0.08), rot=(-0.72, 0, 0), var=0)
        box(body, "CarBlue", (side * 0.74, 0.5, -1.33), (0.05, 0.62, 0.1), rot=(0.17, 0, 0), var=0)

    for x in (-0.24, 0.24):
        box(body, "White", (x, 0.28, 1.32), (0.16, 0.11, 1.3), rot=(0.07, 0, 0), shade=5, var=0)
        box(body, "White", (x, 0.86, -0.44), (0.16, 0.02, 1.5), shade=5, var=0)
        box(body, "White", (x, 0.24, -1.72), (0.16, 0.02, 0.56), shade=5, var=0)

    for side in (-1, 1):
        for z in (1.3, -1.28):
            box(body, "CarBlue", (side * 0.99, 0.0, z), (0.2, 0.2, 1.0), shade=2, var=0)
            box(body, "Rubber", (side * 1.0, 0.11, z), (0.22, 0.06, 1.04), var=0)
        cyl(body, "White", (side * 0.915, -0.02, -0.06), 0.22, 0.22, 0.02, seg=14, axis="x", shade=5, var=0)
        box(body, "Rubber", (side * 0.93, -0.02, -0.06), (0.02, 0.24, 0.07), var=0)
        box(body, "Rubber", (side * 0.93, 0.08, -0.03), (0.02, 0.06, 0.07), rot=(0.6, 0, 0), var=0)
        box(body, "Yellow", (side * 0.92, -0.2, 0.0), (0.02, 0.05, 1.6), var=0)
        box(body, "CarBlue", (side * 0.95, 0.4, 0.72), (0.14, 0.09, 0.16), shade=2, var=0)
        box(body, "Red", (side * 0.86, -0.38, -1.8), (0.24, 0.28, 0.03), var=0)

    box(body, "Rubber", (0, -0.24, 2.03), (1.84, 0.22, 0.16), var=0)
    box(body, "Iron", (0, 0.03, 2.01), (0.8, 0.16, 0.04), var=0)
    box(body, "Rubber", (0, -0.24, -2.03), (1.84, 0.22, 0.16), var=0)
    box(body, "Iron", (0, 0.08, -2.01), (0.7, 0.14, 0.03), var=0)
    cyl(body, "Steel", (0.55, -0.3, -2.06), 0.05, 0.05, 0.2, seg=8, axis="z", shade=5, var=0)

    for side in (-1, 1):
        box(body, "GlowWhite", (side * 0.6, 0.06, 2.0), (0.38, 0.13, 0.06), var=0)
        box(brake, "GlowRed", (side * 0.66, 0.08, -2.0), (0.34, 0.12, 0.06), var=0)

    box(body, "Iron", (0, 0.34, 1.98), (1.1, 0.06, 0.08), var=0)
    for x in (-0.42, -0.14, 0.14, 0.42):
        cyl(body, "Iron", (x, 0.44, 2.0), 0.1, 0.1, 0.08, seg=10, axis="z", var=0)
        cyl(body, "GlowWhite", (x, 0.44, 2.045), 0.08, 0.08, 0.02, seg=10, axis="z", var=0)

    box(body, "CarBlue", (0, 0.98, -1.62), (1.62, 0.04, 0.34), shade=2, var=0)
    for side in (-1, 1):
        box(body, "CarBlue", (side * 0.78, 0.92, -1.62), (0.04, 0.14, 0.4), shade=2, var=0)
        box(body, "Iron", (side * 0.4, 0.9, -1.56), (0.05, 0.14, 0.08), var=0)

    return [body, brake]


def wheel(p, rng):
    cyl(p, "Rubber", (0, 0, 0), 0.36, 0.36, 0.26, seg=16, axis="x", var=0)
    cyl(p, "Rubber", (0, 0, 0), 0.33, 0.33, 0.28, seg=16, axis="x", shade=1, var=0)
    cyl(p, "White", (0.02, 0, 0), 0.23, 0.23, 0.25, seg=14, axis="x", shade=4, var=0)
    cyl(p, "Steel", (0.13, 0, 0), 0.07, 0.07, 0.04, seg=8, axis="x", shade=5, var=0)
    for index in range(5):
        a = index / 5 * math.tau
        box(p, "Steel", (0.15, math.sin(a) * 0.12, math.cos(a) * 0.12), (0.02, 0.05, 0.2), rot=(-a, 0, 0), shade=3, var=0)


def cone(p, rng):
    box(p, "Rubber", (0, -0.33, 0), (0.42, 0.04, 0.42), var=0)
    cyl(p, "Orange", (0, 0.0, 0), 0.19, 0.04, 0.62, seg=12, var=0)
    cyl(p, "White", (0, 0.04, 0), 0.115, 0.095, 0.1, seg=12, shade=5, var=0)
    cyl(p, "White", (0, -0.14, 0), 0.165, 0.145, 0.1, seg=12, shade=5, var=0)


def barrel(p, rng):
    cyl(p, "Blue", (0, 0, 0), 0.3, 0.3, 0.9, seg=14)
    for y in (-0.3, 0.0, 0.3):
        cyl(p, "Iron", (0, y, 0), 0.31, 0.31, 0.05, seg=14, var=0)
    cyl(p, "Steel", (0.12, 0.45, 0.08), 0.05, 0.05, 0.03, seg=8, shade=5, var=0)
    box(p, "White", (0, 0.12, 0.305), (0.22, 0.16, 0.01), var=0)


def crate(p, rng):
    box(p, "Wood", (0, 0, 0), (0.94, 0.94, 0.94), shade=2)
    for axis in range(3):
        for a in (-1, 1):
            for b in (-1, 1):
                loc, size = [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
                u, v = (axis + 1) % 3, (axis + 2) % 3
                loc[u], loc[v] = a * 0.45, b * 0.45
                size[axis], size[u], size[v] = 1.0, 0.1, 0.1
                box(p, "Wood", tuple(loc), tuple(size), shade=4)
    for sign in (-1, 1):
        box(p, "Wood", (0, 0, sign * 0.48), (0.14, 1.2, 0.02), rot=(0, 0, 0.785), shade=4)
        box(p, "Wood", (sign * 0.48, 0, 0), (0.02, 1.2, 0.14), rot=(0.785, 0, 0), shade=4)


def hay_bale(p, rng):
    box(p, "Hay", (0, 0, 0), (1.1, 0.5, 0.55), shade=3, var=2)
    for x in (-0.3, 0.3):
        box(p, "Red", (x, 0, 0), (0.03, 0.52, 0.57), shade=2, var=0)
    for index in range(10):
        box(p, "Hay", (rng.uniform(-0.5, 0.5), 0.26, rng.uniform(-0.24, 0.24)), (0.1, 0.03, 0.08), rot=(0, rng.uniform(0, 3), 0), shade=5, var=1)


def ramp(p, rng, length, height, width):
    half = width * 0.5
    angle = math.atan2(height, length)
    slope = math.hypot(length, height)
    prism(p, "Concrete", [(-length * 0.5, 0.0), (length * 0.5, height), (length * 0.5, 0.0)], "x", -half, half, shade=2, var=0)
    box(p, "Steel", (0, height * 0.5, 0), (width, 0.05, slope), rot=(-angle, 0, 0), shade=3, var=0)
    stripes = max(4, int(slope / 0.6))
    for index in range(stripes):
        t = (index + 0.5) / stripes
        box(p, "Yellow" if index % 2 == 0 else "Rubber", (0, height * t + 0.03, -length * 0.5 + length * t), (width, 0.02, slope / stripes), rot=(-angle, 0, 0), var=0)
    for side in (-1, 1):
        box(p, "Red", (side * (half + 0.06), height * 0.5 + 0.08, 0), (0.12, 0.16, slope), rot=(-angle, 0, 0), var=0)


def tabletop(p, rng):
    up, top, height, width = 9.0, 8.0, 2.2, 6.0
    half = width * 0.5
    total = up * 2 + top
    outline = [(-total * 0.5, 0.0), (-total * 0.5 + up, height), (total * 0.5 - up, height), (total * 0.5, 0.0)]
    prism(p, "Concrete", outline, "x", -half, half, shade=2, var=0)
    box(p, "Steel", (0, height + 0.02, 0), (width, 0.04, top), shade=3, var=0)
    angle = math.atan2(height, up)
    slope = math.hypot(up, height)
    for sign in (-1, 1):
        box(p, "Steel", (0, height * 0.5 + 0.02, sign * (top * 0.5 + up * 0.5)), (width, 0.04, slope), rot=(sign * angle, 0, 0), shade=3, var=0)
        for index in range(6):
            t = (index + 0.5) / 6
            z = sign * (top * 0.5 + up * (1.0 - t))
            box(p, "Yellow" if index % 2 == 0 else "Rubber", (0, height * t + 0.05, z), (width, 0.02, slope / 6), rot=(sign * angle, 0, 0), var=0)
    for side in (-1, 1):
        box(p, "Red", (side * (half + 0.06), height + 0.1, 0), (0.12, 0.2, top), var=0)
        for z in (-top * 0.5, top * 0.5):
            cyl(p, "GlowAmber", (side * (half + 0.06), height + 0.3, z), 0.08, 0.06, 0.18, seg=8, var=0)


def pine(p, rng):
    height = rng.uniform(5.5, 8.0)
    cyl(p, "Bark", (0, 0.9, 0), 0.26, 0.2, 1.8, seg=6)
    for index in range(4):
        y = 1.4 + index * height * 0.2
        radius = 2.0 - index * 0.42
        cyl(p, "Pine", (0, y + 0.9, 0), radius, 0.15, height * 0.32, seg=7, rot=(0, rng.uniform(0, 1), 0), shade=2 + index, var=1)


def round_tree(p, rng):
    cyl(p, "Bark", (0, 1.3, 0), 0.3, 0.2, 2.6, seg=6)
    cyl(p, "Bark", (0.4, 2.5, 0.1), 0.12, 0.08, 1.2, seg=5, rot=(0, 0, -0.7))
    for index in range(3):
        a = index / 3 * math.tau + rng.uniform(0, 1)
        ico(p, "Leaf", (math.cos(a) * 0.9, 3.6 + rng.uniform(-0.2, 0.6), math.sin(a) * 0.9), rng.uniform(1.3, 1.7), jitter=0.18, seed=rng.randint(0, 999), shade=rng.randint(2, 5))
    ico(p, "Leaf", (0, 4.4, 0), 1.6, jitter=0.2, seed=rng.randint(0, 999), shade=4)


def palm(p, rng):
    lean = rng.uniform(0.2, 0.45)
    x, y = 0.0, 0.0
    segments = 7
    for index in range(segments):
        t = index / segments
        tilt = lean * (0.4 + t)
        length = 0.95
        cx = x + math.sin(tilt) * length * 0.5
        cy = y + math.cos(tilt) * length * 0.5
        cyl(p, "Bark", (cx, cy, 0), 0.24 - t * 0.08, 0.22 - t * 0.08, length, seg=6, rot=(0, 0, -tilt), shade=2 + index % 3, var=0)
        x += math.sin(tilt) * length
        y += math.cos(tilt) * length
    ico(p, "Bark", (x, y, 0), 0.32, scale=(1, 0.7, 1), shade=1, var=0)
    for index in range(3):
        a = index / 3 * math.tau
        ico(p, "Wood", (x + math.cos(a) * 0.22, y - 0.2, math.sin(a) * 0.22), 0.13, shade=2, var=0)
    for index in range(8):
        a = index / 8 * math.tau + rng.uniform(-0.2, 0.2)
        for k in range(3):
            r = 0.5 + k * 0.75
            droop = 0.2 + k * k * 0.25
            transform = Matrix.Translation((x + math.cos(a) * r, y + 0.1 - droop, math.sin(a) * r)) @ Matrix.Rotation(-a, 4, "Y") @ Matrix.Rotation(-0.2 - k * 0.35, 4, "Z") @ Matrix.Diagonal((0.8, 0.04, 0.42 - k * 0.1, 1.0))
            result = bmesh.ops.create_cube(p.bm, size=1.0, matrix=transform)
            p.tag(result["verts"], "Palm", 2 + k, 1)


def trunk_collider(radius, height):
    def collider(c):
        cyl(c, "Bark", (0, height * 0.5, 0), radius, radius, height, seg=6, var=0)
    return collider


def box_collider(loc, size):
    def collider(c):
        box(c, "Concrete", loc, size, var=0)
    return collider


def rock(p, rng, seed):
    ico(p, "Rock", (0, 0.5, 0), 1.0, scale=(1.3, 0.8, 1.0), jitter=0.28, flatten_below=-0.3, seed=seed, subdiv=1, shade=3, var=2)


def bush(p, rng):
    for index in range(3):
        a = index / 3 * math.tau + rng.uniform(0, 1)
        ico(p, "Leaf", (math.cos(a) * 0.35, 0.35, math.sin(a) * 0.35), rng.uniform(0.45, 0.6), jitter=0.2, seed=rng.randint(0, 999), shade=rng.randint(1, 4))


def grass(p, rng):
    for index in range(7):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.0, 0.25)
        height = rng.uniform(0.25, 0.5)
        cyl(p, "Grass", (math.cos(a) * r, height * 0.5, math.sin(a) * r), 0.05, 0.0, height, seg=3, rot=(rng.uniform(-0.3, 0.3), 0, rng.uniform(-0.3, 0.3)), shade=rng.randint(3, 6), var=0)


def flowers(p, rng):
    grass(p, rng)
    for index in range(4):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.05, 0.25)
        box(p, rng.choice(["Flower", "Yellow", "White"]), (math.cos(a) * r, rng.uniform(0.35, 0.5), math.sin(a) * r), (0.09, 0.05, 0.09), shade=5, var=1)


def gable_roof(p, family, width, depth, eave, ridge, overhang):
    run = depth * 0.5 + overhang
    angle = math.atan2(ridge - eave, run)
    slab = math.hypot(run, ridge - eave) + 0.12
    for side in (-1, 1):
        box(p, family, (0, (eave + ridge) * 0.5 + 0.06, side * run * 0.5), (width + overhang * 2.0, 0.16, slab), rot=(side * angle, 0, 0), shade=3, var=0)
    box(p, family, (0, ridge + 0.1, 0), (width + overhang * 2.0 + 0.04, 0.12, 0.3), shade=1, var=0)


def house(p, rng, wall, roof):
    width, depth, height = 7.0, 6.0, 3.2
    box(p, "Concrete", (0, -0.9, 0), (width + 0.4, 2.2, depth + 0.4), shade=2, var=0)
    box(p, wall, (0, height * 0.5, 0), (width, height, depth), var=0)
    prism(p, wall, [(-depth * 0.5, height), (0.0, height + 2.0), (depth * 0.5, height)], "x", -width * 0.5, width * 0.5, shade=4, var=0)
    gable_roof(p, roof, width, depth, height - 0.4, height + 2.05, 0.5)
    box(p, "Wood", (0.0, 1.05, depth * 0.5 + 0.02), (1.0, 2.1, 0.06), shade=2, var=0)
    box(p, "Concrete", (0.0, 0.08, depth * 0.5 + 0.5), (1.6, 0.16, 1.0), var=0)
    for x in (-2.3, 2.3):
        box(p, "White", (x, 1.7, depth * 0.5 + 0.02), (1.25, 1.15, 0.06), shade=5, var=0)
        box(p, "GlowWindow" if rng.random() < 0.3 else "Glass", (x, 1.7, depth * 0.5 + 0.05), (1.0, 0.9, 0.04), var=0)
    for z in (-1.4, 1.4):
        for side in (-1, 1):
            box(p, "White", (side * (width * 0.5 + 0.02), 1.7, z), (0.06, 1.15, 1.25), shade=5, var=0)
            box(p, "Glass", (side * (width * 0.5 + 0.05), 1.7, z), (0.04, 0.9, 1.0), var=0)
    box(p, "Rock", (2.2, height + 1.4, -1.0), (0.6, 1.6, 0.6), shade=2, var=0)
    box(p, "Wood", (-2.8, 0.45, depth * 0.5 + 1.6), (0.08, 0.9, 0.08), var=0)
    box(p, "Red", (-2.8, 1.0, depth * 0.5 + 1.6), (0.35, 0.25, 0.25), var=0)


def barn(p, rng):
    width, depth, height = 9.0, 7.0, 4.0
    box(p, "Concrete", (0, -0.9, 0), (width + 0.4, 2.2, depth + 0.4), shade=2, var=0)
    box(p, "Red", (0, height * 0.5, 0), (width, height, depth), var=0)
    prism(p, "Red", [(-depth * 0.5, height), (0.0, height + 2.6), (depth * 0.5, height)], "x", -width * 0.5, width * 0.5, shade=4, var=0)
    gable_roof(p, "Iron", width, depth, height - 0.3, height + 2.65, 0.35)
    box(p, "Red", (0, 1.4, depth * 0.5 + 0.03), (3.0, 2.8, 0.06), shade=2, var=0)
    for a in (-1, 1):
        box(p, "White", (0, 1.4, depth * 0.5 + 0.07), (0.12, 3.6, 0.04), rot=(0, 0, a * 0.75), shade=5, var=0)
    box(p, "White", (0, 2.85, depth * 0.5 + 0.07), (3.2, 0.12, 0.04), shade=5, var=0)
    for side in (-1, 1):
        box(p, "White", (side * 1.55, 1.4, depth * 0.5 + 0.07), (0.12, 2.9, 0.04), shade=5, var=0)
    box(p, "White", (0, height + 1.2, depth * 0.5 + 0.04), (1.2, 0.9, 0.04), shade=5, var=0)
    box(p, "Glass", (0, height + 1.2, depth * 0.5 + 0.07), (0.9, 0.65, 0.04), var=0)


def lighthouse(p, rng):
    cyl(p, "Concrete", (0, 0.0, 0), 3.4, 3.2, 3.0, seg=12, shade=2, var=0)
    rings = 6
    for index in range(rings):
        y0 = 1.5 + index * 2.4
        r0 = 2.3 - index * 0.17
        cyl(p, "White" if index % 2 == 0 else "Red", (0, y0 + 1.2, 0), r0, r0 - 0.17, 2.4, seg=12, shade=4 if index % 2 == 0 else 3, var=0)
    top = 1.5 + rings * 2.4
    cyl(p, "Iron", (0, top + 0.1, 0), 1.9, 1.9, 0.2, seg=12, var=0)
    for index in range(12):
        a = index / 12 * math.tau
        box(p, "Iron", (math.cos(a) * 1.8, top + 0.6, math.sin(a) * 1.8), (0.06, 0.8, 0.06), var=0)
    cyl(p, "Iron", (0, top + 1.0, 0), 1.84, 1.84, 0.06, seg=12, var=0)
    cyl(p, "Glass", (0, top + 1.1, 0), 1.0, 1.0, 1.8, seg=10, var=0)
    cyl(p, "GlowWhite", (0, top + 1.1, 0), 0.55, 0.55, 1.2, seg=10, var=0)
    cyl(p, "Red", (0, top + 2.4, 0), 1.2, 0.15, 0.9, seg=10, var=0)
    ico(p, "Iron", (0, top + 2.95, 0), 0.18, var=0)
    box(p, "Wood", (0, 1.0, 2.2), (1.0, 2.0, 0.2), shade=2, var=0)


def start_arch(p, rng):
    span = ROAD_HALF + 1.0
    for side in (-1, 1):
        box(p, "Concrete", (side * span, 0.2, 0), (0.9, 0.4, 0.9), var=0)
        box(p, "Red", (side * span, 3.3, 0), (0.5, 6.6, 0.5), var=0)
        box(p, "White", (side * span, 1.2, 0), (0.52, 0.2, 0.52), shade=5, var=0)
    box(p, "Red", (0, 6.4, 0), (span * 2 + 0.5, 0.4, 0.5), shade=2, var=0)
    columns = 22
    for row in range(2):
        for index in range(columns):
            x = -span + 0.25 + (index + 0.5) * (span * 2 - 0.5) / columns
            family = "White" if (index + row) % 2 == 0 else "Rubber"
            box(p, family, (x, 5.75 - row * 0.5, 0), ((span * 2 - 0.5) / columns, 0.5, 0.12), shade=5 if family == "White" else 2, var=0)
    for side in (-1, 1):
        for index in range(3):
            cyl(p, "GlowAmber", (side * (span - 1.5 - index * 1.2), 6.75, 0), 0.14, 0.1, 0.25, seg=8, var=0)


def start_arch_collider(c):
    for side in (-1, 1):
        box(c, "Concrete", (side * (ROAD_HALF + 1.0), 3.3, 0), (0.9, 6.6, 0.9), var=0)


def lamp_post(p, rng):
    box(p, "Concrete", (0, 0.15, 0), (0.45, 0.3, 0.45), var=0)
    cyl(p, "Iron", (0, 3.0, 0), 0.1, 0.07, 6.0, seg=8, shade=4, var=0)
    box(p, "Iron", (0, 5.9, 0.85), (0.08, 0.08, 1.7), shade=4, var=0)
    box(p, "Iron", (0, 5.75, 1.7), (0.3, 0.16, 0.6), var=0)
    box(p, "GlowAmber", (0, 5.66, 1.7), (0.24, 0.03, 0.5), var=0)


def chevron(p, rng, direction):
    for x in (-0.5, 0.5):
        box(p, "Iron", (x, 0.7, -0.03), (0.08, 1.4, 0.08), shade=4, var=0)
    box(p, "Yellow", (0, 1.65, 0), (1.5, 0.8, 0.04), shade=4, var=0)
    box(p, "Rubber", (0, 1.65, -0.01), (1.56, 0.86, 0.03), var=0)
    for offset in (-0.3, 0.14):
        for sign in (-1, 1):
            box(p, "Rubber", (direction * offset, 1.65 + sign * 0.14, 0.03), (0.42, 0.12, 0.02), rot=(0, 0, -direction * sign * 0.75), var=0)


def chevron_collider(c):
    for x in (-0.5, 0.5):
        box(c, "Iron", (x, 1.0, 0), (0.1, 2.0, 0.1), var=0)


def build_layout(rng):
    layout = bpy.data.objects.new("Layout", None)
    bpy.context.scene.collection.objects.link(layout)
    counters = {}

    def place(kind, asset, pos, yaw=0.0, scale=1.0):
        index = counters.get((kind, asset), 0)
        counters[(kind, asset)] = index + 1
        name = asset if kind is None else f"{kind}_{asset}_{index:04d}"
        empty = bpy.data.objects.new(name, None)
        empty.location = TO_BLENDER @ Vector(pos)
        empty.rotation_euler = (0.0, 0.0, yaw)
        empty.scale = (scale, scale, scale)
        empty.parent = layout
        bpy.context.scene.collection.objects.link(empty)

    def height_at(x, z):
        h, _, _ = final_height(np.array([x]), np.array([z]))
        return float(h[0])

    def face_yaw(dx, dz):
        return math.atan2(dx, dz)

    for index in range(0, ROAD.count, WAYPOINT_STRIDE):
        place("Road", "Point", ROAD.point(index), float(ROAD.yaw[index]))

    place(None, "Spawn", ROAD.point(8), float(ROAD.yaw[8]))
    place("Place", "StartArch", ROAD.point(1), float(ROAD.yaw[1]))

    occupied = []

    def free(x, z, radius):
        return all(math.hypot(x - ox, z - oz) > radius + orad for ox, oz, orad in occupied)

    last_chevron = -100
    for index in range(ROAD.count):
        k = ROAD.curvature[index]
        if abs(k) < 1.0 / 60.0 or index - last_chevron < 6:
            continue
        last_chevron = index
        side = -1.0 if k > 0.0 else 1.0
        x, _, z = ROAD.point(index, side * (ROAD_HALF + 3.0))
        tangent = ROAD.tangent[index]
        place("Place", "ChevronL" if k > 0.0 else "ChevronR", (x, height_at(x, z) - 0.05, z), face_yaw(-tangent[0], -tangent[1]))
        occupied.append((x, z, 1.5))

    tight = sorted(range(ROAD.count), key=lambda i: -abs(ROAD.curvature[i]))
    corners = []
    for index in tight:
        if all(min(abs(index - c), ROAD.count - abs(index - c)) > 25 for c in corners):
            corners.append(index)
        if len(corners) == 5:
            break

    for corner_number, index in enumerate(corners):
        k = ROAD.curvature[index]
        inside = 1.0 if k > 0.0 else -1.0
        for offset in (-6, 0, 6):
            x, _, z = ROAD.point(index + offset, inside * (ROAD_HALF + 1.4))
            place("Prop", "Cone", (x, height_at(x, z) + 0.36, z), rng.uniform(0, math.tau))
        if corner_number < 3:
            for offset in range(-6, 7, 3):
                x, _, z = ROAD.point(index + offset, -inside * (ROAD_HALF + 3.2))
                t = ROAD.tangent[(index + offset) % ROAD.count]
                place("Prop", "HayBale", (x, height_at(x, z) + 0.27, z), face_yaw(t[1], -t[0]))
                occupied.append((x, z, 1.0))

    village = [index for index in range(ROAD.count) if ROAD.xz[index][1] > 100.0 and -20.0 < ROAD.xz[index][0] < 115.0]
    houses = ["HouseA", "HouseB", "Barn"]
    for number, index in enumerate(village[::7]):
        for side in (-1.0, 1.0):
            if rng.random() < 0.25:
                continue
            offset = ROAD_HALF + rng.uniform(13.0, 17.0)
            x, _, z = ROAD.point(index, side * offset)
            distance, _ = ROAD.nearest([x], [z])
            if distance[0] < ROAD_HALF + 11.0 or not free(x, z, 7.0):
                continue
            left = ROAD.left[index]
            place("Place", rng.choice(houses), (x, height_at(x, z), z), face_yaw(-left[0] * side, -left[1] * side) + rng.uniform(-0.1, 0.1))
            occupied.append((x, z, 6.0))

    for number, index in enumerate(village[::12]):
        side = 1.0 if number % 2 == 0 else -1.0
        x, _, z = ROAD.point(index, side * (ROAD_HALF + 1.3))
        left = ROAD.left[index]
        place("Place", "LampPost", (x, height_at(x, z), z), face_yaw(-left[0] * side, -left[1] * side))
        occupied.append((x, z, 0.8))

    theta = -0.35
    for radius in np.linspace(260.0, 120.0, 300):
        lx, lz = math.cos(theta) * radius, math.sin(theta) * radius
        if height_at(lx, lz) > 2.6:
            break
    lh = height_at(lx, lz)
    place("Place", "Lighthouse", (lx, lh - 0.4, lz), rng.uniform(0, math.tau))
    occupied.append((lx, lz, 6.0))
    for index in range(5):
        a = index / 5 * math.tau
        rx, rz = lx + math.cos(a) * 5.5, lz + math.sin(a) * 5.5
        place("Place", f"Rock{index % 3}", (rx, height_at(rx, rz) - 0.3, rz), rng.uniform(0, math.tau), rng.uniform(1.4, 2.2))
        occupied.append((rx, rz, 2.5))

    pad_y = PAD_HEIGHT
    place("Place", "Tabletop", (PAD_X, pad_y, PAD_Z), math.pi * 0.5)
    place("Place", "Ramp", (PAD_X - 30.0, pad_y, PAD_Z + 16.0), math.pi * 0.5)
    place("Place", "Kicker", (PAD_X - 30.0, pad_y, PAD_Z - 16.0), math.pi * 0.5)

    def pyramid(cx, cz, yaw):
        c, s = math.cos(yaw), math.sin(yaw)
        for row, count in enumerate((3, 2, 1)):
            for index in range(count):
                lx = (index - (count - 1) * 0.5) * 1.02
                place("Prop", "Crate", (cx + lx * c, pad_y + 0.47 + row * 0.95, cz - lx * s), yaw)

    pyramid(PAD_X + 30.0, PAD_Z, math.pi * 0.5)
    pyramid(PAD_X - 8.0, PAD_Z + 16.0, math.pi * 0.5)
    pyramid(PAD_X - 8.0, PAD_Z - 16.0, math.pi * 0.5)

    for index in range(9):
        x = PAD_X - 30.0 + index * 7.0
        place("Prop", "Cone", (x, pad_y + 0.36, PAD_Z + 22.0), rng.uniform(0, math.tau))

    ring_x, ring_z = PAD_X + 24.0, PAD_Z - 14.0
    for index in range(10):
        a = index / 10 * math.tau
        place("Prop", "Cone", (ring_x + math.cos(a) * 8.2, pad_y + 0.36, ring_z + math.sin(a) * 8.2), rng.uniform(0, math.tau))
    for index in range(5):
        a = index / 5 * math.tau
        place("Prop", "Barrel", (ring_x + math.cos(a) * 0.8, pad_y + 0.46, ring_z + math.sin(a) * 0.8), rng.uniform(0, math.tau))

    for side in (-1.0, 1.0):
        for index in range(8):
            x = PAD_X - PAD_HX + 4.0 + index * 10.0
            z = PAD_Z + side * (PAD_HZ - 2.2)
            if side > 0 and index % 2 == 1:
                continue
            place("Prop", "HayBale", (x, pad_y + 0.27, z), 0.0)

    for kind_name, count, radius in (("Barrel", 10, 1.0), ("Crate", 8, 1.0)):
        for _ in range(count):
            for attempt in range(40):
                index = rng.randrange(ROAD.count)
                side = rng.choice((-1.0, 1.0))
                x, _, z = ROAD.point(index, side * (ROAD_HALF + rng.uniform(3.0, 6.0)))
                if free(x, z, radius) and height_at(x, z) > 1.0:
                    place("Prop", kind_name, (x, height_at(x, z) + (0.46 if kind_name == "Barrel" else 0.48), z), rng.uniform(0, math.tau))
                    occupied.append((x, z, radius))
                    break

    count = 9000
    cx = np.array([rng.uniform(-240.0, 240.0) for _ in range(count)])
    cz = np.array([rng.uniform(-240.0, 240.0) for _ in range(count)])
    ch, cd, croad = final_height(cx, cz)
    cpad = pad_distance(cx, cz)
    slope = np.zeros(count)
    for dx, dz in ((1.0, 0.0), (0.0, 1.0)):
        hn, _, _ = final_height(cx + dx, cz + dz)
        slope = np.maximum(slope, np.abs(hn - ch))

    trees, rocks, bushes, tufts = 0, 0, 0, 0
    for index in range(count):
        x, z, h, d = float(cx[index]), float(cz[index]), float(ch[index]), float(cd[index])
        if d > 0.95 or h < 0.6 or cpad[index] < 4.0:
            continue
        clear = croad[index] - ROAD_HALF
        roll = rng.random()

        if trees < 420 and clear > 7.0 and slope[index] < 0.9 and roll < 0.25:
            if not free(x, z, 3.0):
                continue
            if d > 0.74 and h < 4.5:
                asset = "Palm"
            elif h > 14.0:
                asset = "Pine"
            else:
                asset = "Pine" if rng.random() < 0.45 else "RoundTree"
            place("Place", asset, (x, h - 0.1, z), rng.uniform(0, math.tau), rng.uniform(0.8, 1.3))
            occupied.append((x, z, 1.0))
            trees += 1
        elif rocks < 80 and clear > 4.0 and roll < 0.3:
            if not free(x, z, 2.0):
                continue
            scale = rng.uniform(0.5, 1.8) if h > 3.0 else rng.uniform(0.8, 2.4)
            place("Place", f"Rock{rng.randrange(3)}", (x, h - 0.2, z), rng.uniform(0, math.tau), scale)
            occupied.append((x, z, scale))
            rocks += 1
        elif bushes < 260 and clear > 3.0 and roll < 0.42 and h > 1.5:
            place("Place", "Bush", (x, h - 0.05, z), rng.uniform(0, math.tau), rng.uniform(0.7, 1.4))
            bushes += 1
        elif tufts < 900 and clear > 1.5 and h > 1.5:
            place("Place", "Flowers" if rng.random() < 0.2 else "Grass", (x, h - 0.02, z), rng.uniform(0, math.tau), rng.uniform(0.8, 1.6))
            tufts += 1

    print(f"layout: {trees} trees, {rocks} rocks, {bushes} bushes, {tufts} grass, {len(corners)} corners")


ASSETS = [
    ("Car", car),
    ("Wheel", lambda r: single(r, wheel)),
    ("Cone", lambda r: single(r, cone)),
    ("Barrel", lambda r: single(r, barrel)),
    ("Crate", lambda r: single(r, crate)),
    ("HayBale", lambda r: single(r, hay_bale)),
    ("Ramp", lambda r: single(r, ramp, 7.0, 1.3, 4.0)),
    ("Kicker", lambda r: single(r, ramp, 3.2, 1.0, 3.6)),
    ("Tabletop", lambda r: single(r, tabletop)),
    ("Pine", lambda r: with_collision(r, pine, trunk_collider(0.28, 3.0))),
    ("RoundTree", lambda r: with_collision(r, round_tree, trunk_collider(0.32, 3.0))),
    ("Palm", lambda r: with_collision(r, palm, trunk_collider(0.26, 3.0))),
    ("Bush", lambda r: single(r, bush)),
    ("Grass", lambda r: single(r, grass)),
    ("Flowers", lambda r: single(r, flowers)),
    ("HouseA", lambda r: with_collision(r, lambda p, rr: house(p, rr, "Cream", "Roof"), box_collider((0, 2.5, 0), (7.4, 5.0, 6.4)))),
    ("HouseB", lambda r: with_collision(r, lambda p, rr: house(p, rr, "Teal", "Iron"), box_collider((0, 2.5, 0), (7.4, 5.0, 6.4)))),
    ("Barn", lambda r: with_collision(r, barn, box_collider((0, 3.0, 0), (9.2, 6.0, 7.4)))),
    ("Lighthouse", lambda r: with_collision(r, lighthouse, trunk_collider(3.3, 18.0))),
    ("StartArch", lambda r: with_collision(r, start_arch, start_arch_collider)),
    ("LampPost", lambda r: with_collision(r, lamp_post, trunk_collider(0.2, 6.0))),
    ("ChevronL", lambda r: with_collision(r, lambda p, rr: chevron(p, rr, -1.0), chevron_collider)),
    ("ChevronR", lambda r: with_collision(r, lambda p, rr: chevron(p, rr, 1.0), chevron_collider)),
]

for index in range(3):
    ASSETS.append((f"Rock{index}", lambda r, index=index: single(r, rock, 100 + index)))


def main():
    global ROAD, PAD_HEIGHT

    output = Path(sys.argv[sys.argv.index("--") + 1]) if "--" in sys.argv else Path(__file__).with_name("CoastRally.glb")

    bpy.ops.wm.read_factory_settings(use_empty=True)

    ROAD = Road()
    pad_x, pad_z = np.meshgrid(np.linspace(PAD_X - PAD_HX, PAD_X + PAD_HX, 9), np.linspace(PAD_Z - PAD_HZ, PAD_Z + PAD_HZ, 7))
    PAD_HEIGHT = max(float(np.mean(base_height(pad_x, pad_z)[0])), 2.0)

    write_palettes(output.parent)

    material = bpy.data.materials.new("Palette")

    for name, function in (("Terrain", terrain), ("Road", road), ("Pad", pad), ("Water", water)):
        build_asset(name, function(random.Random(name)), (0.0, 0.0, 0.0), material)

    for index, (name, function) in enumerate(ASSETS):
        rng = random.Random(3000 + index)
        offset = (400.0 + (index % 8) * 30.0, (index // 8) * 30.0, 0.0)
        build_asset(name, function(rng), offset, material)

    build_layout(random.Random(42))

    bpy.ops.wm.save_as_mainfile(filepath=str(output.with_suffix(".blend")))
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True, export_apply=True, export_normals=True, export_texcoords=True, export_materials="EXPORT", export_cameras=False, export_lights=False)


main()
