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
    ("Cloak",      (0.12, 0.19, 0.46), False),
    ("Skin",       (0.86, 0.66, 0.52), False),
    ("Gold",       (0.95, 0.70, 0.22), False),
    ("Leather",    (0.15, 0.10, 0.07), False),
    ("Wood",       (0.30, 0.19, 0.11), False),
    ("GhoulSkin",  (0.34, 0.50, 0.30), False),
    ("Rags",       (0.24, 0.19, 0.15), False),
    ("BatFur",     (0.38, 0.20, 0.50), False),
    ("Membrane",   (0.24, 0.10, 0.30), False),
    ("BruteSkin",  (0.62, 0.15, 0.11), False),
    ("Hide",       (0.42, 0.29, 0.16), False),
    ("Bone",       (0.86, 0.82, 0.70), False),
    ("Stone",      (0.44, 0.46, 0.50), False),
    ("Iron",       (0.13, 0.13, 0.15), False),
    ("Grass",      (0.20, 0.33, 0.14), False),
    ("Dirt",       (0.34, 0.24, 0.15), False),
    ("Moss",       (0.27, 0.40, 0.18), False),
    ("Bloat",      (0.52, 0.58, 0.26), False),
    ("Wraith",     (0.18, 0.22, 0.30), False),
    ("LichRobe",   (0.20, 0.09, 0.27), False),
    ("Red",        (0.72, 0.10, 0.10), False),
    ("Autumn",     (0.62, 0.32, 0.10), False),
    ("GlowCyan",   (0.35, 0.85, 1.00), True),
    ("GlowGreen",  (0.55, 1.00, 0.30), True),
    ("GlowRed",    (1.00, 0.20, 0.14), True),
    ("GlowOrange", (1.00, 0.52, 0.16), True),
    ("GlowYellow", (1.00, 0.85, 0.30), True),
    ("GlowPurple", (0.72, 0.32, 1.00), True),
    ("GlowWhite",  (1.00, 1.00, 1.00), True),
    ("GlowPink",   (1.00, 0.38, 0.62), True),
    ("Steel",      (0.56, 0.60, 0.68), False),
]

FAMILY_INDEX = {name: index for index, (name, _, _) in enumerate(FAMILIES)}


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
            emit = linear if glow else [0.0, 0.0, 0.0]

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
        self.recalc = True
        self.uv_tile = None

    def tag(self, verts, family, shade, variation):
        faces = {face for vert in verts for face in vert.link_faces}
        for face in faces:
            face[self.family] = FAMILY_INDEX[family]
            value = shade + (self.rng.randint(-variation, variation) if variation else 0)
            face[self.shade] = max(0, min(len(SHADES) - 1, value))
        return verts


def matrix(loc, rot, scale):
    return Matrix.LocRotScale(Vector(loc), Euler(rot), Vector(scale))


def box(p, family, loc, size, rot=(0, 0, 0), shade=3, var=1):
    result = bmesh.ops.create_cube(p.bm, size=1.0, matrix=matrix(loc, rot, size))
    return p.tag(result["verts"], family, shade, var)


def cyl(p, family, loc, r1, r2, depth, seg=6, rot=(0, 0, 0), scale=(1, 1, 1), shade=3, var=1):
    result = bmesh.ops.create_cone(p.bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=depth, matrix=matrix(loc, rot, scale))
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
            vert.co.z = max(vert.co.z, flatten_below)
    return p.tag(verts, family, shade, var)


def sphere(p, family, loc, radius, seg=8, rings=6, scale=(1, 1, 1), rot=(0, 0, 0), shade=3, var=1):
    result = bmesh.ops.create_uvsphere(p.bm, u_segments=seg, v_segments=rings, radius=radius, matrix=matrix(loc, rot, scale))
    return p.tag(result["verts"], family, shade, var)


def slab(p, family, outline, thickness, loc=(0, 0, 0), rot=(0, 0, 0), ridge=0.0, shade=3, var=1):
    transform = matrix(loc, rot, (1, 1, 1))
    cx = sum(x for x, _ in outline) / len(outline)
    cy = sum(y for _, y in outline) / len(outline)
    top = p.bm.verts.new(transform @ Vector((cx, cy, thickness * 0.5 + ridge)))
    bottom = p.bm.verts.new(transform @ Vector((cx, cy, -thickness * 0.5 - ridge)))
    upper = [p.bm.verts.new(transform @ Vector((x, y, thickness * 0.5))) for x, y in outline]
    lower = [p.bm.verts.new(transform @ Vector((x, y, -thickness * 0.5))) for x, y in outline]
    count = len(outline)
    for i in range(count):
        j = (i + 1) % count
        p.bm.faces.new((upper[i], upper[j], top))
        p.bm.faces.new((lower[j], lower[i], bottom))
        p.bm.faces.new((lower[i], lower[j], upper[j], upper[i]))
    return p.tag(upper + lower + [top, bottom], family, shade, var)


def bipyramid(p, family, ring_points, apex_top, apex_bottom, shade=3, var=0):
    ring = [p.bm.verts.new(Vector(point)) for point in ring_points]
    top = p.bm.verts.new(Vector(apex_top))
    bottom = p.bm.verts.new(Vector(apex_bottom))
    count = len(ring)
    for i in range(count):
        j = (i + 1) % count
        p.bm.faces.new((ring[i], ring[j], top))
        p.bm.faces.new((ring[j], ring[i], bottom))
    return p.tag(ring + [top, bottom], family, shade, var)


def torus(p, family, loc, major, minor, seg=20, seg_minor=4, rot=(0, 0, 0), shade=3, var=0):
    transform = matrix(loc, rot, (1, 1, 1))
    rings = []
    for i in range(seg):
        a = i / seg * math.tau
        center = Vector((math.cos(a) * major, math.sin(a) * major, 0.0))
        outward = Vector((math.cos(a), math.sin(a), 0.0))
        ring = []
        for j in range(seg_minor):
            b = j / seg_minor * math.tau + math.pi / seg_minor
            offset = outward * math.cos(b) * minor + Vector((0, 0, math.sin(b) * minor))
            ring.append(p.bm.verts.new(transform @ (center + offset)))
        rings.append(ring)
    for i in range(seg):
        a, b = rings[i], rings[(i + 1) % seg]
        for j in range(seg_minor):
            k = (j + 1) % seg_minor
            p.bm.faces.new((a[j], b[j], b[k], a[k]))
    return p.tag([v for ring in rings for v in ring], family, shade, var)


def look_rot(direction):
    return Vector(direction).normalized().to_track_quat("Z", "Y").to_euler()


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
        if part.joint.length < 1e-4:
            part.joint = Vector((0.0, 0.0, 0.001))

        bm = part.bm
        if part.recalc:
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bmesh.ops.translate(bm, vec=-part.joint, verts=bm.verts)
        bm.normal_update()

        uv_layer = bm.loops.layers.uv.new("UVMap")
        for face in bm.faces:
            if part.uv_tile:
                for loop in face.loops:
                    loop[uv_layer].uv = (loop.vert.co.x / part.uv_tile, loop.vert.co.y / part.uv_tile)
            else:
                assign_uv(face, uv_layer, face[part.family], face[part.shade])

        mesh = bpy.data.meshes.new(f"{name}_{part.name}")
        bm.to_mesh(mesh)
        bm.free()

        for polygon in mesh.polygons:
            polygon.use_smooth = part.uv_tile is not None

        mesh.materials.append(material)

        obj = bpy.data.objects.new(f"{name}_{part.name}", mesh)
        obj.location = part.joint
        obj.parent = empty
        bpy.context.scene.collection.objects.link(obj)


