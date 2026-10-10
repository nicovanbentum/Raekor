import bpy
import bmesh
import math
import random
import sys
from pathlib import Path
from mathutils import Matrix, Euler, Vector


GRID = 32
SWATCH = 32
SIZE = GRID * SWATCH
SHADES = [0.62, 0.74, 0.86, 1.0, 1.1, 1.2, 1.32, 1.46]

FAMILIES = [
    ("Hoodie",      (0.85, 0.25, 0.45), 0.0),
    ("Denim",       (0.15, 0.20, 0.35), 0.0),
    ("Skin",        (0.85, 0.65, 0.50), 0.0),
    ("Cap",         (0.15, 0.55, 0.95), 0.0),
    ("Backpack",    (0.95, 0.65, 0.15), 0.0),
    ("White",       (0.86, 0.86, 0.86), 0.0),
    ("Rubber",      (0.05, 0.05, 0.06), 0.0),
    ("Navy",        (0.08, 0.11, 0.24), 0.0),
    ("Gold",        (0.95, 0.70, 0.22), 0.0),
    ("TrainBlue",   (0.12, 0.30, 0.70), 0.0),
    ("TrainRed",    (0.72, 0.12, 0.10), 0.0),
    ("TrainGreen",  (0.12, 0.48, 0.26), 0.0),
    ("TrainSilver", (0.60, 0.62, 0.66), 0.0),
    ("Glass",       (0.07, 0.10, 0.15), 0.0),
    ("Steel",       (0.48, 0.48, 0.52), 0.0),
    ("Iron",        (0.13, 0.13, 0.15), 0.0),
    ("Gravel",      (0.30, 0.28, 0.26), 0.0),
    ("Wood",        (0.30, 0.19, 0.11), 0.0),
    ("Concrete",    (0.55, 0.55, 0.53), 0.0),
    ("Yellow",      (0.95, 0.75, 0.10), 0.0),
    ("Brick",       (0.50, 0.22, 0.15), 0.0),
    ("Cream",       (0.78, 0.70, 0.55), 0.0),
    ("Teal",        (0.28, 0.48, 0.50), 0.0),
    ("Orange",      (0.80, 0.50, 0.30), 0.0),
    ("Red",         (0.80, 0.12, 0.10), 0.0),
    ("Green",       (0.18, 0.40, 0.18), 0.0),
    ("GlowWindow",  (1.00, 0.85, 0.55), 0.12),
    ("GlowWhite",   (1.00, 0.95, 0.85), 1.0),
    ("GlowGold",    (1.00, 0.75, 0.20), 0.15),
    ("GlowRed",     (1.00, 0.22, 0.16), 0.6),
    ("GlowGreen",   (0.35, 1.00, 0.45), 0.6),
    ("GlowCyan",    (0.35, 0.85, 1.00), 0.3),
]

FAMILY_INDEX = {name: index for index, (name, _, _) in enumerate(FAMILIES)}

TRAIN_COLORS = [("Blue", "TrainBlue"), ("Red", "TrainRed"), ("Green", "TrainGreen"), ("Silver", "TrainSilver")]

LANE_WIDTH = 2.6
TRACK_HALF = (3 * LANE_WIDTH + 1.0) * 0.5
SEGMENT_LENGTH = 30.0
TRAIN_WIDTH = 2.3
TRAIN_HEIGHT = 3.0
UNIT_LENGTH = 6.0
RAMP_LENGTH = 9.0
PLATFORM_WIDTH = 4.0
PLATFORM_TOP = 0.85
BUILDING_LENGTH = 14.4

TO_BLENDER = Matrix.Rotation(math.pi * 0.5, 4, "X")


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
        faces = {face for vert in verts for face in vert.link_faces}
        for face in faces:
            face[self.family] = FAMILY_INDEX[family]
            value = shade + (self.rng.randint(-variation, variation) if variation else 0)
            face[self.shade] = max(0, min(len(SHADES) - 1, value))
        return verts


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


def grid(p, x0, x1, z0, z1, y, cells_x, cells_z, pick, jitter=0.0, per_cell=False):
    verts = []
    for j in range(cells_z + 1):
        row = []
        for i in range(cells_x + 1):
            x = x0 + (x1 - x0) * i / cells_x
            z = z0 + (z1 - z0) * j / cells_z
            h = y + (p.rng.uniform(-jitter, jitter) if 0 < i < cells_x and 0 < j < cells_z else 0.0)
            row.append(p.bm.verts.new((x, h, z)))
        verts.append(row)

    for j in range(cells_z):
        for i in range(cells_x):
            a, b, c, d = verts[j][i], verts[j + 1][i], verts[j + 1][i + 1], verts[j][i + 1]
            tris = ((a, b, c), (a, c, d)) if (i + j) % 2 == 0 else ((a, b, d), (b, c, d))
            cell = pick((a.co + c.co) * 0.5) if per_cell else None
            for tri in tris:
                face = p.bm.faces.new(tri)
                family, shade = cell if per_cell else pick(face.calc_center_median())
                face[p.family] = FAMILY_INDEX[family]
                face[p.shade] = shade


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


