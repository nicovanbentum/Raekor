import bpy
import bmesh
import math
import random
import sys
from pathlib import Path
from mathutils import Matrix, Euler, Vector


MATERIALS = {
    "Cloak":     (0.12, 0.18, 0.40, 0.0),
    "Skin":      (0.85, 0.70, 0.58, 0.0),
    "Visor":     (0.35, 0.85, 1.00, 1.0),
    "Trim":      (0.90, 0.68, 0.25, 0.0),
    "Boot":      (0.10, 0.08, 0.07, 0.0),
    "Wood":      (0.18, 0.12, 0.08, 0.0),
    "GhoulSkin": (0.30, 0.42, 0.28, 0.0),
    "Rags":      (0.18, 0.15, 0.12, 0.0),
    "GhoulEyes": (0.70, 1.00, 0.30, 1.0),
    "BatFur":    (0.42, 0.22, 0.55, 0.0),
    "Membrane":  (0.22, 0.10, 0.28, 0.0),
    "BatEyes":   (1.00, 0.20, 0.15, 1.0),
    "BruteSkin": (0.55, 0.12, 0.10, 0.0),
    "Hide":      (0.35, 0.24, 0.14, 0.0),
    "Horn":      (0.85, 0.80, 0.65, 0.0),
    "BruteEyes": (1.00, 0.85, 0.20, 1.0),
    "Stone":     (0.42, 0.44, 0.48, 0.0),
    "StoneDark": (0.20, 0.21, 0.23, 0.0),
    "Iron":      (0.10, 0.10, 0.11, 0.0),
    "Flame":     (1.00, 0.55, 0.20, 1.0),
    "Gem":       (0.30, 1.00, 0.50, 1.0),
    "Blade":     (1.00, 0.60, 0.20, 1.0),
    "Bolt":      (0.35, 0.85, 1.00, 1.0),
}


def get_material(name):
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    r, g, b, emissive = MATERIALS[name]
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
    bsdf.inputs["Emission Color"].default_value = (r, g, b, 1.0)
    bsdf.inputs["Emission Strength"].default_value = emissive
    bsdf.inputs["Roughness"].default_value = 0.8
    return material


class Builder:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.materials = []

    def material_index(self, material):
        if material not in self.materials:
            self.materials.append(material)
        return self.materials.index(material)

    def finish_part(self, verts, material):
        index = self.material_index(material)
        faces = {face for vert in verts for face in vert.link_faces}
        for face in faces:
            face.material_index = index
            for loop in face.loops:
                co = loop.vert.co
                loop[self.uv].uv = (co.x + co.z * 0.5, co.y + co.z * 0.5)
        return verts

    @staticmethod
    def matrix(loc, rot, scale):
        return Matrix.LocRotScale(Vector(loc), Euler(rot), Vector(scale))

    def box(self, material, loc, size, rot=(0, 0, 0)):
        result = bmesh.ops.create_cube(self.bm, size=1.0, matrix=self.matrix(loc, rot, size))
        return self.finish_part(result["verts"], material)

    def cone(self, material, loc, r1, r2, depth, segments=6, rot=(0, 0, 0), scale=(1, 1, 1)):
        result = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segments, radius1=r1, radius2=r2, depth=depth, matrix=self.matrix(loc, rot, scale))
        return self.finish_part(result["verts"], material)

    def ico(self, material, loc, radius, scale=(1, 1, 1), rot=(0, 0, 0), jitter=0.0, flatten_below=None, seed=0):
        result = bmesh.ops.create_icosphere(self.bm, subdivisions=1, radius=radius, matrix=Matrix.Identity(4))
        verts = result["verts"]
        rng = random.Random(seed)
        for vert in verts:
            vert.co *= 1.0 + rng.uniform(-jitter, jitter)
        bmesh.ops.transform(self.bm, matrix=self.matrix(loc, rot, scale), verts=verts)
        if flatten_below is not None:
            for vert in verts:
                vert.co.z = max(vert.co.z, flatten_below)
        return self.finish_part(verts, material)

    def slab(self, material, outline, thickness, loc=(0, 0, 0), rot=(0, 0, 0), ridge=0.0):
        transform = self.matrix(loc, rot, (1, 1, 1))
        cx = sum(x for x, _ in outline) / len(outline)
        cy = sum(y for _, y in outline) / len(outline)
        top = self.bm.verts.new(transform @ Vector((cx, cy, thickness * 0.5 + ridge)))
        bottom = self.bm.verts.new(transform @ Vector((cx, cy, -thickness * 0.5 - ridge)))
        upper = [self.bm.verts.new(transform @ Vector((x, y, thickness * 0.5))) for x, y in outline]
        lower = [self.bm.verts.new(transform @ Vector((x, y, -thickness * 0.5))) for x, y in outline]
        count = len(outline)
        for i in range(count):
            j = (i + 1) % count
            self.bm.faces.new((upper[i], upper[j], top))
            self.bm.faces.new((lower[j], lower[i], bottom))
            self.bm.faces.new((lower[i], lower[j], upper[j], upper[i]))
        return self.finish_part(upper + lower + [top, bottom], material)

    def bipyramid(self, material, ring_points, apex_top, apex_bottom):
        ring = [self.bm.verts.new(Vector(p)) for p in ring_points]
        top = self.bm.verts.new(Vector(apex_top))
        bottom = self.bm.verts.new(Vector(apex_bottom))
        count = len(ring)
        for i in range(count):
            j = (i + 1) % count
            self.bm.faces.new((ring[i], ring[j], top))
            self.bm.faces.new((ring[j], ring[i], bottom))
        return self.finish_part(ring + [top, bottom], material)

    def build(self, offset):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for polygon in mesh.polygons:
            polygon.use_smooth = False
        for material in self.materials:
            mesh.materials.append(get_material(material))
        obj = bpy.data.objects.new(self.name, mesh)
        obj.location = offset
        bpy.context.scene.collection.objects.link(obj)
        return obj