def hero(rng):
    body, head = Part("Body", (0, 0, 0.78), rng), Part("Head", (0, 0, 1.22), rng)
    arm_l, arm_r = Part("ArmL", (-0.31, 0, 1.14), rng), Part("ArmR", (0.31, 0, 1.14), rng)
    leg_l, leg_r = Part("LegL", (-0.11, 0, 0.42), rng), Part("LegR", (0.11, 0, 0.42), rng)

    cyl(body, "Cloak", (0, 0, 0.62), 0.33, 0.25, 0.5, seg=8, var=0)
    cyl(body, "Gold", (0, 0, 0.385), 0.335, 0.335, 0.04, seg=8, var=0)
    cyl(body, "Cloak", (0, 0, 1.0), 0.24, 0.31, 0.36, seg=4, rot=(0, 0, math.pi * 0.25), scale=(1.0, 0.72, 1.0), var=0)
    box(body, "Leather", (0, 0, 0.84), (0.5, 0.36, 0.07), var=0)
    box(body, "Gold", (0, 0.18, 0.84), (0.09, 0.03, 0.08), var=0)
    box(body, "Gold", (0, 0.17, 1.02), (0.06, 0.02, 0.28), var=0)
    box(body, "Gold", (0, 0.15, 0.62), (0.06, 0.02, 0.4), rot=(-0.13, 0, 0), var=0)
    box(body, "Cloak", (0, 0, 1.2), (0.62, 0.34, 0.08), shade=4, var=0)
    box(body, "Gold", (0, 0.04, 1.2), (0.3, 0.3, 0.1), var=0)

    box(head, "Skin", (0, 0.02, 1.38), (0.25, 0.25, 0.26), var=0)
    box(head, "Skin", (0, 0.05, 1.27), (0.21, 0.2, 0.08), shade=2, var=0)
    box(head, "Skin", (0, 0.16, 1.37), (0.05, 0.05, 0.07), shade=4, var=0)
    for side in (-1, 1):
        box(head, "Iron", (side * 0.06, 0.145, 1.42), (0.04, 0.02, 0.03), shade=0, var=0)
        box(head, "Bone", (side * 0.06, 0.15, 1.46), (0.07, 0.03, 0.025), shade=6, var=0)
        box(head, "Skin", (side * 0.13, 0.02, 1.39), (0.03, 0.07, 0.08), shade=2, var=0)
    box(head, "Bone", (0, 0.12, 1.29), (0.24, 0.1, 0.12), shade=6, var=0)
    cyl(head, "Bone", (0, 0.15, 1.12), 0.13, 0.02, 0.32, seg=4, rot=(math.pi + 0.15, 0, math.pi * 0.25), scale=(1.0, 0.6, 1.0), shade=6, var=0)
    box(head, "Bone", (0, 0.17, 1.33), (0.1, 0.03, 0.025), shade=7, var=0)
    cyl(head, "Cloak", (0, 0, 1.52), 0.33, 0.3, 0.035, seg=10, var=0)
    cyl(head, "Gold", (0, 0, 1.56), 0.17, 0.165, 0.05, seg=8, var=0)
    cyl(head, "Cloak", (0, -0.02, 1.76), 0.16, 0.06, 0.38, seg=8, rot=(-0.12, 0, 0), var=0)
    cyl(head, "Cloak", (0, -0.07, 2.0), 0.062, 0.0, 0.2, seg=6, rot=(-0.55, 0, 0), shade=4, var=0)
    box(head, "Gold", (0, 0.1, 1.72), (0.05, 0.02, 0.05), rot=(0.12, 0, math.pi * 0.25), var=0)

    for part, side in ((arm_l, -1), (arm_r, 1)):
        box(part, "Cloak", (side * 0.35, 0.0, 1.0), (0.15, 0.17, 0.3), rot=(0.05, side * 0.08, 0), var=0)
        cyl(part, "Cloak", (side * 0.36, 0.03, 0.8), 0.09, 0.15, 0.2, seg=6, var=0)
        cyl(part, "Gold", (side * 0.36, 0.03, 0.695), 0.153, 0.153, 0.03, seg=6, var=0)
        box(part, "Skin", (side * 0.36, 0.05, 0.63), (0.12, 0.12, 0.12), var=0)
        box(part, "Skin", (side * 0.36, 0.11, 0.62), (0.05, 0.05, 0.08), shade=2, var=0)

    cyl(arm_r, "Wood", (0.38, 0.12, 0.86), 0.035, 0.03, 1.7, seg=5)
    for z in (0.4, 0.62):
        cyl(arm_r, "Leather", (0.38, 0.12, z), 0.042, 0.042, 0.05, seg=5, var=0)
    cyl(arm_r, "Gold", (0.38, 0.12, 1.62), 0.06, 0.06, 0.06, seg=5, var=0)
    ico(arm_r, "GlowCyan", (0.38, 0.12, 1.75), 0.1, var=0)
    for index in range(3):
        a = index / 3 * math.tau
        cyl(arm_r, "Gold", (0.38 + math.cos(a) * 0.07, 0.12 + math.sin(a) * 0.07, 1.74), 0.02, 0.0, 0.2, seg=3, rot=(math.sin(a) * -0.4, math.cos(a) * 0.4, 0), var=0)

    for part, side in ((leg_l, -1), (leg_r, 1)):
        box(part, "Cloak", (side * 0.11, 0, 0.3), (0.15, 0.16, 0.26), shade=1, var=0)
        box(part, "Leather", (side * 0.11, 0.0, 0.14), (0.17, 0.2, 0.14), var=0)
        box(part, "Leather", (side * 0.11, 0.07, 0.05), (0.17, 0.3, 0.1), shade=2, var=0)
        box(part, "Leather", (side * 0.11, 0.0, 0.215), (0.19, 0.22, 0.04), shade=5, var=0)

    return [body, head, arm_l, arm_r, leg_l, leg_r]