def runner(rng):
    body, head = Part("Body", (0, 1.0, 0), rng), Part("Head", (0, 1.5, 0), rng)
    arm_l, arm_r = Part("ArmL", (0.36, 1.38, 0), rng), Part("ArmR", (-0.36, 1.38, 0), rng)
    leg_l, leg_r = Part("LegL", (0.15, 0.8, 0), rng), Part("LegR", (-0.15, 0.8, 0), rng)
    shoe_l, shoe_r = Part("ShoeL", (0.15, 0.8, 0), rng), Part("ShoeR", (-0.15, 0.8, 0), rng)

    box(body, "Hoodie", (0, 1.13, 0), (0.56, 0.62, 0.34))
    box(body, "Hoodie", (0, 1.4, -0.06), (0.46, 0.1, 0.3), shade=2)
    box(body, "Hoodie", (0, 1.42, -0.15), (0.36, 0.18, 0.12), shade=2)
    box(body, "Hoodie", (0, 0.96, 0.175), (0.34, 0.14, 0.02), shade=1, var=0)
    for side in (-1, 1):
        box(body, "White", (side * 0.06, 1.3, 0.18), (0.025, 0.16, 0.02), var=0)
    box(body, "Denim", (0, 0.83, 0), (0.5, 0.12, 0.3), shade=2)
    box(body, "Backpack", (0, 1.15, -0.27), (0.44, 0.5, 0.22))
    box(body, "Backpack", (0, 1.0, -0.39), (0.34, 0.2, 0.05), shade=2)
    box(body, "Iron", (0, 1.38, -0.27), (0.3, 0.05, 0.16), var=0)
    for side in (-1, 1):
        box(body, "Iron", (side * 0.17, 1.2, 0.0), (0.05, 0.5, 0.36), var=0)
        box(body, "Yellow", (side * 0.23, 1.2, -0.27), (0.02, 0.06, 0.1), var=0)

    ico(head, "Skin", (0, 1.66, 0.01), 0.21)
    ico(head, "Cap", (0, 1.73, 0), 0.225, flatten_below=1.74, var=0)
    box(head, "Cap", (0, 1.745, 0.2), (0.34, 0.035, 0.2), shade=2, var=0)
    box(head, "White", (0, 1.83, 0.12), (0.06, 0.06, 0.06), var=0)
    for side in (-1, 1):
        box(head, "Rubber", (side * 0.075, 1.67, 0.195), (0.04, 0.05, 0.02), var=0)
        box(head, "Skin", (side * 0.21, 1.65, 0.0), (0.05, 0.08, 0.06), shade=2)

    for part, side in ((arm_l, 1), (arm_r, -1)):
        box(part, "Hoodie", (side * 0.36, 1.12, 0), (0.15, 0.52, 0.17))
        box(part, "White", (side * 0.36, 0.86, 0), (0.16, 0.06, 0.18), var=0)
        ico(part, "Skin", (side * 0.36, 0.77, 0.01), 0.08)

    for part, side in ((leg_l, 1), (leg_r, -1)):
        box(part, "Denim", (side * 0.15, 0.45, 0), (0.2, 0.7, 0.22))
        box(part, "Denim", (side * 0.15, 0.16, 0), (0.22, 0.08, 0.24), shade=2, var=0)

    for part, side in ((shoe_l, 1), (shoe_r, -1)):
        box(part, "White", (side * 0.15, 0.07, 0.05), (0.22, 0.12, 0.34))
        box(part, "Rubber", (side * 0.15, 0.015, 0.05), (0.23, 0.03, 0.36), var=0)
        box(part, "Cap", (side * (0.15 + 0.112), 0.07, 0.03), (0.01, 0.05, 0.18), var=0)
        box(part, "Cap", (side * (0.15 - 0.112), 0.07, 0.03), (0.01, 0.05, 0.18), var=0)

    return [body, head, arm_l, arm_r, leg_l, leg_r, shoe_l, shoe_r]


def guard(rng):
    body, head = Part("Body", (0, 1.1, 0), rng), Part("Head", (0, 1.6, 0), rng)
    arm_l, arm_r = Part("ArmL", (0.44, 1.5, 0), rng), Part("ArmR", (-0.44, 1.5, 0), rng)
    leg_l, leg_r = Part("LegL", (0.18, 0.86, 0), rng), Part("LegR", (-0.18, 0.86, 0), rng)

    box(body, "Navy", (0, 1.2, 0), (0.7, 0.68, 0.42))
    ico(body, "Navy", (0, 1.08, 0.12), 0.34, scale=(1.0, 0.8, 0.8))
    box(body, "Iron", (0, 0.9, 0.0), (0.66, 0.1, 0.46), var=0)
    box(body, "Gold", (0, 0.9, 0.235), (0.1, 0.08, 0.02), var=0)
    for index in range(3):
        box(body, "Gold", (0, 1.42 - index * 0.16, 0.22 + index * 0.06), (0.05, 0.05, 0.03), var=0)
    box(body, "Gold", (0.18, 1.42, 0.22), (0.1, 0.12, 0.02), var=0)
    for side in (-1, 1):
        box(body, "Navy", (side * 0.34, 1.52, 0), (0.16, 0.08, 0.42), shade=2)
        box(body, "Gold", (side * 0.34, 1.565, 0), (0.14, 0.02, 0.3), var=0)

    ico(head, "Skin", (0, 1.76, 0.02), 0.22)
    box(head, "Rubber", (0, 1.69, 0.21), (0.18, 0.04, 0.04), var=0)
    for side in (-1, 1):
        box(head, "Rubber", (side * 0.075, 1.79, 0.205), (0.04, 0.04, 0.02), var=0)
        box(head, "Skin", (side * 0.22, 1.76, 0.0), (0.05, 0.08, 0.06), shade=2)
    cyl(head, "Navy", (0, 1.93, 0), 0.24, 0.27, 0.14, seg=10, var=0)
    box(head, "Rubber", (0, 1.88, 0.2), (0.34, 0.03, 0.2), var=0)
    box(head, "Gold", (0, 1.95, 0.25), (0.08, 0.08, 0.02), var=0)

    for part, side in ((arm_l, 1), (arm_r, -1)):
        box(part, "Navy", (side * 0.44, 1.22, 0), (0.17, 0.56, 0.2))
        box(part, "Gold", (side * 0.44, 0.96, 0), (0.18, 0.04, 0.21), var=0)
        ico(part, "Skin", (side * 0.44, 0.87, 0.01), 0.09)

    cyl(arm_r, "Iron", (-0.44, 0.8, 0.16), 0.03, 0.03, 0.5, seg=6, axis="z", var=0)

    for part, side in ((leg_l, 1), (leg_r, -1)):
        box(part, "Navy", (side * 0.18, 0.48, 0), (0.24, 0.76, 0.26), shade=2)
        box(part, "Rubber", (side * 0.18, 0.06, 0.05), (0.26, 0.12, 0.38), var=0)

    return [body, head, arm_l, arm_r, leg_l, leg_r]