def hero(b):
    for side in (-1, 1):
        b.box("Boot", (side * 0.11, 0.02, 0.1), (0.16, 0.24, 0.2))
        b.box("Cloak", (side * 0.11, 0.0, 0.35), (0.15, 0.17, 0.35))
        b.box("Cloak", (side * 0.36, 0.02, 0.92), (0.14, 0.15, 0.48), rot=(0.15, side * 0.12, 0))
        b.box("Skin", (side * 0.38, 0.06, 0.66), (0.11, 0.11, 0.1))
    b.cone("Cloak", (0, 0, 0.78), 0.36, 0.22, 0.72, segments=8)
    b.cone("Trim", (0, 0, 0.86), 0.29, 0.29, 0.07, segments=8)
    b.box("Cloak", (0, 0, 1.12), (0.6, 0.32, 0.16))
    b.ico("Skin", (0, 0.02, 1.36), 0.19)
    b.cone("Cloak", (0, -0.04, 1.5), 0.27, 0.0, 0.56, segments=6, rot=(-0.12, 0, 0))
    for side in (-1, 1):
        b.box("Visor", (side * 0.065, 0.17, 1.39), (0.07, 0.04, 0.045))
    b.cone("Wood", (0.44, 0.1, 0.85), 0.035, 0.03, 1.65, segments=5)
    b.ico("Visor", (0.44, 0.1, 1.72), 0.09)
    b.cone("Trim", (0.44, 0.1, 1.6), 0.06, 0.06, 0.06, segments=5)


def ghoul(b):
    for side in (-1, 1):
        b.box("Rags", (side * 0.13, 0.0, 0.25), (0.14, 0.16, 0.5), rot=(side * 0.1, 0, 0))
        b.box("GhoulSkin", (side * 0.13, 0.05, 0.03), (0.15, 0.22, 0.06))
        b.box("GhoulSkin", (side * 0.29, 0.38, 0.88), (0.1, 0.5, 0.1), rot=(-0.25, 0, side * 0.12))
        for finger in (-1, 0, 1):
            b.cone("Horn", (side * 0.29 + finger * 0.03, 0.66, 0.79), 0.018, 0.0, 0.12, segments=3, rot=(-math.pi * 0.5 - 0.4, 0, 0))
    b.box("Rags", (0, 0.06, 0.74), (0.48, 0.3, 0.52), rot=(-0.35, 0, 0))
    b.box("GhoulSkin", (0, 0.16, 0.9), (0.4, 0.26, 0.2), rot=(-0.35, 0, 0))
    b.box("GhoulSkin", (0, 0.3, 1.07), (0.3, 0.3, 0.28), rot=(0.1, 0, 0.08))
    b.box("GhoulSkin", (0, 0.38, 0.92), (0.24, 0.2, 0.08), rot=(0.25, 0, 0))
    for side in (-1, 1):
        b.box("GhoulEyes", (side * 0.07, 0.45, 1.1), (0.07, 0.03, 0.05))
    for index, (x, z) in enumerate(((-0.2, 0.5), (0.15, 0.48), (0.0, 0.46))):
        b.cone("Rags", (x, 0.02, z), 0.08, 0.0, 0.18, segments=3, rot=(math.pi, 0, index))


