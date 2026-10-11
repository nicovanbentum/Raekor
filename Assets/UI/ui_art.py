import math
import numpy as np


class Canvas:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.rgba = np.zeros((height, width, 4), dtype=np.float32)
        ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
        self.x = xs + 0.5
        self.y = ys + 0.5

    def over(self, color, alpha):
        alpha = np.clip(alpha, 0.0, 1.0)[..., None]
        color = np.broadcast_to(np.asarray(color, dtype=np.float32)[..., :3], self.rgba[..., :3].shape)
        dst_a = self.rgba[..., 3:4]
        out_a = alpha + dst_a * (1.0 - alpha)
        out_rgb = (color * alpha + self.rgba[..., :3] * dst_a * (1.0 - alpha)) / np.maximum(out_a, 1e-6)
        self.rgba[..., :3] = out_rgb
        self.rgba[..., 3:4] = out_a

    def fill(self, sdf, color, opacity=1.0, aa=1.0):
        self.over(color, coverage(sdf, aa) * opacity)

    def save(self, path):
        import bpy
        image = bpy.data.images.new(path.stem, self.width, self.height, alpha=True)
        image.colorspace_settings.name = "Non-Color"
        image.pixels.foreach_set(np.flipud(np.clip(self.rgba, 0.0, 1.0)).ravel())
        image.filepath_raw = str(path)
        image.file_format = "PNG"
        image.save()
        bpy.data.images.remove(image)


def coverage(sdf, aa=1.0):
    return np.clip(0.5 - sdf / aa, 0.0, 1.0)


def box(c, cx, cy, hw, hh, r=0.0):
    qx = np.abs(c.x - cx) - hw + r
    qy = np.abs(c.y - cy) - hh + r
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def rotated_box(c, cx, cy, hw, hh, angle, r=0.0):
    s, co = math.sin(-angle), math.cos(-angle)
    dx, dy = c.x - cx, c.y - cy
    lx, ly = dx * co - dy * s, dx * s + dy * co
    qx = np.abs(lx) - hw + r
    qy = np.abs(ly) - hh + r
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def circle(c, cx, cy, radius):
    return np.hypot(c.x - cx, c.y - cy) - radius


def ring(c, cx, cy, radius, thickness):
    return np.abs(np.hypot(c.x - cx, c.y - cy) - radius) - thickness * 0.5


def segment(c, ax, ay, bx, by, radius):
    pax, pay = c.x - ax, c.y - ay
    bax, bay = bx - ax, by - ay
    h = np.clip((pax * bax + pay * bay) / max(bax * bax + bay * bay, 1e-6), 0.0, 1.0)
    return np.hypot(pax - bax * h, pay - bay * h) - radius


def polygon(c, points):
    d = np.full(c.x.shape, 1e9, dtype=np.float32)
    sign = np.ones(c.x.shape, dtype=np.float32)
    count = len(points)
    for i in range(count):
        ax, ay = points[i]
        bx, by = points[(i + 1) % count]
        ex, ey = bx - ax, by - ay
        wx, wy = c.x - ax, c.y - ay
        h = np.clip((wx * ex + wy * ey) / max(ex * ex + ey * ey, 1e-6), 0.0, 1.0)
        d = np.minimum(d, np.hypot(wx - ex * h, wy - ey * h))
        cond_a = c.y >= ay
        cond_b = c.y < by
        cond_c = ex * wy > ey * wx
        flip = (cond_a & cond_b & cond_c) | (~cond_a & ~cond_b & ~cond_c)
        sign = np.where(flip, -sign, sign)
    return d * sign


def star(c, cx, cy, outer, inner, points=5, rotation=-math.pi * 0.5):
    vertices = []
    for i in range(points * 2):
        a = rotation + i * math.pi / points
        r = outer if i % 2 == 0 else inner
        vertices.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return polygon(c, vertices)


def union(*sdfs):
    out = sdfs[0]
    for s in sdfs[1:]:
        out = np.minimum(out, s)
    return out


def subtract(a, b):
    return np.maximum(a, -b)


def noise(c, cell, seed, octaves=3):
    rng = np.random.default_rng(seed)
    total = np.zeros(c.x.shape, dtype=np.float32)
    amplitude, norm = 1.0, 0.0
    for octave in range(octaves):
        size = max(cell / (2 ** octave), 1.0)
        gw = int(math.ceil(c.width / size)) + 2
        gh = int(math.ceil(c.height / size)) + 2
        grid = rng.random((gh, gw)).astype(np.float32)
        gx = c.x / size
        gy = c.y / size
        x0 = np.floor(gx).astype(int)
        y0 = np.floor(gy).astype(int)
        fx = gx - x0
        fy = gy - y0
        fx = fx * fx * (3 - 2 * fx)
        fy = fy * fy * (3 - 2 * fy)
        v00 = grid[y0 % gh, x0 % gw]
        v10 = grid[y0 % gh, (x0 + 1) % gw]
        v01 = grid[(y0 + 1) % gh, x0 % gw]
        v11 = grid[(y0 + 1) % gh, (x0 + 1) % gw]
        total += amplitude * ((v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy)
        norm += amplitude
        amplitude *= 0.5
    return total / norm


def bevel(sdf, width, light=(-0.6, -0.8)):
    gy, gx = np.gradient(sdf)
    length = np.maximum(np.hypot(gx, gy), 1e-6)
    nx, ny = gx / length, gy / length
    facing = -(nx * light[0] + ny * light[1])
    band = np.clip(1.0 + sdf / width, 0.0, 1.0) * (sdf <= 0)
    return facing * band


def shade(color, amount):
    return np.clip(np.asarray(color, dtype=np.float32)[None, None, :3] * (1.0 + amount[..., None]), 0.0, 1.0)


def mix(a, b, t):
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    t = np.asarray(t, dtype=np.float32)[..., None]
    return a[None, None, :3] * (1 - t) + b[None, None, :3] * t


def hex_color(value, alpha=1.0):
    value = value.lstrip("#")
    return (int(value[0:2], 16) / 255.0, int(value[2:4], 16) / 255.0, int(value[4:6], 16) / 255.0, alpha)
