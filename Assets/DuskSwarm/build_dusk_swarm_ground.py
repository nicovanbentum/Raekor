import math
from pathlib import Path

import bpy
import numpy as np


OUT = Path(__file__).resolve().parent / "Ground"
RNG = np.random.default_rng(29)


def to_srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)


def band_noise(n, low, high):
    spectrum = np.fft.rfft2(RNG.standard_normal((n, n)))
    fy = np.fft.fftfreq(n)[:, None] * n
    fx = np.fft.rfftfreq(n)[None, :] * n
    f = np.maximum(np.sqrt(fx * fx + fy * fy), 1e-6)
    gain = 1.0 / np.sqrt(1.0 + (low / f) ** 4) / np.sqrt(1.0 + (f / high) ** 4)
    gain[0, 0] = 0.0
    out = np.fft.irfft2(spectrum * gain, (n, n))
    return out / (np.std(out) + 1e-9)


def blur(x, cycles):
    n = x.shape[0]
    spectrum = np.fft.rfft2(x)
    fy = np.fft.fftfreq(n)[:, None] * n
    fx = np.fft.rfftfreq(n)[None, :] * n
    f = np.sqrt(fx * fx + fy * fy)
    return np.fft.irfft2(spectrum * np.exp(-(f / cycles) ** 2), (n, n))


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, t):
    t = t[..., None]
    return np.asarray(a) * (1.0 - t) + np.asarray(b) * t


def strokes(n, count, length, height_range, width=1, steps=12):
    value = np.zeros((n, n))
    weight = np.zeros((n, n))
    x0 = RNG.uniform(0, n, count)
    y0 = RNG.uniform(0, n, count)
    angle = RNG.uniform(0, math.tau, count)
    lengths = RNG.uniform(length[0], length[1], count)
    heights = RNG.uniform(height_range[0], height_range[1], count)
    side_x, side_y = -np.sin(angle), np.cos(angle)
    for step in range(steps):
        t = step / (steps - 1)
        taper = 1.0 - 0.7 * t
        for offset in range(width):
            shift = (offset - (width - 1) * 0.5) * (1.0 - t * 0.8)
            x = np.round(x0 + np.cos(angle) * lengths * t + side_x * shift).astype(np.int64) % n
            y = np.round(y0 + np.sin(angle) * lengths * t + side_y * shift).astype(np.int64) % n
            np.maximum.at(value, (y, x), heights * taper)
            np.add.at(weight, (y, x), 1.0)
    return value, weight


def stamp_discs(n, count, radius, rng_value):
    height = np.zeros((n, n))
    mask = np.zeros((n, n))
    tone = np.zeros((n, n))
    shade = np.zeros((n, n))
    shadow = np.zeros((n, n))
    for _ in range(count):
        r = RNG.uniform(radius[0], radius[1])
        sx = RNG.uniform(0.6, 1.4)
        cx, cy = RNG.uniform(0, n), RNG.uniform(0, n)
        angle = RNG.uniform(0, math.pi)
        size = int(math.ceil(r * 2.0)) + 1
        ys = np.arange(int(cy) - size, int(cy) + size + 1)
        xs = np.arange(int(cx) - size, int(cx) + size + 1)
        dy, dx = np.meshgrid(ys - cy, xs - cx, indexing="ij")
        u = (dx * math.cos(angle) + dy * math.sin(angle)) / sx
        v = (-dx * math.sin(angle) + dy * math.cos(angle)) * sx
        d = np.sqrt(u * u + v * v) / r
        dome = np.sqrt(np.clip(1.0 - d * d, 0.0, 1.0))
        cover = smoothstep(1.0, 0.85, d)
        iy, ix = np.ix_(ys % n, xs % n)
        height[iy, ix] = np.maximum(height[iy, ix], dome * r * 0.12)
        tone[iy, ix] = np.where(cover > mask[iy, ix], rng_value(), tone[iy, ix])
        shade[iy, ix] = np.where(cover > mask[iy, ix], dome, shade[iy, ix])
        mask[iy, ix] = np.maximum(mask[iy, ix], cover)
        shadow[iy, ix] = np.maximum(shadow[iy, ix], smoothstep(1.45, 0.95, d))
    return height, mask, tone, shade, np.clip(shadow - mask, 0.0, 1.0)