def bat_body(b):
    b.ico("BatFur", (0, 0, 0), 0.17, scale=(1.0, 1.25, 1.0))
    b.ico("BatFur", (0, 0.2, 0.06), 0.12)
    for side in (-1, 1):
        b.cone("BatFur", (side * 0.07, 0.19, 0.2), 0.05, 0.0, 0.15, segments=4, rot=(0, side * 0.3, 0))
        b.box("BatEyes", (side * 0.05, 0.31, 0.08), (0.045, 0.02, 0.04))
        b.cone("Horn", (side * 0.025, 0.3, 0.0), 0.012, 0.0, 0.06, segments=3, rot=(math.pi, 0, 0))
        b.cone("BatFur", (side * 0.06, -0.08, -0.16), 0.03, 0.0, 0.12, segments=3, rot=(math.pi, 0, 0))


def bat_wing(b, side):
    outline = [(0.0, 0.1), (0.3, 0.14), (0.68, 0.02), (0.5, -0.12), (0.36, -0.06), (0.24, -0.17), (0.12, -0.08), (0.0, -0.1)]
    b.slab("Membrane", [(side * x, y) for x, y in outline], 0.025)
    b.cone("BatFur", (side * 0.34, 0.11, 0.0), 0.02, 0.012, 0.68, segments=4, rot=(0, math.pi * 0.5, side * -0.1))


def brute(b):
    for side in (-1, 1):
        b.box("BruteSkin", (side * 0.3, 0.0, 0.36), (0.36, 0.4, 0.72))
        b.box("Hide", (side * 0.3, 0.06, 0.06), (0.42, 0.52, 0.12))
        b.ico("BruteSkin", (side * 0.62, 0.0, 1.66), 0.3)
        b.box("BruteSkin", (side * 0.8, 0.06, 1.18), (0.3, 0.3, 0.8), rot=(0.12, 0, side * -0.1))
        b.box("BruteSkin", (side * 0.84, 0.12, 0.7), (0.38, 0.38, 0.34))
        b.cone("Horn", (side * 0.22, 0.1, 2.28), 0.09, 0.0, 0.38, segments=5, rot=(0, side * 0.55, 0))
        b.box("BruteEyes", (side * 0.11, 0.39, 2.0), (0.09, 0.03, 0.05))
        b.cone("Horn", (side * 0.12, 0.39, 1.83), 0.03, 0.0, 0.12, segments=3)
        b.box("Iron", (side * 0.84, 0.12, 0.92), (0.4, 0.4, 0.08))
    b.box("Hide", (0, 0.04, 0.78), (0.9, 0.56, 0.32))
    b.box("BruteSkin", (0, 0.0, 1.32), (1.08, 0.72, 0.84))
    b.ico("BruteSkin", (0, 0.2, 1.18), 0.42, scale=(1.0, 0.8, 0.9))
    b.box("Hide", (0, 0.0, 1.5), (1.12, 0.76, 0.12), rot=(0, 0.6, 0))
    b.box("BruteSkin", (0, 0.18, 1.98), (0.48, 0.44, 0.42))
    b.box("BruteSkin", (0, 0.3, 1.8), (0.4, 0.26, 0.12))


def gravestone(b):
    b.box("Stone", (0, 0, 0.06), (0.86, 0.4, 0.12))
    b.box("Stone", (0, 0, 0.45), (0.68, 0.22, 0.7))
    b.cone("Stone", (0, 0, 0.8), 0.34, 0.34, 0.22, segments=10, rot=(math.pi * 0.5, 0, 0))
    b.box("StoneDark", (0, 0.11, 0.62), (0.06, 0.02, 0.36))
    b.box("StoneDark", (0, 0.11, 0.72), (0.24, 0.02, 0.06))


def grave_cross(b):
    b.box("Stone", (0, 0, 0.08), (0.62, 0.4, 0.16), rot=(0, 0, 0.05))
    b.box("Stone", (0, 0, 0.66), (0.16, 0.16, 1.1))
    b.box("Stone", (0, 0, 0.88), (0.6, 0.15, 0.15))
    b.box("StoneDark", (0, 0.0, 0.22), (0.24, 0.2, 0.12))


def rock(b, seed):
    rng = random.Random(seed)
    b.ico("Stone", (0, 0, 0.2), 0.75, scale=(1.0, rng.uniform(0.8, 1.1), rng.uniform(0.6, 0.8)), rot=(0, 0, rng.uniform(0, math.pi)), jitter=0.22, flatten_below=-0.05, seed=seed)
    b.ico("StoneDark", (rng.uniform(0.4, 0.6), rng.uniform(-0.3, 0.3), 0.1), 0.32, scale=(1.0, 0.9, 0.7), jitter=0.25, flatten_below=-0.05, seed=seed + 7)