def ghoul(rng):
    body, head = Part("Body", (0, 0, 0.55), rng), Part("Head", (0, 0.2, 0.98), rng)
    arm_l, arm_r = Part("ArmL", (-0.27, 0.14, 0.98), rng), Part("ArmR", (0.27, 0.14, 0.98), rng)
    leg_l, leg_r = Part("LegL", (-0.13, 0, 0.5), rng), Part("LegR", (0.13, 0, 0.5), rng)

    box(body, "Rags", (0, 0.06, 0.74), (0.48, 0.3, 0.52), rot=(-0.35, 0, 0))
    box(body, "GhoulSkin", (0, 0.16, 0.9), (0.4, 0.26, 0.2), rot=(-0.35, 0, 0))
    for index, z in enumerate((0.8, 0.95)):
        ico(body, "Bone", (0, -0.12 + index * 0.02, z), 0.05, var=0)
    for index, (x, z) in enumerate(((-0.2, 0.5), (0.15, 0.48), (0.0, 0.46))):
        cyl(body, "Rags", (x, 0.02, z), 0.08, 0.0, 0.18, seg=3, rot=(math.pi, 0, index), shade=2)

    box(head, "GhoulSkin", (0, 0.3, 1.07), (0.3, 0.3, 0.28), rot=(0.1, 0, 0.08))
    box(head, "GhoulSkin", (0, 0.38, 0.92), (0.24, 0.2, 0.08), rot=(0.25, 0, 0), shade=2)
    for side in (-1, 1):
        box(head, "GlowGreen", (side * 0.07, 0.45, 1.1), (0.07, 0.03, 0.05), var=0)
        cyl(head, "Bone", (side * 0.05, 0.47, 0.95), 0.015, 0.0, 0.06, seg=3, rot=(math.pi, 0, 0), var=0)

    for part, side in ((arm_l, -1), (arm_r, 1)):
        box(part, "GhoulSkin", (side * 0.29, 0.38, 0.88), (0.1, 0.5, 0.1), rot=(-0.25, 0, side * 0.12))
        for finger in (-1, 0, 1):
            cyl(part, "Bone", (side * 0.29 + finger * 0.03, 0.66, 0.79), 0.018, 0.0, 0.12, seg=3, rot=(-math.pi * 0.5 - 0.4, 0, 0), var=0)

    for part, side in ((leg_l, -1), (leg_r, 1)):
        box(part, "Rags", (side * 0.13, 0, 0.25), (0.14, 0.16, 0.5), rot=(side * 0.08, 0, 0))
        box(part, "GhoulSkin", (side * 0.13, 0.05, 0.03), (0.15, 0.22, 0.06))

    return [body, head, arm_l, arm_r, leg_l, leg_r]


def bat(rng):
    body = Part("Body", (0, 0, 0), rng)
    wing_l, wing_r = Part("WingL", (-0.12, 0, 0), rng), Part("WingR", (0.12, 0, 0), rng)

    ico(body, "BatFur", (0, 0, 0), 0.17, scale=(1.0, 1.25, 1.0))
    ico(body, "BatFur", (0, 0.2, 0.06), 0.12, shade=4)
    for side in (-1, 1):
        cyl(body, "BatFur", (side * 0.07, 0.19, 0.2), 0.05, 0.0, 0.15, seg=4, rot=(0, side * 0.3, 0))
        box(body, "GlowRed", (side * 0.05, 0.31, 0.08), (0.045, 0.02, 0.04), var=0)
        cyl(body, "Bone", (side * 0.025, 0.3, 0.0), 0.012, 0.0, 0.06, seg=3, rot=(math.pi, 0, 0), var=0)
        cyl(body, "BatFur", (side * 0.06, -0.08, -0.16), 0.03, 0.0, 0.12, seg=3, rot=(math.pi, 0, 0), shade=1)

    outline = [(0.0, 0.1), (0.3, 0.14), (0.68, 0.02), (0.5, -0.12), (0.36, -0.06), (0.24, -0.17), (0.12, -0.08), (0.0, -0.1)]
    for part, side in ((wing_l, -1), (wing_r, 1)):
        slab(part, "Membrane", [(side * (0.12 + x), y) for x, y in outline], 0.025)
        cyl(part, "BatFur", (side * 0.46, 0.11, 0.0), 0.02, 0.012, 0.68, seg=4, rot=(0, math.pi * 0.5, side * -0.1), shade=2)

    return [body, wing_l, wing_r]


def skeleton(rng):
    body, head = Part("Body", (0, 0, 0.6), rng), Part("Head", (0, 0, 1.2), rng)
    arm_l, arm_r = Part("ArmL", (-0.19, 0, 1.12), rng), Part("ArmR", (0.19, 0, 1.12), rng)
    leg_l, leg_r = Part("LegL", (-0.09, 0, 0.58), rng), Part("LegR", (0.09, 0, 0.58), rng)

    box(body, "Bone", (0, 0, 0.62), (0.3, 0.16, 0.1))
    cyl(body, "Bone", (0, -0.02, 0.8), 0.035, 0.035, 0.4, seg=5, shade=2)
    ico(body, "Bone", (0, 0.02, 1.0), 0.19, scale=(1.1, 0.75, 1.0))
    for z in (0.94, 1.04):
        box(body, "Iron", (0, 0.13, z), (0.28, 0.03, 0.025), var=0, shade=1)
    box(body, "Rags", (0, 0.02, 0.5), (0.28, 0.18, 0.22), shade=2)
    cyl(body, "Leather", (0.1, -0.16, 1.0), 0.06, 0.06, 0.45, seg=6, rot=(0.3, 0.25, 0))
    for index in range(3):
        cyl(body, "Wood", (0.07 + index * 0.03, -0.2, 1.28), 0.008, 0.008, 0.2, seg=3, rot=(0.3, 0.25, 0), var=0)
        box(body, "Red", (0.07 + index * 0.03, -0.24, 1.38), (0.02, 0.05, 0.06), rot=(0.3, 0.25, 0), var=0)

    ico(head, "Bone", (0, 0.02, 1.33), 0.13, scale=(1.0, 1.1, 1.05))
    box(head, "Bone", (0, 0.07, 1.2), (0.14, 0.12, 0.06), shade=2)
    cyl(head, "Iron", (0, 0, 1.43), 0.15, 0.11, 0.08, seg=6, shade=2)
    for side in (-1, 1):
        box(head, "Iron", (side * 0.045, 0.12, 1.33), (0.06, 0.04, 0.05), var=0, shade=0)
        box(head, "GlowRed", (side * 0.045, 0.135, 1.33), (0.03, 0.02, 0.025), var=0)

    box(arm_l, "Bone", (-0.22, 0.15, 1.1), (0.05, 0.32, 0.05), rot=(0.1, 0, 0.1))
    for index in range(7):
        t = (index - 3) / 3.0
        a = t * 1.0
        cyl(arm_l, "Wood", (-0.25, 0.34 - (1 - math.cos(a)) * 0.2, 1.1 + math.sin(a) * 0.38), 0.018, 0.018, 0.13, seg=4, rot=(-a, 0, 0), var=0)
    box(arm_l, "Bone", (-0.25, 0.25, 1.1), (0.008, 0.008, 0.68), shade=7, var=0)

    box(arm_r, "Bone", (0.2, 0.1, 1.1), (0.05, 0.26, 0.05), rot=(0.1, 0, -0.2))
    ico(arm_r, "Bone", (0.22, 0.24, 1.1), 0.04, var=0)

    for part, side in ((leg_l, -1), (leg_r, 1)):
        box(part, "Bone", (side * 0.09, 0, 0.3), (0.05, 0.05, 0.55))
        ico(part, "Bone", (side * 0.09, 0.02, 0.3), 0.04, var=0)
        box(part, "Bone", (side * 0.09, 0.05, 0.02), (0.08, 0.15, 0.04))

    return [body, head, arm_l, arm_r, leg_l, leg_r]