def train_unit(rng, paint, front):
    body = Part("Body", (0, 0, 0), rng)
    length = UNIT_LENGTH
    center = length * 0.5

    box(body, paint, (0, 1.78, center), (TRAIN_WIDTH, 2.44, length - 0.1))
    box(body, "Steel", (0, 3.0, center), (TRAIN_WIDTH - 0.2, 0.06, length - 0.3), var=0)
    box(body, "Iron", (0, 0.33, center), (TRAIN_WIDTH - 0.2, 0.46, length - 0.4), var=0)
    box(body, "White", (0, 1.3, center), (TRAIN_WIDTH + 0.02, 0.18, length - 0.12), var=0)
    box(body, paint, (0, 2.75, center), (TRAIN_WIDTH + 0.02, 0.06, length - 0.12), shade=1, var=0)

    for z in (1.0, length - 1.0):
        box(body, paint, (0, 1.75, z), (TRAIN_WIDTH + 0.02, 2.1, 1.1), shade=1, var=0)
        box(body, "Glass", (0, 2.15, z - 0.27), (TRAIN_WIDTH + 0.04, 0.62, 0.42), var=0)
        box(body, "Glass", (0, 2.15, z + 0.27), (TRAIN_WIDTH + 0.04, 0.62, 0.42), var=0)
        box(body, "Iron", (0, 1.75, z), (TRAIN_WIDTH + 0.03, 2.1, 0.03), var=0)

    for z in (2.35, 3.65):
        box(body, "Glass", (0, 2.15, z), (TRAIN_WIDTH + 0.04, 0.72, 1.05), var=0)

    for z in (1.2, length - 1.2):
        box(body, "Iron", (0, 0.25, z), (1.9, 0.3, 1.5), var=0)
        for dz in (-0.45, 0.45):
            cyl(body, "Steel", (0, 0.3, z + dz), 0.28, 0.28, 2.06, seg=10, axis="x", shade=2, var=0)

    if front:
        box(body, "Glass", (0, 2.2, 0.03), (1.9, 0.85, 0.06), var=0)
        box(body, paint, (0, 2.2, 0.035), (0.08, 0.85, 0.07), shade=1, var=0)
        box(body, "GlowCyan", (0, 2.8, 0.035), (0.9, 0.18, 0.05), var=0)
        box(body, "Yellow", (0, 1.15, 0.04), (TRAIN_WIDTH - 0.1, 0.55, 0.04), var=0)
        box(body, "Iron", (0, 0.55, 0.0), (TRAIN_WIDTH - 0.1, 0.3, 0.2), var=0)
        box(body, "Iron", (0, 0.6, -0.1), (0.3, 0.25, 0.2), var=0)
        for side in (-1, 1):
            box(body, "Rubber", (side * 0.7, 0.9, 0.04), (0.5, 0.4, 0.05), var=0)
            box(body, "GlowRed", (side * 0.95, 1.55, 0.04), (0.14, 0.1, 0.05), var=0)
        box(body, "Iron", (0, 1.75, length - 0.02), (1.2, 2.2, 0.08), var=0)
    else:
        for z in (0.02, length - 0.02):
            box(body, "Iron", (0, 1.75, z), (1.2, 2.2, 0.08), var=0)
            box(body, "Iron", (0, 0.6, z), (0.3, 0.25, 0.3), var=0)

    if not front:
        return [body]

    lights = Part("Lights", (0, 0.9, 0), rng)
    for side in (-1, 1):
        cyl(lights, "GlowWhite", (side * 0.7, 0.9, -0.0), 0.17, 0.17, 0.06, seg=10, axis="z", var=0)
    return [body, lights]