def normal_map(height, strength):
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * strength
    normal = np.stack([-dx, dy, np.ones_like(height)], axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    return normal * 0.5 + 0.5


def save(name, rgb, alpha=None):
    OUT.mkdir(parents=True, exist_ok=True)
    n = rgb.shape[0]
    pixels = np.ones((n, n, 4), dtype=np.float32)
    pixels[..., :3] = np.clip(rgb, 0.0, 1.0)
    if alpha is not None:
        pixels[..., 3] = np.clip(alpha, 0.0, 1.0)
    image = bpy.data.images.new(name, n, n, alpha=True)
    image.colorspace_settings.name = "Non-Color"
    image.pixels.foreach_set(pixels.ravel())
    image.filepath_raw = str(OUT / f"{name}.png")
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)
    print(f"wrote {name}.png {n}x{n}")


def luminance(color):
    return color[..., 0] * 0.2126 + color[..., 1] * 0.7152 + color[..., 2] * 0.0722


def save_material(name, albedo, height, roughness, strength, brightness):
    albedo = albedo * (brightness / np.mean(luminance(albedo)))
    save(f"{name}Albedo", to_srgb(albedo))
    normal = normal_map(height, strength)
    save(f"{name}Normal", np.stack([normal[..., 0], normal[..., 1], normal[..., 2]], axis=-1), normal[..., 0])
    save(f"{name}Roughness", np.repeat(roughness[..., None], 3, axis=-1))


def grass():
    n = 2048
    patches = band_noise(n, 1.0, 5.0)
    clumps = band_noise(n, 8.0, 40.0)
    fine = band_noise(n, 80.0, 400.0)

    under_value, _ = strokes(n, 90000, (16.0, 34.0), (0.2, 0.6), width=2)
    over_value, _ = strokes(n, 70000, (14.0, 30.0), (0.5, 1.0), width=2)
    under = blur(under_value, 700.0)
    over = blur(over_value, 700.0)
    blades = np.maximum(under, over)

    dark = (0.022, 0.040, 0.016)
    mid = (0.055, 0.095, 0.032)
    light = (0.110, 0.165, 0.050)
    tip = (0.150, 0.190, 0.070)
    dry = (0.120, 0.110, 0.050)
    soil = (0.050, 0.034, 0.021)

    lush = smoothstep(-2.0, 2.0, patches * 0.6 + clumps * 0.4)
    albedo = mix(dark, mid, np.clip(under * 1.3, 0.0, 1.0))
    albedo = mix(albedo, light, np.clip(over * 1.2, 0.0, 1.0) * (0.6 + 0.4 * lush))
    albedo = mix(albedo, tip, smoothstep(0.75, 1.0, over) * 0.6)
    albedo = mix(albedo, dry, smoothstep(0.8, 2.2, -patches + clumps * 0.3) * 0.45)
    gaps = smoothstep(0.12, 0.0, blades)
    albedo = mix(albedo, soil, gaps * 0.7)
    albedo *= (0.85 + 0.15 * lush + 0.05 * fine)[..., None]

    height = blades * 2.0 + clumps * 0.25 + fine * 0.05
    roughness = np.clip(0.8 + fine * 0.03 - over * 0.08 + gaps * 0.12, 0.0, 1.0)
    save_material("Grass", albedo, height, roughness, 3.0, 0.24)