def bloater(rng):
    body, head = Part("Body", (0, 0, 0.6), rng), Part("Head", (0, 0.12, 1.2), rng)
    arm_l, arm_r = Part("ArmL", (-0.5, 0, 0.95), rng), Part("ArmR", (0.5, 0, 0.95), rng)
    leg_l, leg_r = Part("LegL", (-0.22, 0, 0.3), rng), Part("LegR", (0.22, 0, 0.3), rng)

    ico(body, "Bloat", (0, 0.05, 0.72), 0.55, scale=(1.0, 0.95, 0.9), jitter=0.06, seed=3, var=2)
    box(body, "Rags", (0, 0.0, 0.33), (0.8, 0.62, 0.14), shade=2)
    for index in range(7):
        a = index * 2.1
        z = 0.55 + (index % 3) * 0.17
        r = 0.5 * math.sqrt(max(0.0, 1.0 - ((z - 0.72) / 0.5) ** 2))
        ico(body, "GlowGreen", (math.cos(a) * r, 0.05 + math.sin(a) * r * 0.95, z), 0.065 + 0.02 * (index % 2), var=0)
    for index in range(4):
        box(body, "Iron", (-0.15 + index * 0.1, 0.52, 0.72), (0.02, 0.05, 0.12), rot=(0, 0.3, 0), var=0)

    ico(head, "Bloat", (0, 0.18, 1.3), 0.17, shade=2)
    for side in (-1, 1):
        box(head, "GlowGreen", (side * 0.06, 0.33, 1.33), (0.05, 0.02, 0.04), var=0)
    box(head, "Iron", (0, 0.33, 1.23), (0.12, 0.03, 0.04), var=0, shade=0)

    for part, side in ((arm_l, -1), (arm_r, 1)):
        box(part, "Bloat", (side * 0.62, 0.06, 0.76), (0.14, 0.14, 0.42), rot=(0, side * 0.3, 0))
        ico(part, "Bloat", (side * 0.68, 0.08, 0.54), 0.1, shade=2)

    for part, side in ((leg_l, -1), (leg_r, 1)):
        box(part, "Bloat", (side * 0.22, 0, 0.16), (0.18, 0.2, 0.32), shade=2)

    return [body, head, arm_l, arm_r, leg_l, leg_r]


def wraith(rng):
    body, head = Part("Body", (0, 0, 0.9), rng), Part("Head", (0, 0, 1.45), rng)
    arm_l, arm_r = Part("ArmL", (-0.25, 0, 1.3), rng), Part("ArmR", (0.25, 0, 1.3), rng)

    cyl(body, "Wraith", (0, 0, 0.85), 0.38, 0.2, 1.1, seg=7)
    for index in range(7):
        a = index / 7 * math.tau
        cyl(body, "Wraith", (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.2), 0.1, 0.0, 0.3, seg=3, rot=(math.pi, 0, a), shade=1)
    ico(body, "GlowCyan", (0, 0.2, 1.05), 0.07, var=0)
    for index in range(2):
        torus(body, "Iron", (0, 0, 0.7 + index * 0.35), 0.33 - index * 0.06, 0.02, seg=10, seg_minor=3, shade=2)

    cyl(head, "Wraith", (0, -0.03, 1.62), 0.24, 0.02, 0.55, seg=6, shade=2)
    box(head, "Iron", (0, 0.12, 1.53), (0.22, 0.05, 0.2), shade=0, var=0)
    for side in (-1, 1):
        box(head, "GlowCyan", (side * 0.06, 0.15, 1.56), (0.06, 0.02, 0.035), var=0)

    for part, side in ((arm_l, -1), (arm_r, 1)):
        cyl(part, "Wraith", (side * 0.32, 0.16, 1.15), 0.1, 0.05, 0.45, seg=5, rot=(-1.0, 0, side * -0.3))
        for finger in (-1, 0, 1):
            cyl(part, "Bone", (side * 0.36 + finger * 0.03, 0.36, 1.02), 0.015, 0.0, 0.14, seg=3, rot=(-math.pi * 0.5 - 0.6, 0, 0), var=0)

    return [body, head, arm_l, arm_r]


def brute(rng):
    body, head = Part("Body", (0, 0, 0.75), rng), Part("Head", (0, 0.15, 1.75), rng)
    arm_l, arm_r = Part("ArmL", (-0.62, 0, 1.62), rng), Part("ArmR", (0.62, 0, 1.62), rng)
    leg_l, leg_r = Part("LegL", (-0.3, 0, 0.72), rng), Part("LegR", (0.3, 0, 0.72), rng)

    box(body, "Hide", (0, 0.04, 0.78), (0.9, 0.56, 0.32))
    box(body, "BruteSkin", (0, 0.0, 1.32), (1.08, 0.72, 0.84))
    ico(body, "BruteSkin", (0, 0.2, 1.18), 0.42, scale=(1.0, 0.8, 0.9), shade=4)
    box(body, "Hide", (0, 0.0, 1.5), (1.12, 0.76, 0.12), rot=(0, 0.6, 0), shade=2)
    for side in (-1, 1):
        ico(body, "BruteSkin", (side * 0.62, 0.0, 1.66), 0.3)
        cyl(body, "Bone", (side * 0.62, 0.0, 1.98), 0.06, 0.0, 0.22, seg=4, rot=(0, side * 0.4, 0), var=0)

    box(head, "BruteSkin", (0, 0.18, 1.98), (0.48, 0.44, 0.42))
    box(head, "BruteSkin", (0, 0.3, 1.8), (0.4, 0.26, 0.12), shade=2)
    for side in (-1, 1):
        cyl(head, "Bone", (side * 0.22, 0.1, 2.28), 0.09, 0.0, 0.38, seg=5, rot=(0, side * 0.55, 0), var=0)
        box(head, "GlowYellow", (side * 0.11, 0.39, 2.0), (0.09, 0.03, 0.05), var=0)
        cyl(head, "Bone", (side * 0.12, 0.39, 1.83), 0.03, 0.0, 0.12, seg=3, var=0)

    for part, side in ((arm_l, -1), (arm_r, 1)):
        box(part, "BruteSkin", (side * 0.8, 0.06, 1.18), (0.3, 0.3, 0.8), rot=(0.12, 0, side * -0.1))
        box(part, "BruteSkin", (side * 0.84, 0.12, 0.7), (0.38, 0.38, 0.34))
        box(part, "Iron", (side * 0.84, 0.12, 0.92), (0.4, 0.4, 0.08), var=0)

    cyl(arm_r, "Wood", (0.86, 0.5, 0.68), 0.08, 0.17, 1.15, seg=6, rot=(-1.15, 0, 0))
    for index in range(4):
        a = index / 4 * math.tau
        cyl(arm_r, "Iron", (0.86 + math.cos(a) * 0.16, 0.95, 0.5 + math.sin(a) * 0.16), 0.04, 0.0, 0.16, seg=3, rot=(math.sin(a) * -1.2, math.cos(a) * 1.2, 0), var=0)

    for part, side in ((leg_l, -1), (leg_r, 1)):
        box(part, "BruteSkin", (side * 0.3, 0.0, 0.36), (0.36, 0.4, 0.72))
        box(part, "Hide", (side * 0.3, 0.06, 0.06), (0.42, 0.52, 0.12))

    return [body, head, arm_l, arm_r, leg_l, leg_r]