def ramp(p, rng):
    angle = math.atan2(TRAIN_HEIGHT, RAMP_LENGTH)
    slope = math.hypot(RAMP_LENGTH, TRAIN_HEIGHT)
    half = TRAIN_WIDTH * 0.5

    prism(p, "Iron", [(0.0, 0.0), (RAMP_LENGTH, TRAIN_HEIGHT - 0.06), (RAMP_LENGTH, 0.0)], "x", -half + 0.08, half - 0.08, shade=2, var=0)
    box(p, "Steel", (0, TRAIN_HEIGHT * 0.5 - 0.03, RAMP_LENGTH * 0.5), (TRAIN_WIDTH, 0.06, slope), rot=(-angle, 0, 0), var=0)

    for index in range(14):
        t = (index + 0.6) / 14.5
        z = t * RAMP_LENGTH
        box(p, "Steel", (0, TRAIN_HEIGHT * t + 0.02, z), (TRAIN_WIDTH - 0.2, 0.03, 0.08), rot=(-angle, 0, 0), shade=5, var=0)

    for index in range(6):
        x = -half + (index + 0.5) * TRAIN_WIDTH / 6
        box(p, "Yellow" if index % 2 == 0 else "Rubber", (x, 0.1, 0.3), (TRAIN_WIDTH / 6, 0.03, 0.6), rot=(-angle, 0, 0), var=0)

    for side in (-1, 1):
        box(p, "Yellow", (side * (half - 0.04), TRAIN_HEIGHT * 0.5 + 0.06, RAMP_LENGTH * 0.5), (0.08, 0.12, slope), rot=(-angle, 0, 0), var=0)
        for z in (3.0, 6.0, RAMP_LENGTH - 0.15):
            height = TRAIN_HEIGHT * z / RAMP_LENGTH
            box(p, "Iron", (side * (half - 0.15), height * 0.5, z), (0.14, height, 0.14), shade=4, var=0)


def low_barrier(p, rng):
    half = TRAIN_WIDTH * 0.5
    blocks = 8
    for row, (y, offset) in enumerate(((0.78, 0), (0.42, 1))):
        for index in range(blocks):
            x = -half + (index + 0.5) * TRAIN_WIDTH / blocks
            family = "Red" if (index + offset) % 2 == 0 else "White"
            box(p, family, (x, y, 0.12), (TRAIN_WIDTH / blocks, 0.26, 0.06), var=0)
    for side in (-1, 1):
        for dz in (-0.12, 0.12):
            box(p, "Iron", (side * (half - 0.12), 0.48, 0.15 + dz * 1.6), (0.07, 1.0, 0.07), rot=(-dz * 1.6, 0, 0), shade=4, var=0)
        box(p, "Iron", (side * (half - 0.12), 0.06, 0.15), (0.1, 0.06, 0.6), var=0)
    cyl(p, "GlowRed", (half - 0.25, 1.0, 0.12), 0.06, 0.05, 0.1, seg=6, var=0)
    cyl(p, "GlowRed", (-half + 0.25, 1.0, 0.12), 0.06, 0.05, 0.1, seg=6, var=0)


def high_barrier(p, rng):
    half = TRAIN_WIDTH * 0.5
    bottom, top = 1.35, 2.25
    blocks = 9
    for index in range(blocks):
        x = -half + (index + 0.5) * TRAIN_WIDTH / blocks
        box(p, "Yellow" if index % 2 == 0 else "Rubber", (x, (bottom + top) * 0.5, 0.15), (TRAIN_WIDTH / blocks, top - bottom, 0.16), var=0)
    box(p, "Iron", (0, top + 0.03, 0.15), (TRAIN_WIDTH + 0.1, 0.06, 0.2), var=0)
    box(p, "Iron", (0, bottom - 0.03, 0.15), (TRAIN_WIDTH + 0.1, 0.06, 0.2), var=0)
    box(p, "White", (0, (bottom + top) * 0.5, 0.06), (0.9, 0.3, 0.02), var=0)
    box(p, "Rubber", (0, (bottom + top) * 0.5, 0.05), (0.6, 0.08, 0.02), var=0)
    for side in (-1, 1):
        box(p, "Steel", (side * half, top * 0.5, 0.15), (0.14, top, 0.14), shade=2)
        box(p, "Concrete", (side * half, 0.1, 0.15), (0.34, 0.2, 0.4), var=1)
        cyl(p, "GlowRed", (side * half, top + 0.1, 0.15), 0.07, 0.06, 0.12, seg=6, var=0)


def coin(p, rng):
    cyl(p, "GlowGold", (0, 0, 0), 0.3, 0.3, 0.07, seg=14, axis="z", var=0)
    cyl(p, "GlowGold", (0, 0, 0), 0.21, 0.21, 0.1, seg=14, axis="z", shade=6, var=0)
    box(p, "GlowGold", (0, 0, 0), (0.07, 0.22, 0.12), shade=1, var=0)


def magnet(p, rng):
    for index in range(7):
        a = math.pi + index / 6 * math.pi
        box(p, "GlowRed", (math.cos(a) * 0.22, math.sin(a) * 0.22, 0), (0.14, 0.13, 0.14), rot=(0, 0, a), var=0)
    for side in (-1, 1):
        box(p, "GlowRed", (side * 0.22, 0.08, 0), (0.14, 0.18, 0.14), var=0)
        box(p, "Steel", (side * 0.22, 0.24, 0), (0.15, 0.14, 0.15), shade=7, var=0)


def sneaker(p, rng):
    box(p, "White", (0, -0.16, 0), (0.28, 0.07, 0.56), shade=5, var=0)
    box(p, "GlowGreen", (0, -0.04, -0.04), (0.26, 0.18, 0.44), var=0)
    ico(p, "GlowGreen", (0, -0.08, 0.18), 0.14, scale=(1.0, 0.8, 1.1), flatten_below=-0.13, var=0)
    box(p, "GlowGreen", (0, 0.1, -0.16), (0.24, 0.14, 0.2), shade=5, var=0)
    for index in range(3):
        box(p, "White", (0, 0.06 - index * 0.02, 0.0 + index * 0.07), (0.16, 0.02, 0.025), rot=(0.4, 0, 0), shade=6, var=0)
    for side in (-1, 1):
        for index in range(3):
            length = 0.28 - index * 0.06
            box(p, "White", (side * 0.15, 0.06 + index * 0.06, -0.16 - length * 0.4), (0.02, 0.05, length), rot=(-0.5 - index * 0.25, 0, 0), shade=6, var=0)