def dead_tree(b, seed):
    rng = random.Random(seed)
    b.cone("Wood", (0, 0, 1.5), 0.2, 0.06, 3.0, segments=6, rot=(rng.uniform(-0.06, 0.06), rng.uniform(-0.06, 0.06), 0))
    for index in range(4):
        angle = index * math.pi * 0.5 + rng.uniform(-0.4, 0.4)
        b.cone("Wood", (math.cos(angle) * 0.22, math.sin(angle) * 0.22, 0.08), 0.1, 0.02, 0.5, segments=4, rot=(0, math.pi * 0.5 - 0.35, angle))
    for index in range(rng.randint(4, 5)):
        angle = index * 2.4 + rng.uniform(-0.3, 0.3)
        height = rng.uniform(1.5, 2.7)
        length = rng.uniform(0.7, 1.2)
        pitch = rng.uniform(0.6, 1.0)
        direction = Vector((math.cos(angle) * math.sin(pitch), math.sin(angle) * math.sin(pitch), math.cos(pitch)))
        center = Vector((0, 0, height)) + direction * length * 0.5
        rot = direction.to_track_quat("Z", "Y").to_euler()
        b.cone("Wood", center, 0.07, 0.015, length, segments=4, rot=rot)
        twig_dir = (direction + Vector((-direction.y, direction.x, 0.6))).normalized()
        twig_center = Vector((0, 0, height)) + direction * length * 0.7 + twig_dir * 0.2
        b.cone("Wood", twig_center, 0.03, 0.008, 0.4, segments=3, rot=twig_dir.to_track_quat("Z", "Y").to_euler())


def lantern_post(b):
    b.box("Wood", (0, 0, 1.1), (0.18, 0.18, 2.2))
    b.box("Iron", (0, 0, 2.23), (0.38, 0.38, 0.06))
    b.box("Flame", (0, 0, 2.42), (0.24, 0.24, 0.32))
    for x in (-1, 1):
        for y in (-1, 1):
            b.box("Iron", (x * 0.15, y * 0.15, 2.42), (0.04, 0.04, 0.34))
    b.cone("Iron", (0, 0, 2.68), 0.3, 0.0, 0.22, segments=4, rot=(0, 0, math.pi * 0.25))
    b.cone("Iron", (0, 0, 2.83), 0.04, 0.04, 0.1, segments=6)


def gem(b):
    b.bipyramid("Gem", [(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.0) for a in (i * math.pi * 2 / 6 for i in range(6))], (0, 0, 0.5), (0, 0, -0.5))


def blade(b):
    outline = [(-0.42, 0.0), (-0.3, 0.06), (-0.05, 0.1), (0.2, 0.08), (0.42, 0.0), (0.2, -0.03), (-0.05, -0.05), (-0.3, -0.04)]
    b.slab("Blade", outline, 0.03, ridge=0.025)


def bolt(b):
    b.bipyramid("Bolt", [(math.cos(a) * 0.11, 0.08, math.sin(a) * 0.11) for a in (i * math.pi * 2 / 6 for i in range(6))], (0, 0.4, 0), (0, -0.38, 0))


ASSETS = [
    ("Hero", hero),
    ("Ghoul", ghoul),
    ("Bat", bat_body),
    ("BatWingL", lambda b: bat_wing(b, -1)),
    ("BatWingR", lambda b: bat_wing(b, 1)),
    ("Brute", brute),
    ("Gravestone", gravestone),
    ("GraveCross", grave_cross),
    ("Rock0", lambda b: rock(b, 11)),
    ("Rock1", lambda b: rock(b, 23)),
    ("Rock2", lambda b: rock(b, 37)),
    ("DeadTree0", lambda b: dead_tree(b, 5)),
    ("DeadTree1", lambda b: dead_tree(b, 9)),
    ("LanternPost", lantern_post),
    ("Gem", gem),
    ("Blade", blade),
    ("Bolt", bolt),
]


def main():
    output = Path(sys.argv[sys.argv.index("--") + 1]) if "--" in sys.argv else Path(__file__).with_name("DuskSwarm.glb")

    bpy.ops.wm.read_factory_settings(use_empty=True)

    for index, (name, function) in enumerate(ASSETS):
        builder = Builder(name)
        function(builder)
        builder.build(((index % 6) * 3.0 + 1.0, (index // 6) * 3.0 + 1.0, 0.0))

    bpy.ops.wm.save_as_mainfile(filepath=str(output.with_suffix(".blend")))
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True, export_apply=True, export_normals=True, export_texcoords=True, export_materials="EXPORT", export_cameras=False, export_lights=False)


main()