def lich(rng):
    body, head = Part("Body", (0, 0, 1.2), rng), Part("Head", (0, 0, 2.15), rng)
    arm_l, arm_r = Part("ArmL", (-0.6, 0, 1.95), rng), Part("ArmR", (0.6, 0, 1.95), rng)

    cyl(body, "LichRobe", (0, 0, 1.0), 0.78, 0.42, 1.9, seg=8)
    cyl(body, "Gold", (0, 0, 0.08), 0.79, 0.79, 0.08, seg=8, var=0)
    cyl(body, "Gold", (0, 0, 1.5), 0.55, 0.55, 0.08, seg=8, var=0)
    cyl(body, "LichRobe", (0, 0, 2.05), 0.58, 0.36, 0.3, seg=8, shade=2)
    ico(body, "GlowPurple", (0, 0.42, 1.6), 0.13, var=0)
    for index in range(8):
        a = index / 8 * math.tau
        cyl(body, "LichRobe", (math.cos(a) * 0.68, math.sin(a) * 0.68, 0.05), 0.16, 0.0, 0.35, seg=3, rot=(math.pi, 0, a), shade=1)
    for side in (-1, 1):
        for index in range(3):
            cyl(body, "Bone", (side * (0.4 + index * 0.1), -0.05, 2.15 + index * 0.02), 0.06, 0.0, 0.35 - index * 0.06, seg=4, rot=(0, side * (0.5 + index * 0.25), 0), var=0)

    ico(head, "Bone", (0, 0.05, 2.42), 0.27, scale=(1.0, 1.1, 1.05))
    box(head, "Bone", (0, 0.14, 2.22), (0.28, 0.22, 0.1), shade=2)
    cyl(head, "Gold", (0, 0, 2.65), 0.26, 0.28, 0.14, seg=8, var=0)
    for index in range(8):
        a = index / 8 * math.tau
        cyl(head, "Gold", (math.cos(a) * 0.25, math.sin(a) * 0.25, 2.82), 0.05, 0.0, 0.24, seg=3, var=0)
    box(head, "GlowRed", (0, 0.26, 2.67), (0.07, 0.04, 0.07), var=0)
    for side in (-1, 1):
        box(head, "Iron", (side * 0.09, 0.27, 2.43), (0.1, 0.06, 0.08), shade=0, var=0)
        box(head, "GlowPurple", (side * 0.09, 0.3, 2.43), (0.06, 0.03, 0.045), var=0)

    for part, side in ((arm_l, -1), (arm_r, 1)):
        cyl(part, "LichRobe", (side * 0.78, 0.18, 1.62), 0.13, 0.26, 0.85, seg=6, rot=(-0.4, 0, side * -0.3))
        ico(part, "Bone", (side * 0.88, 0.38, 1.28), 0.1)

    ico(arm_l, "GlowPurple", (-0.95, 0.62, 1.28), 0.16, var=0)
    torus(arm_l, "Gold", (-0.95, 0.62, 1.28), 0.24, 0.02, seg=12, seg_minor=3, rot=(0.6, 0.3, 0), var=0)

    cyl(arm_r, "Iron", (0.92, 0.4, 1.5), 0.05, 0.05, 3.0, seg=5)
    ico(arm_r, "Bone", (0.92, 0.4, 3.05), 0.16)
    ico(arm_r, "GlowPurple", (0.92, 0.4, 3.32), 0.12, var=0)
    for index in range(3):
        a = index / 3 * math.tau
        cyl(arm_r, "Gold", (0.92 + math.cos(a) * 0.12, 0.4 + math.sin(a) * 0.12, 3.3), 0.025, 0.0, 0.3, seg=3, rot=(math.sin(a) * -0.4, math.cos(a) * 0.4, 0), var=0)

    return [body, head, arm_l, arm_r]


def single(rng, function):
    main = Part("Main", (0, 0, 0), rng)
    function(main, rng)
    return [main]


def gravestone(p, rng):
    box(p, "Stone", (0, 0, 0.06), (0.86, 0.4, 0.12), var=2)
    box(p, "Stone", (0, 0, 0.45), (0.68, 0.22, 0.7), var=2)
    cyl(p, "Stone", (0, 0, 0.8), 0.34, 0.34, 0.22, seg=10, rot=(math.pi * 0.5, 0, 0), var=2)
    box(p, "Iron", (0, 0.11, 0.62), (0.06, 0.02, 0.36), shade=4, var=0)
    box(p, "Iron", (0, 0.11, 0.72), (0.24, 0.02, 0.06), shade=4, var=0)
    box(p, "Moss", (0.2, 0.0, 0.14), (0.3, 0.42, 0.06), rot=(0, 0, 0.3))


def grave_cross(p, rng):
    box(p, "Stone", (0, 0, 0.08), (0.62, 0.4, 0.16), rot=(0, 0, 0.05), var=2)
    box(p, "Stone", (0, 0, 0.66), (0.16, 0.16, 1.1), var=2)
    box(p, "Stone", (0, 0, 0.88), (0.6, 0.15, 0.15), var=2)
    box(p, "Moss", (0, 0.0, 0.2), (0.2, 0.2, 0.08))


def rock(seed):
    def build(p, rng):
        local = random.Random(seed)
        ico(p, "Stone", (0, 0, 0.2), 0.75, scale=(1.0, local.uniform(0.8, 1.1), local.uniform(0.6, 0.8)), rot=(0, 0, local.uniform(0, math.pi)), jitter=0.22, flatten_below=-0.05, seed=seed, var=2)
        ico(p, "Stone", (local.uniform(0.4, 0.6), local.uniform(-0.3, 0.3), 0.1), 0.32, scale=(1.0, 0.9, 0.7), jitter=0.25, flatten_below=-0.05, seed=seed + 7, shade=1, var=1)
        ico(p, "Moss", (0, 0, 0.62), 0.35, scale=(1.4, 1.1, 0.25), jitter=0.2, seed=seed + 3, var=1)
    return build