def star(p, rng):
    outline = []
    for index in range(10):
        a = math.pi * 0.5 + index / 10 * math.tau
        r = 0.38 if index % 2 == 0 else 0.17
        outline.append((math.cos(a) * r, math.sin(a) * r))
    prism(p, "GlowGold", outline, "z", -0.05, 0.05, var=0)
    cyl(p, "GlowGold", (0, 0, 0), 0.13, 0.13, 0.14, seg=10, axis="z", shade=6, var=0)


def track(rng):
    p = Part("Main", (0, 0, 0), rng)
    box(p, "Gravel", (0, -0.27, SEGMENT_LENGTH * 0.5), (TRACK_HALF * 2, 0.5, SEGMENT_LENGTH), shade=1, var=0)

    def gravel(center):
        return ("Gravel", rng.randint(1, 5))

    grid(p, -TRACK_HALF, TRACK_HALF, 0.0, SEGMENT_LENGTH, 0.0, 22, 60, gravel, jitter=0.015)

    for lane in range(3):
        lane_x = (1 - lane) * LANE_WIDTH
        for tie in range(20):
            z = (tie + 0.5) * SEGMENT_LENGTH / 20
            box(p, "Wood", (lane_x + rng.uniform(-0.03, 0.03), 0.04, z), (2.0, 0.1, 0.3), rot=(0, rng.uniform(-0.03, 0.03), 0), var=2)
            for side in (-1, 1):
                box(p, "Iron", (lane_x + side * 0.72, 0.1, z), (0.26, 0.02, 0.32), var=0)
        for side in (-1, 1):
            x = lane_x + side * 0.72
            box(p, "Steel", (x, 0.125, SEGMENT_LENGTH * 0.5), (0.16, 0.03, SEGMENT_LENGTH), shade=1, var=0)
            box(p, "Steel", (x, 0.17, SEGMENT_LENGTH * 0.5), (0.05, 0.07, SEGMENT_LENGTH), shade=2, var=0)
            box(p, "Steel", (x, 0.22, SEGMENT_LENGTH * 0.5), (0.1, 0.04, SEGMENT_LENGTH), shade=6, var=0)
    return [p]