def dirt():
    n = 1024
    large = band_noise(n, 1.0, 5.0)
    mid = band_noise(n, 10.0, 50.0)
    fine = band_noise(n, 100.0, 400.0)

    pebble_height, pebble_mask, pebble_tone, pebble_shade, pebble_shadow = stamp_discs(n, 380, (2.5, 8.0), lambda: RNG.uniform(0.0, 1.0))
    grit_height, grit_mask, grit_tone, grit_shade, grit_shadow = stamp_discs(n, 1800, (0.8, 2.0), lambda: RNG.uniform(0.0, 1.0))

    tuft_value, _ = strokes(n, 9000, (6.0, 16.0), (0.3, 1.0), width=2)
    tufts = blur(tuft_value, 300.0) * smoothstep(0.6, 1.8, large + mid * 0.3)

    soil_dark = (0.045, 0.030, 0.018)
    soil = (0.085, 0.058, 0.035)
    soil_light = (0.130, 0.095, 0.060)
    stone_dark = (0.075, 0.062, 0.050)
    stone_light = (0.170, 0.150, 0.125)
    moss = (0.055, 0.085, 0.028)

    albedo = mix(soil_dark, soil, smoothstep(-1.5, 1.2, large * 0.6 + mid * 0.6))
    albedo = mix(albedo, soil_light, smoothstep(0.8, 2.4, mid + fine * 0.4) * 0.5)
    albedo *= (0.88 + 0.12 * fine)[..., None]
    pebbles = mix(stone_dark, stone_light, pebble_tone) * (0.55 + 0.45 * pebble_shade)[..., None]
    grit = mix(soil_dark, soil_light, grit_tone) * (0.6 + 0.5 * grit_shade)[..., None]
    albedo *= (1.0 - 0.45 * np.maximum(pebble_shadow, grit_shadow * 0.6))[..., None]
    albedo = mix(albedo, grit, grit_mask * 0.8)
    albedo = mix(albedo, pebbles, pebble_mask)
    albedo = mix(albedo, moss, np.clip(tufts * 0.8, 0.0, 1.0))

    height = mid * 0.25 + fine * 0.12 + pebble_height + grit_height * 0.6 + tufts * 0.5
    roughness = np.clip(0.93 + fine * 0.03 - pebble_mask * 0.2, 0.0, 1.0)
    save_material("Dirt", albedo, height, roughness, 2.0, 0.21)


def stone():
    n = 1024
    seeds = []
    columns = 4
    for row in range(columns):
        for column in range(columns):
            seeds.append(((column + RNG.uniform(0.15, 0.85)) * n / columns, (row + RNG.uniform(0.15, 0.85)) * n / columns))

    ys, xs = np.mgrid[0:n, 0:n].astype(np.float64)
    warp_x = band_noise(n, 4.0, 16.0) * 9.0
    warp_y = band_noise(n, 4.0, 16.0) * 9.0
    px, py = xs + warp_x, ys + warp_y

    d1 = np.full((n, n), 1e9)
    d2 = np.full((n, n), 1e9)
    cell = np.zeros((n, n), dtype=np.int64)
    for index, (sx, sy) in enumerate(seeds):
        for ox in (-n, 0, n):
            for oy in (-n, 0, n):
                d = np.hypot(px - sx - ox, py - sy - oy)
                closer = d < d1
                d2 = np.where(closer, d1, np.minimum(d2, d))
                cell = np.where(closer, index, cell)
                d1 = np.where(closer, d, d1)

    edge = d2 - d1
    stone_mask = smoothstep(5.0, 13.0, edge)

    cell_tone = RNG.uniform(0.0, 1.0, len(seeds))[cell]
    cell_tilt_x = RNG.uniform(-1.0, 1.0, len(seeds))[cell]

    mid = band_noise(n, 6.0, 40.0)
    fine = band_noise(n, 80.0, 400.0)
    stains = band_noise(n, 2.0, 8.0)

    stone_dark = (0.070, 0.070, 0.075)
    stone_light = (0.160, 0.155, 0.150)
    grout = (0.035, 0.028, 0.020)
    moss = (0.050, 0.085, 0.028)

    bevel = smoothstep(2.0, 20.0, edge)
    albedo = mix(stone_dark, stone_light, np.clip(cell_tone * 0.7 + mid * 0.12 + 0.15, 0.0, 1.0))
    albedo *= (0.9 + 0.1 * fine)[..., None] * (0.65 + 0.35 * bevel)[..., None]
    albedo = mix(albedo, albedo * 0.6, smoothstep(0.5, 2.0, stains) * 0.6)
    albedo = mix(grout, albedo, stone_mask)
    moss_amount = (1.0 - smoothstep(2.0, 16.0, edge)) * smoothstep(-0.3, 1.0, mid + stains * 0.5)
    albedo = mix(albedo, moss, np.clip(moss_amount, 0.0, 1.0) * 0.85)

    height = bevel * 6.0 + (mid * 0.3 + fine * 0.15 + cell_tilt_x * 0.8) * stone_mask
    roughness = np.clip(0.72 + fine * 0.05 + (1.0 - stone_mask) * 0.25 + moss_amount * 0.1, 0.0, 1.0)
    save_material("Stone", albedo, height, roughness, 0.9, 0.36)


grass()
dirt()
stone()