def dead_tree(seed):
    def build(p, rng):
        local = random.Random(seed)
        cyl(p, "Wood", (0, 0, 1.5), 0.2, 0.06, 3.0, seg=6, rot=(local.uniform(-0.06, 0.06), local.uniform(-0.06, 0.06), 0))
        for index in range(4):
            angle = index * math.pi * 0.5 + local.uniform(-0.4, 0.4)
            cyl(p, "Wood", (math.cos(angle) * 0.22, math.sin(angle) * 0.22, 0.08), 0.1, 0.02, 0.5, seg=4, rot=(0, math.pi * 0.5 - 0.35, angle), shade=2)
        for index in range(local.randint(4, 5)):
            angle = index * 2.4 + local.uniform(-0.3, 0.3)
            height = local.uniform(1.5, 2.7)
            length = local.uniform(0.7, 1.2)
            pitch = local.uniform(0.6, 1.0)
            direction = Vector((math.cos(angle) * math.sin(pitch), math.sin(angle) * math.sin(pitch), math.cos(pitch)))
            center = Vector((0, 0, height)) + direction * length * 0.5
            cyl(p, "Wood", center, 0.07, 0.015, length, seg=4, rot=look_rot(direction))
            twig = (direction + Vector((-direction.y, direction.x, 0.6))).normalized()
            cyl(p, "Wood", Vector((0, 0, height)) + direction * length * 0.7 + twig * 0.2, 0.03, 0.008, 0.4, seg=3, rot=look_rot(twig))
            if local.random() < 0.6:
                tip = Vector((0, 0, height)) + direction * length
                ico(p, "Autumn", tip, 0.16, scale=(1.2, 1.2, 0.7), jitter=0.3, seed=index + seed, var=2)
    return build


def lantern_post(p, rng):
    box(p, "Wood", (0, 0, 1.1), (0.18, 0.18, 2.2))
    box(p, "Iron", (0, 0, 2.23), (0.38, 0.38, 0.06), var=0)
    box(p, "GlowOrange", (0, 0, 2.42), (0.24, 0.24, 0.32), var=0)
    for x in (-1, 1):
        for y in (-1, 1):
            box(p, "Iron", (x * 0.15, y * 0.15, 2.42), (0.04, 0.04, 0.34), var=0)
    cyl(p, "Iron", (0, 0, 2.68), 0.3, 0.0, 0.22, seg=4, rot=(0, 0, math.pi * 0.25), var=0)
    cyl(p, "Iron", (0, 0, 2.83), 0.04, 0.04, 0.1, seg=6, var=0)


def wall_segment(p, rng):
    x = -2.0
    while x < 2.0:
        width = min(rng.uniform(0.5, 0.9), 2.0 - x)
        height = rng.uniform(0.38, 0.5)
        box(p, "Stone", (x + width * 0.5, rng.uniform(-0.04, 0.04), height * 0.5), (width - 0.04, 0.9, height), rot=(0, 0, rng.uniform(-0.05, 0.05)), var=2)
        x += width
    x = -2.0
    while x < 2.0:
        width = min(rng.uniform(0.4, 0.8), 2.0 - x)
        height = rng.uniform(0.3, 0.48)
        box(p, "Stone", (x + width * 0.5, rng.uniform(-0.06, 0.06), 0.45 + height * 0.5), (width - 0.05, 0.78, height), rot=(rng.uniform(-0.04, 0.04), 0, rng.uniform(-0.08, 0.08)), var=2)
        if rng.random() < 0.35:
            box(p, "Moss", (x + width * 0.5, 0.0, 0.45 + height + 0.03), (width * 0.7, 0.6, 0.06), var=1)
        x += width


def pillar(p, rng):
    box(p, "Stone", (0, 0, 0.1), (0.9, 0.9, 0.2), var=2)
    cyl(p, "Stone", (0, 0, 0.9), 0.32, 0.29, 1.6, seg=8, var=2)
    cyl(p, "Stone", (0, 0, 1.75), 0.29, 0.22, 0.18, seg=8, rot=(0.2, 0.1, 0), var=2)
    ico(p, "Stone", (0.55, 0.3, 0.12), 0.18, jitter=0.25, seed=4, var=2)
    ico(p, "Stone", (-0.4, 0.5, 0.1), 0.14, jitter=0.25, seed=5, var=2)
    box(p, "Moss", (0, 0.0, 0.22), (0.7, 0.7, 0.06), rot=(0, 0, 0.3))


def mushrooms(p, rng):
    for index in range(rng.randint(3, 5)):
        x, y = rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3)
        height = rng.uniform(0.12, 0.35)
        family = "GlowCyan" if rng.random() < 0.5 else "GlowPink"
        cyl(p, "Bone", (x, y, height * 0.5), 0.025, 0.02, height, seg=4, shade=6, var=0)
        cyl(p, family, (x, y, height + 0.03), rng.uniform(0.07, 0.12), 0.0, 0.08, seg=6, shade=3, var=1)


def grass_tuft(p, rng):
    for index in range(rng.randint(5, 8)):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.0, 0.18)
        height = rng.uniform(0.25, 0.5)
        lean = rng.uniform(0.1, 0.4)
        cyl(p, "Grass", (math.cos(a) * r, math.sin(a) * r, height * 0.5), 0.035, 0.0, height, seg=3, rot=(math.sin(a) * lean, -math.cos(a) * lean, a), shade=rng.randint(3, 6), var=0)


def bones(p, rng):
    ico(p, "Bone", (0, 0, 0.12), 0.13, scale=(1.0, 1.1, 1.0), rot=(0.3, 0.2, 0.5))
    box(p, "Iron", (0.05, 0.1, 0.13), (0.06, 0.04, 0.05), shade=0, var=0)
    for index in range(4):
        a = rng.uniform(0, math.tau)
        cyl(p, "Bone", (math.cos(a) * 0.25, math.sin(a) * 0.25, 0.03), 0.025, 0.025, 0.4, seg=4, rot=(math.pi * 0.5, 0, a), var=1)


def gem(p, rng):
    bipyramid(p, "GlowGreen", [(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.0) for a in (i * math.tau / 6 for i in range(6))], (0, 0, 0.5), (0, 0, -0.5))


def sickle(p, angle, samples=12):
    faces = [("Steel", 4), ("GlowWhite", 0), ("GlowWhite", 0), ("Steel", 3), ("Steel", 1)]
    rings = []
    for index in range(samples):
        t = index / samples
        radius = 0.06 + 0.4 * t
        theta = angle + 1.15 * t
        width = 0.13 * math.sin(math.pi * (0.18 + 0.82 * t)) ** 0.8 + 0.01
        center = Vector((math.cos(theta) * radius, math.sin(theta) * radius, 0.0))
        tangent = Vector((-math.sin(theta) * radius + math.cos(theta) * 0.4, math.cos(theta) * radius + math.sin(theta) * 0.4, 0.0)).normalized()
        normal = Vector((tangent.y, -tangent.x, 0.0))
        spine = center - normal * width * 0.5
        mid = center + normal * width * 0.25
        edge = center + normal * width * 0.5
        thickness = 0.024 * (1.0 - 0.6 * t)
        rings.append([p.bm.verts.new(v) for v in (spine + Vector((0, 0, thickness)), mid + Vector((0, 0, thickness)), edge, mid - Vector((0, 0, thickness)), spine - Vector((0, 0, thickness)))])

    tip = p.bm.verts.new(Vector((math.cos(angle + 1.15) * 0.48, math.sin(angle + 1.15) * 0.48, 0.0)))

    def tag(face, family, shade):
        face[p.family] = FAMILY_INDEX[family]
        face[p.shade] = shade

    for a, b in zip(rings[:-1], rings[1:]):
        for k, (family, shade) in enumerate(faces):
            tag(p.bm.faces.new((a[k], a[(k + 1) % 5], b[(k + 1) % 5], b[k])), family, shade)
    for k, (family, shade) in enumerate(faces):
        tag(p.bm.faces.new((rings[-1][k], rings[-1][(k + 1) % 5], tip)), family, shade)
    tag(p.bm.faces.new(list(reversed(rings[0]))), "Steel", 1)