def platform(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    inner = side * TRACK_HALF
    outer = side * (TRACK_HALF + PLATFORM_WIDTH)
    center_x = (inner + outer) * 0.5

    box(p, "Concrete", (center_x, 0.29, SEGMENT_LENGTH * 0.5), (PLATFORM_WIDTH, 1.1, SEGMENT_LENGTH), shade=1, var=0)
    box(p, "Concrete", (inner - side * 0.04, 0.8, SEGMENT_LENGTH * 0.5), (0.12, 0.1, SEGMENT_LENGTH), shade=4, var=0)
    box(p, "Yellow", (inner + side * 0.3, PLATFORM_TOP + 0.006, SEGMENT_LENGTH * 0.5), (0.3, 0.012, SEGMENT_LENGTH), var=0)

    def tile(center):
        return ("Concrete", rng.randint(2, 5))

    x0, x1 = sorted((inner + side * 0.5, outer))
    grid(p, x0, x1, 0.0, SEGMENT_LENGTH, PLATFORM_TOP, 4, 30, tile, per_cell=True)

    for index in range(3):
        z = 5.0 + index * 10.0
        box(p, "Concrete", (inner + side * 0.02, 0.3, z), (0.08, 0.6, 1.2), shade=0, var=0)
    return [p]


def lamp_post(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    toward = -side
    box(p, "Concrete", (0, 0.1, 0), (0.4, 0.2, 0.4), var=0)
    cyl(p, "Iron", (0, 2.45, 0), 0.08, 0.06, 4.7, seg=8, shade=4, var=0)
    box(p, "Iron", (toward * 0.55, 4.75, 0), (1.25, 0.1, 0.1), shade=4, var=0)
    box(p, "Iron", (toward * 0.25, 4.5, 0), (0.6, 0.06, 0.06), rot=(0, 0, side * 0.6), shade=4, var=0)
    box(p, "Iron", (toward * 1.15, 4.65, 0), (0.5, 0.14, 0.32), var=0)
    box(p, "GlowWhite", (toward * 1.15, 4.57, 0), (0.42, 0.04, 0.26), var=0)
    box(p, "Navy", (side * 0.1, 3.0, 0), (0.06, 0.5, 0.9), var=0)
    box(p, "White", (side * 0.135, 3.0, 0), (0.01, 0.12, 0.6), var=0)
    return [p]


def building_shell(p, side, depth, height, family, z_half=BUILDING_LENGTH * 0.5):
    box(p, family, (side * depth * 0.5, height * 0.5, 0), (depth, height, z_half * 2), var=0)
    box(p, family, (side * (depth * 0.5 - 0.05), height + 0.25, 0), (depth + 0.2, 0.5, z_half * 2 + 0.2), shade=1, var=0)
    box(p, "Iron", (side * depth * 0.5, height + 0.02, 0), (depth - 0.4, 0.06, z_half * 2 - 0.4), var=0)


def window(p, rng, side, y, z, width, height, lit_chance=0.45, frame="White"):
    family = "GlowWindow" if rng.random() < lit_chance else "Glass"
    box(p, frame, (-side * 0.03, y, z), (0.06, height + 0.14, width + 0.14), shade=4, var=0)
    box(p, family, (-side * 0.06, y, z), (0.04, height, width), shade=rng.randint(2, 5), var=0)
    box(p, frame, (-side * 0.08, y - height * 0.5 - 0.06, z), (0.12, 0.06, width + 0.2), shade=4, var=0)


def apartment(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    floors, floor_height, depth = 4, 3.2, 7.0
    height = floors * floor_height + 0.6
    building_shell(p, side, depth, height, "Brick")
    for floor in range(floors):
        y = 1.9 + floor * floor_height
        for bay in range(5):
            z = -BUILDING_LENGTH * 0.5 + (bay + 0.5) * BUILDING_LENGTH / 5
            window(p, rng, side, y, z, 1.0, 1.5)
        if floor > 0:
            for z in (-3.6, 3.6):
                box(p, "Concrete", (-side * 0.45, y - 1.0, z), (0.9, 0.12, 2.6), var=0)
                box(p, "Iron", (-side * 0.88, y - 0.55, z), (0.04, 0.8, 2.6), var=0)
                for side_z in (-1, 1):
                    box(p, "Iron", (-side * 0.45, y - 0.55, z + side_z * 1.28), (0.9, 0.8, 0.04), var=0)
        box(p, "Brick", (-side * 0.04, y - 1.05, 0), (0.08, 0.14, BUILDING_LENGTH), shade=5, var=0)
    tower_x = side * depth * 0.55
    for dx in (-0.6, 0.6):
        for dz in (-0.6, 0.6):
            box(p, "Iron", (tower_x + dx, height + 1.1, 2.5 + dz), (0.08, 1.8, 0.08), var=0)
    cyl(p, "Wood", (tower_x, height + 2.8, 2.5), 0.9, 0.9, 1.8, seg=10, var=1)
    cyl(p, "Iron", (tower_x, height + 3.95, 2.5), 0.95, 0.0, 0.5, seg=10, var=0)
    for index in range(3):
        cyl(p, "Iron", (tower_x, height + 2.2 + index * 0.6, 2.5), 0.92, 0.92, 0.05, seg=10, var=0)
    return [p]


def tower(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    floors, floor_height, depth = 6, 3.3, 7.5
    height = floors * floor_height + 0.8
    building_shell(p, side, depth, height, "Teal")
    for floor in range(floors):
        y = 2.0 + floor * floor_height
        box(p, "Teal", (-side * 0.06, y - 1.2, 0), (0.12, 0.3, BUILDING_LENGTH), shade=5, var=0)
        for index in range(7):
            z = -BUILDING_LENGTH * 0.5 + (index + 0.5) * BUILDING_LENGTH / 7
            family = "GlowWindow" if rng.random() < 0.4 else "Glass"
            box(p, family, (-side * 0.05, y, z), (0.06, 1.7, BUILDING_LENGTH / 7 - 0.18), shade=rng.randint(2, 5), var=0)
            box(p, "Steel", (-side * 0.07, y, z + BUILDING_LENGTH / 14), (0.06, 1.8, 0.1), shade=5, var=0)
    for index in range(3):
        box(p, "Steel", (side * (2.0 + index * 1.6), height + 0.45, -3.5 + index * 2.5), (1.2, 0.9, 1.4), shade=4)
        box(p, "Iron", (side * (2.0 + index * 1.6) - side * 0.61, height + 0.45, -3.5 + index * 2.5), (0.02, 0.6, 0.6), var=0)
    cyl(p, "Iron", (side * 4.5, height + 2.5, 4.0), 0.06, 0.03, 5.0, seg=6, var=0)
    ico(p, "GlowRed", (side * 4.5, height + 5.05, 4.0), 0.12, var=0)
    return [p]


def shop(rng, side, awning):
    p = Part("Main", (0, 0, 0), rng)
    depth, height = 6.0, 7.2
    building_shell(p, side, depth, height, "Cream")
    box(p, "Concrete", (-side * 0.05, 0.3, 0), (0.1, 0.6, BUILDING_LENGTH), shade=2, var=0)
    for z in (-3.8, 3.8):
        box(p, "GlowWindow", (-side * 0.04, 1.7, z), (0.06, 2.0, 4.8), shade=3, var=0)
        box(p, "Iron", (-side * 0.07, 1.7, z), (0.04, 2.1, 0.08), var=0)
        box(p, "Iron", (-side * 0.06, 2.75, z), (0.08, 0.1, 5.0), var=0)
    box(p, "Wood", (-side * 0.04, 1.3, 0), (0.08, 2.6, 1.4), shade=2, var=0)
    box(p, "Gold", (-side * 0.09, 1.3, 0.5), (0.03, 0.1, 0.1), var=0)
    stripes = 12
    for index in range(stripes):
        z = -BUILDING_LENGTH * 0.5 + 0.3 + (index + 0.5) * (BUILDING_LENGTH - 0.6) / stripes
        family = awning if index % 2 == 0 else "White"
        box(p, family, (-side * 0.6, 3.15, z), (1.3, 0.05, (BUILDING_LENGTH - 0.6) / stripes), rot=(0, 0, side * 0.35), var=0)
    box(p, awning, (-side * 1.2, 2.92, 0), (0.05, 0.25, BUILDING_LENGTH - 0.6), var=0)
    box(p, "Iron", (-side * 0.08, 3.9, 0), (0.12, 0.7, 6.0), var=0)
    box(p, "GlowCyan", (-side * 0.15, 3.9, 0), (0.04, 0.4, 5.2), var=0)
    for index in range(4):
        z = -BUILDING_LENGTH * 0.5 + (index + 0.5) * BUILDING_LENGTH / 4
        window(p, rng, side, 5.4, z, 1.3, 1.4, lit_chance=0.5)
    return [p]


def warehouse(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    depth, height = 8.0, 8.5
    building_shell(p, side, depth, height, "Orange")
    for z in (-3.4, 3.4):
        box(p, "Steel", (-side * 0.04, 2.0, z), (0.08, 4.0, 4.2), shade=3, var=0)
        for index in range(10):
            box(p, "Steel", (-side * 0.09, 0.2 + index * 0.4, z), (0.04, 0.05, 4.2), shade=1, var=0)
        box(p, "Yellow", (-side * 0.06, 4.1, z), (0.1, 0.16, 4.4), var=0)
        box(p, "GlowWhite", (-side * 0.25, 4.6, z), (0.3, 0.1, 0.5), var=0)
    for index in range(6):
        z = -BUILDING_LENGTH * 0.5 + (index + 0.5) * BUILDING_LENGTH / 6
        window(p, rng, side, 6.6, z, 1.6, 0.8, lit_chance=0.3, frame="Iron")
    box(p, "Orange", (-side * 0.05, 5.3, 0), (0.1, 0.2, BUILDING_LENGTH), shade=5, var=0)
    for index in range(4):
        z = -BUILDING_LENGTH * 0.5 + 1.8 + index * 3.6
        prism(p, "Iron", [(z - 1.6, height), (z + 1.6, height), (z + 1.6, height + 1.4)], "x", side * 0.5, side * (depth - 0.5), shade=2, var=0)
        box(p, "Glass", (side * depth * 0.5, height + 0.7, z + 1.6), (depth - 1.2, 1.3, 0.06), var=0)
    return [p]


def office(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    depth, height = 7.0, 16.5
    building_shell(p, side, depth, height, "Concrete")
    columns = 8
    for index in range(columns):
        z = -BUILDING_LENGTH * 0.5 + (index + 0.5) * BUILDING_LENGTH / columns
        for floor in range(5):
            family = "GlowWindow" if rng.random() < 0.35 else "Glass"
            box(p, family, (-side * 0.04, 1.9 + floor * 3.2, z), (0.06, 2.6, BUILDING_LENGTH / columns - 0.5), shade=rng.randint(2, 5), var=0)
        box(p, "Concrete", (-side * 0.12, height * 0.5, z + BUILDING_LENGTH / columns * 0.5), (0.24, height, 0.3), shade=5, var=0)
    box(p, "Concrete", (-side * 0.15, 0.45, 0), (0.3, 0.9, BUILDING_LENGTH), shade=2, var=0)
    box(p, "GlowWhite", (-side * 0.3, 15.6, 0), (0.06, 0.5, 4.0), var=0)
    box(p, "Navy", (-side * 0.27, 15.6, 0), (0.04, 0.8, 4.6), var=0)
    return [p]


def brownstone(rng, side):
    p = Part("Main", (0, 0, 0), rng)
    depth, height = 6.5, 10.6
    building_shell(p, side, depth, height, "Red")
    box(p, "Red", (side * depth * 0.5, height * 0.5, 0), (depth, height, BUILDING_LENGTH - 0.02), shade=1, var=0)
    for floor in range(3):
        y = 2.0 + floor * 3.3
        for bay in range(4):
            z = -BUILDING_LENGTH * 0.5 + (bay + 0.5) * BUILDING_LENGTH / 4
            if floor == 0 and bay in (1, 2):
                continue
            window(p, rng, side, y, z, 1.1, 1.7, frame="Cream")
            box(p, "Cream", (-side * 0.08, y + 1.02, z), (0.16, 0.18, 1.5), var=0)
    for z in (-BUILDING_LENGTH / 8, BUILDING_LENGTH / 8):
        box(p, "Wood", (-side * 0.04, 1.25, z), (0.08, 2.5, 1.1), shade=2, var=0)
        box(p, "GlowWindow", (-side * 0.06, 2.75, z), (0.06, 0.3, 0.9), var=0)
        for step in range(4):
            box(p, "Concrete", (-side * (0.3 + step * 0.3), 0.6 - step * 0.15, z), (0.3, 0.15, 1.5), shade=3, var=0)
        for side_z in (-1, 1):
            box(p, "Iron", (-side * 0.75, 0.9, z + side_z * 0.78), (1.2, 0.04, 0.04), rot=(0, 0, side * 0.4), var=0)
    box(p, "Cream", (-side * 0.2, height - 0.2, 0), (0.4, 0.3, BUILDING_LENGTH + 0.2), var=0)
    box(p, "Cream", (-side * 0.1, height - 0.5, 0), (0.2, 0.15, BUILDING_LENGTH), var=0)
    return [p]


def bench(p, rng):
    for index in range(3):
        box(p, "Wood", (0, 0.45, -0.12 + index * 0.13), (1.8, 0.05, 0.11), var=1)
    for index in range(2):
        box(p, "Wood", (0, 0.68 + index * 0.16, -0.27), (1.8, 0.11, 0.04), rot=(0.15, 0, 0), var=1)
    for side in (-1, 1):
        box(p, "Iron", (side * 0.75, 0.22, 0.0), (0.06, 0.44, 0.44), var=0)
        box(p, "Iron", (side * 0.75, 0.6, -0.28), (0.06, 0.6, 0.05), rot=(0.15, 0, 0), var=0)


def trash_can(p, rng):
    cyl(p, "Green", (0, 0.45, 0), 0.26, 0.24, 0.9, seg=10)
    cyl(p, "Iron", (0, 0.93, 0), 0.28, 0.2, 0.08, seg=10, var=0)
    cyl(p, "Iron", (0, 0.6, 0), 0.265, 0.265, 0.05, seg=10, var=0)
    box(p, "White", (0, 0.45, 0.25), (0.14, 0.14, 0.02), var=0)


def vending(p, rng):
    box(p, "Red", (0, 0.95, 0), (0.9, 1.9, 0.75))
    box(p, "GlowWindow", (-0.1, 1.2, 0.38), (0.55, 1.1, 0.02), shade=5, var=0)
    for row in range(4):
        for column in range(3):
            family = ("GlowCyan", "GlowRed", "GlowGreen")[(row + column) % 3]
            box(p, family, (-0.28 + column * 0.18, 0.8 + row * 0.26, 0.39), (0.1, 0.14, 0.02), var=0)
    box(p, "Iron", (0.3, 1.3, 0.385), (0.16, 0.5, 0.02), var=0)
    box(p, "Iron", (0, 0.35, 0.385), (0.6, 0.18, 0.02), var=0)
    box(p, "White", (0, 1.8, 0.385), (0.8, 0.14, 0.02), var=0)


def sign(p, rng):
    for side in (-1, 1):
        box(p, "Iron", (side * 0.9, 1.2, 0), (0.08, 2.4, 0.08), var=0)
    box(p, "Navy", (0, 2.2, 0), (2.1, 0.6, 0.08), var=0)
    box(p, "White", (-0.2, 2.2, 0.045), (1.2, 0.12, 0.01), var=0)
    cyl(p, "White", (0.75, 2.2, 0.04), 0.18, 0.18, 0.02, seg=12, axis="z", var=0)
    box(p, "Red", (0.75, 2.2, 0.055), (0.3, 0.06, 0.01), var=0)
    box(p, "Iron", (0, 1.0, 0), (1.6, 1.1, 0.06), var=0)
    box(p, "Cream", (0, 1.0, 0.035), (1.45, 0.95, 0.01), var=0)
    for index in range(5):
        family = ("Red", "Cap", "Green", "Yellow", "Orange")[index]
        box(p, family, (-0.55 + index * 0.27, 1.0 + (index % 2) * 0.12, 0.045), (0.24, 0.04, 0.01), var=0)


def planter(p, rng):
    box(p, "Concrete", (0, 0.3, 0), (1.2, 0.6, 0.6), shade=4)
    box(p, "Gravel", (0, 0.58, 0), (1.1, 0.04, 0.5), var=0)
    for index in range(3):
        ico(p, "Green", (-0.35 + index * 0.35, 0.8, rng.uniform(-0.05, 0.05)), 0.3, jitter=0.2, seed=index, var=2)


ASSETS = [
    ("Runner", runner),
    ("Guard", guard),
    ("Ramp", lambda r: single(r, ramp)),
    ("LowBarrier", lambda r: single(r, low_barrier)),
    ("HighBarrier", lambda r: single(r, high_barrier)),
    ("Coin", lambda r: single(r, coin)),
    ("Magnet", lambda r: single(r, magnet)),
    ("Sneaker", lambda r: single(r, sneaker)),
    ("Multiplier", lambda r: single(r, star)),
    ("Bench", lambda r: single(r, bench)),
    ("TrashCan", lambda r: single(r, trash_can)),
    ("Vending", lambda r: single(r, vending)),
    ("Sign", lambda r: single(r, sign)),
    ("Planter", lambda r: single(r, planter)),
    ("Track", track),
]

for color, paint in TRAIN_COLORS:
    ASSETS.append((f"TrainFront{color}", lambda r, paint=paint: train_unit(r, paint, True)))
    ASSETS.append((f"TrainCar{color}", lambda r, paint=paint: train_unit(r, paint, False)))

for suffix, side in (("L", 1), ("R", -1)):
    ASSETS.append((f"Platform{suffix}", lambda r, side=side: platform(r, side)))
    ASSETS.append((f"LampPost{suffix}", lambda r, side=side: lamp_post(r, side)))
    ASSETS.append((f"Apartment{suffix}", lambda r, side=side: apartment(r, side)))
    ASSETS.append((f"Tower{suffix}", lambda r, side=side: tower(r, side)))
    ASSETS.append((f"ShopRed{suffix}", lambda r, side=side: shop(r, side, "Red")))
    ASSETS.append((f"ShopGreen{suffix}", lambda r, side=side: shop(r, side, "Green")))
    ASSETS.append((f"Warehouse{suffix}", lambda r, side=side: warehouse(r, side)))
    ASSETS.append((f"Office{suffix}", lambda r, side=side: office(r, side)))
    ASSETS.append((f"Brownstone{suffix}", lambda r, side=side: brownstone(r, side)))


def main():
    output = Path(sys.argv[sys.argv.index("--") + 1]) if "--" in sys.argv else Path(__file__).with_name("RailRush.glb")

    bpy.ops.wm.read_factory_settings(use_empty=True)

    write_palettes(output.parent)

    material = bpy.data.materials.new("Palette")

    for index, (name, function) in enumerate(ASSETS):
        rng = random.Random(2000 + index)
        offset = ((index % 8) * 40.0, (index // 8) * 40.0, 0.0)
        build_asset(name, function(rng), offset, material)

    bpy.ops.wm.save_as_mainfile(filepath=str(output.with_suffix(".blend")))
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True, export_apply=True, export_normals=True, export_texcoords=True, export_materials="EXPORT", export_cameras=False, export_lights=False)


main()