def blade(p, rng):
    for index in range(3):
        sickle(p, index * math.tau / 3)
    cyl(p, "Gold", (0, 0, 0), 0.11, 0.11, 0.075, seg=10, var=0)
    cyl(p, "Gold", (0, 0, 0), 0.075, 0.075, 0.1, seg=10, shade=5, var=0)
    for side in (-1, 1):
        ico(p, "GlowRed", (0, 0, side * 0.05), 0.045, scale=(1, 1, 0.6), var=0)


def bolt(p, rng):
    bipyramid(p, "GlowCyan", [(math.cos(a) * 0.11, 0.08, math.sin(a) * 0.11) for a in (i * math.tau / 6 for i in range(6))], (0, 0.4, 0), (0, -0.38, 0))


def fireball(p, rng):
    ico(p, "GlowOrange", (0, 0, 0), 0.28, jitter=0.18, seed=2, var=1)
    for index in range(6):
        a = index * 1.1
        direction = Vector((math.cos(a), math.sin(a) * 0.6, math.sin(a * 1.7)))
        cyl(p, "GlowYellow", direction.normalized() * 0.22, 0.08, 0.0, 0.22, seg=3, rot=look_rot(direction), var=0)


def arrow(p, rng):
    cyl(p, "Wood", (0, 0, 0), 0.015, 0.015, 0.75, seg=4, rot=(-math.pi * 0.5, 0, 0), var=0)
    cyl(p, "Iron", (0, 0.42, 0), 0.04, 0.0, 0.1, seg=4, rot=(-math.pi * 0.5, 0, 0), shade=5, var=0)
    for a in (0.0, math.pi * 0.5):
        box(p, "Red", (0, -0.33, 0), (0.12, 0.1, 0.01), rot=(0, a, 0), var=0)


def orb(p, rng):
    ico(p, "GlowPurple", (0, 0, 0), 0.22, var=0)
    for index in range(4):
        a = index / 4 * math.tau
        cyl(p, "GlowPink", (math.cos(a) * 0.2, math.sin(a) * 0.2, 0), 0.05, 0.0, 0.14, seg=3, rot=(0, math.pi * 0.5, a), var=0)


def potion(p, rng):
    sphere(p, "GlowRed", (0, 0, 0.22), 0.2, seg=8, rings=6, scale=(1, 1, 1.05), var=1)
    cyl(p, "Stone", (0, 0, 0.47), 0.07, 0.06, 0.16, seg=6, shade=7, var=0)
    cyl(p, "Wood", (0, 0, 0.58), 0.075, 0.075, 0.08, seg=6, var=0)
    cyl(p, "Gold", (0, 0, 0.4), 0.09, 0.09, 0.04, seg=6, var=0)


def magnet(p, rng):
    for index in range(7):
        a = math.pi + index / 6 * math.pi
        box(p, "Red", (math.cos(a) * 0.22, 0, 0.35 + math.sin(a) * 0.22), (0.13, 0.13, 0.12), rot=(0, -a, 0), var=0)
    for side in (-1, 1):
        box(p, "Red", (side * 0.22, 0, 0.42), (0.13, 0.13, 0.16), var=0)
        box(p, "Stone", (side * 0.22, 0, 0.56), (0.14, 0.14, 0.12), shade=7, var=0)


def bomb(p, rng):
    ico(p, "Iron", (0, 0, 0.3), 0.28, shade=2, var=1)
    cyl(p, "Gold", (0, 0, 0.3), 0.285, 0.285, 0.06, seg=10, var=0)
    cyl(p, "Iron", (0, 0, 0.6), 0.08, 0.08, 0.08, seg=6, shade=4, var=0)
    cyl(p, "Leather", (0.04, 0, 0.72), 0.02, 0.02, 0.18, seg=4, rot=(0, 0.4, 0), var=0)
    ico(p, "GlowOrange", (0.08, 0, 0.82), 0.05, var=0)


def chest(rng):
    base = Part("Base", (0, 0, 0), rng)
    lid = Part("Lid", (0, -0.28, 0.42), rng)
    box(base, "Wood", (0, 0, 0.22), (0.84, 0.56, 0.42))
    box(base, "GlowYellow", (0, 0, 0.42), (0.76, 0.48, 0.03), var=0)
    for x in (-0.3, 0.3):
        box(base, "Gold", (x, 0, 0.22), (0.07, 0.58, 0.44), var=0)
    box(base, "Gold", (0, 0.29, 0.36), (0.12, 0.04, 0.14), var=0)
    cyl(lid, "Wood", (0, 0, 0.42), 0.28, 0.28, 0.84, seg=8, rot=(0, math.pi * 0.5, 0), scale=(1, 1, 1))
    for x in (-0.3, 0.3):
        cyl(lid, "Gold", (x, 0, 0.42), 0.29, 0.29, 0.07, seg=8, rot=(0, math.pi * 0.5, 0), var=0)
    for vert in lid.bm.verts:
        vert.co.z = max(vert.co.z, 0.42)
    return [base, lid]


def meteor(p, rng):
    ico(p, "Stone", (0, 0, 0), 0.6, jitter=0.25, seed=9, shade=1, var=1)
    for index in range(6):
        a = index * 1.3
        direction = Vector((math.cos(a), math.sin(a), math.sin(a * 2.1))).normalized()
        ico(p, "GlowOrange", direction * 0.52, 0.12, jitter=0.2, seed=index, var=0)


def spark(p, rng):
    bipyramid(p, "GlowWhite", [(math.cos(a) * 0.08, 0.0, math.sin(a) * 0.08) for a in (i * math.tau / 4 for i in range(4))], (0, 0.5, 0), (0, -0.5, 0))


def chunk(p, rng):
    ico(p, "Stone", (0, 0, 0), 0.5, jitter=0.3, seed=1, subdiv=1, var=0)


def puff(p, rng):
    ico(p, "GlowWhite", (0, 0, 0), 0.5, jitter=0.12, seed=6, var=0)


def ring(p, rng):
    torus(p, "GlowWhite", (0, 0, 0), 0.5, 0.035, seg=28, seg_minor=4)


def disc(p, rng):
    cyl(p, "GlowWhite", (0, 0, 0), 0.5, 0.5, 0.02, seg=24, var=0)


def starburst(p, rng):
    outline = [(math.cos(a) * r, math.sin(a) * r) for a, r in ((i * math.pi / 12, 0.5 if i % 2 == 0 else 0.25) for i in range(24))]
    slab(p, "GlowWhite", outline, 0.02, var=0)


def beam(p, rng):
    cyl(p, "GlowWhite", (0, 0, 0), 0.045, 0.045, 1.0, seg=5, rot=(-math.pi * 0.5, 0, 0), var=0)


def ground(rng):
    parts = {}
    for name, tile in (("Grass", 8.0), ("Dirt", 5.0), ("Stone", 4.0), ("Moss", 8.0)):
        part = Part(name, (0, 0, 0), rng)
        part.recalc = False
        part.uv_tile = tile
        parts[name] = part

    half = 56.0
    cells = 112
    step = half * 2 / cells
    blobs = [(rng.uniform(-38, 38), rng.uniform(-38, 38), rng.uniform(2.0, 5.0)) for _ in range(22)]

    def height(x, y):
        edge = max(abs(x), abs(y))
        h = (math.sin(x * 0.7) * math.cos(y * 0.6)) * 0.03
        if edge > 41.5:
            t = min((edge - 41.5) / 12.0, 1.0)
            h += t * t * (2.0 + math.sin(x * 0.35) * math.cos(y * 0.4) * 1.2)
        return h

    points = []
    for j in range(cells + 1):
        row = []
        for i in range(cells + 1):
            x = -half + i * step
            y = -half + j * step
            if 0 < i < cells and 0 < j < cells:
                x += rng.uniform(-0.2, 0.2)
                y += rng.uniform(-0.2, 0.2)
            row.append(Vector((x, y, height(x, y))))
        points.append(row)

    def wobble(x, y):
        return math.sin(x * 1.3 + y * 0.4) * 0.35 + math.sin(y * 1.7 - x * 0.6) * 0.3

    def moss_field(x, y):
        return 41.5 + wobble(x, y) - max(abs(x), abs(y))

    def stone_field(x, y):
        return math.hypot(x, y) - 4.0 - wobble(x, y) * 0.5

    def dirt_field(x, y):
        field = math.hypot(x, y) - 5.8 - wobble(x, y)
        for bx, by, br in blobs:
            field = min(field, math.hypot(x - bx, y - by) - br - wobble(x, y) * 1.5)
        return field

    regions = (("Moss", moss_field), ("Stone", stone_field), ("Dirt", dirt_field))

    def key(point):
        return (round(point.x, 5), round(point.y, 5))

    def crossing(p, q, fp, fq):
        if key(q) < key(p):
            p, q, fp, fq = q, p, fq, fp
        t = fp / (fp - fq)
        x = p.x + (q.x - p.x) * t
        y = p.y + (q.y - p.y) * t
        return Vector((x, y, height(x, y)))

    def split(polygon, field):
        values = [field(point.x, point.y) for point in polygon]
        inside, outside = [], []
        for index, point in enumerate(polygon):
            following = (index + 1) % len(polygon)
            fp, fq = values[index], values[following]
            (inside if fp < 0.0 else outside).append(point)
            if (fp < 0.0) != (fq < 0.0):
                cut = crossing(point, polygon[following], fp, fq)
                inside.append(cut)
                outside.append(cut)
        return inside, outside

    shared = {name: {} for name in parts}

    def emit(name, polygon):
        cache = shared[name]
        verts = []
        for point in polygon:
            point_key = key(point)
            if point_key not in cache:
                cache[point_key] = parts[name].bm.verts.new(point)
            if not verts or verts[-1] is not cache[point_key]:
                verts.append(cache[point_key])
        if len(verts) > 1 and verts[0] is verts[-1]:
            verts.pop()
        if len(verts) >= 3 and len(set(verts)) == len(verts):
            parts[name].bm.faces.new(verts)

    for j in range(cells):
        for i in range(cells):
            a, b, c, d = points[j][i], points[j][i + 1], points[j + 1][i + 1], points[j + 1][i]
            tris = ((a, b, c), (a, c, d)) if (i + j) % 2 == 0 else ((a, b, d), (b, c, d))
            for tri in tris:
                remaining = list(tri)
                for name, field in regions:
                    inside, remaining = split(remaining, field)
                    if len(inside) >= 3:
                        emit(name, inside)
                    if len(remaining) < 3:
                        break
                else:
                    emit("Grass", remaining)

    for part in parts.values():
        part.bm.normal_update()

    return list(parts.values())


ASSETS = [
    ("Hero", hero),
    ("Ghoul", ghoul),
    ("Bat", bat),
    ("Skeleton", skeleton),
    ("Bloater", bloater),
    ("Wraith", wraith),
    ("Brute", brute),
    ("Lich", lich),
    ("Chest", chest),
    ("Gravestone", lambda r: single(r, gravestone)),
    ("GraveCross", lambda r: single(r, grave_cross)),
    ("Rock0", lambda r: single(r, rock(11))),
    ("Rock1", lambda r: single(r, rock(23))),
    ("Rock2", lambda r: single(r, rock(37))),
    ("DeadTree0", lambda r: single(r, dead_tree(5))),
    ("DeadTree1", lambda r: single(r, dead_tree(9))),
    ("LanternPost", lambda r: single(r, lantern_post)),
    ("WallSegment", lambda r: single(r, wall_segment)),
    ("Pillar", lambda r: single(r, pillar)),
    ("Mushrooms", lambda r: single(r, mushrooms)),
    ("GrassTuft", lambda r: single(r, grass_tuft)),
    ("Bones", lambda r: single(r, bones)),
    ("Gem", lambda r: single(r, gem)),
    ("Blade", lambda r: single(r, blade)),
    ("Bolt", lambda r: single(r, bolt)),
    ("Fireball", lambda r: single(r, fireball)),
    ("Arrow", lambda r: single(r, arrow)),
    ("Orb", lambda r: single(r, orb)),
    ("Potion", lambda r: single(r, potion)),
    ("Magnet", lambda r: single(r, magnet)),
    ("Bomb", lambda r: single(r, bomb)),
    ("Meteor", lambda r: single(r, meteor)),
    ("Spark", lambda r: single(r, spark)),
    ("Chunk", lambda r: single(r, chunk)),
    ("Puff", lambda r: single(r, puff)),
    ("Ring", lambda r: single(r, ring)),
    ("Disc", lambda r: single(r, disc)),
    ("Beam", lambda r: single(r, beam)),
    ("Starburst", lambda r: single(r, starburst)),
    ("Ground", ground),
]


def main():
    output = Path(sys.argv[sys.argv.index("--") + 1]) if "--" in sys.argv else Path(__file__).with_name("DuskSwarm.glb")

    bpy.ops.wm.read_factory_settings(use_empty=True)

    write_palettes(output.parent)

    material = bpy.data.materials.new("Palette")

    for index, (name, function) in enumerate(ASSETS):
        rng = random.Random(1000 + index)
        offset = (0.0, -120.0, 0.0) if name == "Ground" else ((index % 8) * 4.0, (index // 8) * 4.0, 0.0)
        build_asset(name, function(rng), offset, material)

    bpy.ops.wm.save_as_mainfile(filepath=str(output.with_suffix(".blend")))
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True, export_apply=True, export_normals=True, export_texcoords=True, export_materials="EXPORT", export_cameras=False, export_lights=False)


main()
