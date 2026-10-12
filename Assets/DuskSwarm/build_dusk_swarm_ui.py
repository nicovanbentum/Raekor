import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "UI"))

import numpy as np
from ui_art import Canvas, box, rotated_box, circle, ring, segment, polygon, star, union, subtract, noise, bevel, shade, mix, hex_color, coverage


GOLD = hex_color("e2b04a")
GOLD_DARK = hex_color("7a5420")
IRON = hex_color("1b1820")
LEATHER_TOP = hex_color("1e1628")
LEATHER_BOTTOM = hex_color("0b0910")
RUBY = hex_color("c0182c")
BONE = hex_color("e6dcc2")


def gold_fill(c, sdf, width=4.0):
    light = bevel(sdf, width)
    grain = noise(c, 6, 7, 2) - 0.5
    c.over(shade(GOLD, light * 0.55 + grain * 0.15), coverage(sdf))


def heart(directory):
    c = Canvas(64, 64)
    shape = union(circle(c, 23, 25, 13), circle(c, 41, 25, 13), polygon(c, [(11.5, 30), (52.5, 30), (32, 54)]))
    c.fill(shape - 3, (0.08, 0.0, 0.02))
    light = bevel(shape, 7.0)
    c.over(shade(RUBY, light * 0.6), coverage(shape))
    c.fill(rotated_box(c, 22, 21, 5, 3, -0.6, 2.5), (1, 0.85, 0.85), 0.75)
    c.save(directory / "Heart.png")


def gem(directory):
    c = Canvas(64, 64)
    outline = [(32 + math.cos(a) * 27, 32 + math.sin(a) * 27) for a in (i * math.tau / 8 + math.pi / 8 for i in range(8))]
    shape = polygon(c, outline)
    c.fill(shape - 3, (0.05, 0.04, 0.06))
    c.fill(shape, (0.55, 0.55, 0.55))
    for i in range(8):
        a0 = i * math.tau / 8 + math.pi / 8
        a1 = a0 + math.tau / 8
        facet = polygon(c, [(32, 32), (32 + math.cos(a0) * 27, 32 + math.sin(a0) * 27), (32 + math.cos(a1) * 27, 32 + math.sin(a1) * 27)])
        value = 0.45 + 0.45 * (0.5 + 0.5 * math.cos((a0 + a1) * 0.5 + 2.2))
        c.fill(facet, (value, value, value))
    table = polygon(c, [(32 + math.cos(a) * 13, 32 + math.sin(a) * 13) for a in (i * math.tau / 8 + math.pi / 8 for i in range(8))])
    c.fill(table, (0.95, 0.95, 0.95))
    c.fill(rotated_box(c, 25, 24, 5, 2, -0.7, 1.5), (1, 1, 1), 0.9)
    c.save(directory / "Gem.png")


OUTLINE = (0.06, 0.04, 0.07)
STEEL = hex_color("b9c0cc")
LEATHER = hex_color("6e4024")
WOOD = hex_color("7a4a26")
FIRE_RAMP = [(0.0, hex_color("6e0f08")), (0.3, hex_color("d2380e")), (0.6, hex_color("ff9a24")), (0.85, hex_color("ffe27a")), (1.0, hex_color("fff8e0"))]


def ramp(t, stops):
    t = np.clip(t, 0.0, 1.0)
    out = np.zeros(t.shape + (3,), dtype=np.float32)
    for (t0, c0), (t1, c1) in zip(stops[:-1], stops[1:]):
        f = np.clip((t - t0) / max(t1 - t0, 1e-6), 0.0, 1.0)
        inside = (t >= t0) & (t <= t1)
        out[inside] = (np.asarray(c0[:3]) * (1 - f[..., None]) + np.asarray(c1[:3]) * f[..., None])[inside]
    return out


def depth(sdf, amount):
    return np.clip(-sdf / amount, 0.0, 1.0)


def paint(c, sdf, base, light=0.6, width=6.0, grain=0.12, seed=1, cell=6, opacity=1.0):
    base = np.asarray(base, dtype=np.float32)
    if base.ndim == 1:
        base = np.broadcast_to(base[:3], c.rgba[..., :3].shape)
    amount = bevel(sdf, width) * light + (noise(c, cell, seed, 3) - 0.5) * grain
    c.over(np.clip(base * (1.0 + amount[..., None]), 0.0, 1.0), coverage(sdf) * opacity)


def glow(c, sdf, color, radius, strength):
    edge = np.minimum(np.minimum(c.x, c.width - c.x), np.minimum(c.y, c.height - c.y))
    c.over(color, np.exp(-np.maximum(sdf, 0.0) / radius) * strength * np.clip(edge / 14.0, 0.0, 1.0))


def drop_shadow(c, sdf, dx=2, dy=4, opacity=0.55):
    c.fill(np.roll(np.roll(sdf, dy, 0), dx, 1) - 1.5, (0, 0, 0), opacity, aa=5.0)


def outline(c, sdf, width=3.0):
    c.fill(sdf - width, OUTLINE)


def rotate(points, cx, cy, angle):
    co, s = math.cos(angle), math.sin(angle)
    return [(cx + x * co - y * s, cy + x * s + y * co) for x, y in points]


def ellipse(c, cx, cy, rx, ry, angle=0.0):
    s, co = math.sin(-angle), math.cos(-angle)
    dx, dy = c.x - cx, c.y - cy
    lx, ly = dx * co - dy * s, dx * s + dy * co
    return (np.hypot(lx / rx, ly / ry) - 1.0) * min(rx, ry)


def leaf(c, cx, cy, length, width, angle):
    radius = (length * length + width * width) / (2.0 * width)
    offset = radius - width
    px, py = -math.sin(angle) * offset, math.cos(angle) * offset
    return np.maximum(circle(c, cx + px, cy + py, radius), circle(c, cx - px, cy - py, radius))


def tapered(c, ax, ay, bx, by, ra, rb):
    pax, pay = c.x - ax, c.y - ay
    bax, bay = bx - ax, by - ay
    h = np.clip((pax * bax + pay * bay) / max(bax * bax + bay * bay, 1e-6), 0.0, 1.0)
    return np.hypot(pax - bax * h, pay - bay * h) - (ra + (rb - ra) * h)


def sparkle(c, cx, cy, size, color, opacity=1.0):
    c.fill(star(c, cx, cy, size, size * 0.22, 4, 0.0), color, opacity)


def icon_arcane_bolt(c):
    ux, uy = 1 / math.sqrt(2), -1 / math.sqrt(2)
    cx, cy = 72, 56
    for index, (spread, length) in enumerate(((0.0, 64), (-11.0, 46), (11.0, 46))):
        bx, by = cx - ux * length + uy * spread * -1, cy - uy * length + ux * spread
        wisp = tapered(c, cx - ux * 8, cy - uy * 8, bx, by, 9 - abs(spread) * 0.3, 1.0)
        glow(c, wisp, hex_color("3a7bff"), 6.0, 0.35)
        c.over(ramp(depth(wisp, 6.0), [(0.0, hex_color("3a7bff")), (1.0, hex_color("c8ecff"))]), coverage(wisp) * (0.75 if index == 0 else 0.5))

    tip, side, tail = 40, 18, 26
    points = [(cx + ux * tip, cy + uy * tip), (cx - uy * side, cy + ux * side), (cx - ux * tail, cy - uy * tail), (cx + uy * side, cy - ux * side)]
    crystal = polygon(c, points)
    glow(c, crystal, hex_color("4fa8ff"), 9.0, 0.7)
    outline(c, crystal, 2.5)
    paint(c, crystal, hex_color("2a5fd0"), 0.5, 5.0, 0.1, 31)
    c.fill(polygon(c, [points[0], points[1], points[2]]), hex_color("9fe0ff"), 0.55)
    inner = [(cx + (x - cx) * 0.45 + ux * 6, cy + (y - cy) * 0.45 + uy * 6) for x, y in points]
    c.fill(polygon(c, inner), hex_color("f0fbff"), 0.9)
    sparkle(c, 30, 30, 10, hex_color("dff4ff"))
    sparkle(c, 104, 92, 7, hex_color("9fe0ff"))
    sparkle(c, 48, 106, 5, hex_color("9fe0ff"), 0.8)


def icon_orbiting_blades(c):
    blades = []
    for index in range(3):
        a = index * math.tau / 3 - math.pi * 0.5
        dx, dy = math.cos(a), math.sin(a)
        px, py = -dy, dx
        bx, by = 64 + dx * 26, 64 + dy * 26
        blades.append(subtract(circle(c, bx, by, 30), circle(c, bx + px * 13 + dx * 6, by + py * 13 + dy * 6, 30)))

    shape = union(*blades)
    drop_shadow(c, shape)
    outline(c, shape, 2.5)
    for index, blade in enumerate(blades):
        paint(c, blade, STEEL, 0.9, 7.0, 0.1, 40 + index)
        c.fill(np.maximum(blade, -blade - 2.5), (1, 1, 1), 0.55)

    hub = circle(c, 64, 64, 15)
    outline(c, hub, 2.5)
    gold_fill(c, hub, 6.0)
    c.fill(circle(c, 64, 64, 6.5), RUBY)
    c.fill(circle(c, 62, 62, 2.2), (1, 0.85, 0.85), 0.9)


def icon_nova(c):
    burst = star(c, 64, 64, 60, 30, 12, 0.0)
    glow(c, burst, hex_color("a050ff"), 8.0, 0.5)
    outline(c, burst, 2.0)
    c.over(ramp(depth(burst - 0, 34.0), [(0.0, hex_color("5a1aa8")), (0.5, hex_color("b36bff")), (1.0, hex_color("f4dcff"))]), coverage(burst))
    inner = star(c, 64, 64, 44, 22, 12, math.pi / 12)
    c.over(ramp(depth(inner, 26.0), [(0.0, hex_color("c58cff")), (1.0, hex_color("ffffff"))]), coverage(inner) * 0.8)
    core = circle(c, 64, 64, 15)
    glow(c, core, (1, 1, 1), 6.0, 0.7)
    c.fill(core, hex_color("fff4ff"))
    sparkle(c, 98, 26, 7, (1, 1, 1))


def flame(c, sdf, seed, distortion=7.0, cell=10):
    return sdf + (noise(c, cell, seed, 3) - 0.5) * distortion


def icon_fireball(c):
    body = union(
        circle(c, 74, 54, 30),
        tapered(c, 74, 54, 18, 112, 28, 2),
        tapered(c, 70, 58, 30, 92, 22, 2),
        tapered(c, 78, 60, 50, 116, 20, 2),
    )
    body = flame(c, body, 51, 9.0, 12)
    glow(c, body, hex_color("ff6a10"), 10.0, 0.6)
    outline(c, body, 2.0)
    c.over(ramp(depth(body, 30.0), FIRE_RAMP), coverage(body))
    core = flame(c, circle(c, 78, 50, 16), 52, 5.0, 8)
    c.over(hex_color("fffbe8"), coverage(core, 4.0) * 0.85)
    for x, y, r in ((106, 22, 4), (32, 36, 3), (110, 76, 3)):
        c.fill(circle(c, x, y, r), hex_color("ffc54a"), 0.9)


def icon_chain_lightning(c):
    bolt = polygon(c, [(84, 4), (40, 68), (62, 68), (44, 124), (98, 50), (72, 50), (100, 4)])
    branch = union(segment(c, 52, 66, 34, 72, 2.4), segment(c, 34, 72, 26, 88, 1.6), segment(c, 88, 50, 104, 44, 2.0), segment(c, 104, 44, 114, 50, 1.4))
    shape = union(bolt, branch)
    glow(c, shape, hex_color("5aa8ff"), 12.0, 0.75)
    outline(c, shape, 2.5)
    c.over(ramp(depth(shape, 9.0), [(0.0, hex_color("3c78e6")), (0.45, hex_color("a8dcff")), (1.0, hex_color("ffffff"))]), coverage(shape))
    sparkle(c, 30, 24, 8, hex_color("e8f6ff"))
    sparkle(c, 104, 104, 6, hex_color("a8dcff"))


def icon_meteor(c):
    trail = flame(c, union(tapered(c, 78, 50, 10, 118, 30, 2), tapered(c, 70, 56, 34, 120, 18, 1), tapered(c, 84, 60, 56, 124, 14, 1)), 61, 10.0, 12)
    fade = np.clip(1.0 - np.hypot(c.x - 78, c.y - 50) / 110.0, 0.0, 1.0)
    glow(c, trail, hex_color("ff5010"), 8.0, 0.45)
    c.over(ramp(depth(trail, 22.0) * 0.6 + fade * 0.4, FIRE_RAMP), coverage(trail) * (0.35 + 0.65 * fade))

    rock = circle(c, 82, 46, 27) + (noise(c, 9, 62, 3) - 0.5) * 9.0
    outline(c, rock, 2.5)
    mottling = noise(c, 7, 63, 3)
    base = mix(hex_color("2e2420"), hex_color("6b5446"), mottling)
    paint(c, rock, base, 0.8, 9.0, 0.15, 64)
    for x, y, r in ((92, 36, 6), (74, 54, 4.5), (96, 58, 3.5)):
        c.fill(circle(c, x, y, r), hex_color("221a17"), 0.8)
        c.fill(ring(c, x + 1, y + 1, r, 1.2), hex_color("8a7262"), 0.5)
    for ax, ay, bx, by in ((62, 52, 74, 46), (74, 46, 78, 34), (66, 64, 80, 62)):
        crack = segment(c, ax, ay, bx, by, 1.4)
        glow(c, crack, hex_color("ff7a1a"), 2.5, 0.6)
        c.fill(crack, hex_color("ffd060"))
    c.over(hex_color("ff7a1a"), coverage(rock) * np.clip((96 - c.x + c.y - 46) / 60.0, 0, 1) ** 2 * 0.45)


def icon_swift_boots(c):
    feathers = []
    for index, t in enumerate((0.75, 0.38, 0.02)):
        dx, dy = -math.cos(t), -math.sin(t)
        ax, ay = 42, 46 + index * 9
        feathers.append(ellipse(c, ax + dx * 20, ay + dy * 20, 21 - index * 3, 6.0, math.atan2(dy, dx)))
    wing = union(*feathers)
    boot = polygon(c, [(42, 12), (78, 12), (80, 66), (108, 78), (114, 92), (114, 104), (40, 104), (38, 66)]) - 3.0
    sole = box(c, 76, 108, 38, 6, 3)
    shape = union(wing, boot, sole)
    drop_shadow(c, shape)
    outline(c, shape, 2.5)
    for index, feather in enumerate(feathers):
        if index > 0:
            outline(c, feather, 1.5)
        paint(c, feather, hex_color("eef2f6"), 0.6, 5.0, 0.06, 70 + index)
    c.over(np.clip(mix(hex_color("8a5530"), hex_color("4a2a16"), np.clip(c.y / 110.0, 0, 1)), 0, 1), coverage(boot))
    c.over((1, 1, 1), np.clip(bevel(boot, 7.0), 0, 1) * 0.25)
    c.over((0, 0, 0), np.clip(-bevel(boot, 7.0), 0, 1) * 0.35)
    c.fill(box(c, 60, 20, 22, 8, 3), hex_color("a06a3e"))
    c.fill(box(c, 60, 27, 22, 1.2), OUTLINE, 0.6)
    paint(c, sole, hex_color("2a1c14"), 0.5, 4.0, 0.1, 72)
    strap = box(c, 60, 56, 23, 4.5, 1.5)
    c.fill(strap, hex_color("3a2214"))
    buckle = subtract(box(c, 70, 56, 7, 7, 2), box(c, 70, 56, 3.5, 3.5, 1))
    gold_fill(c, buckle, 3.0)
    for index in range(7):
        y = 34 + index * 9
        if y < 50 or y > 62:
            c.fill(box(c, 77, y, 0.9, 2.5), hex_color("e0c08a"), 0.7)


def icon_vitality(c):
    shape = union(circle(c, 46, 48, 27), circle(c, 82, 48, 27), polygon(c, [(21.5, 58), (106.5, 58), (64, 114)]))
    drop_shadow(c, shape - 4)
    outline(c, shape - 4, 3.0)
    gold_fill(c, subtract(shape - 4, shape + 3), 4.0)
    gem = shape + 3
    c.over(ramp(depth(gem, 30.0), [(0.0, hex_color("5a0612")), (0.55, hex_color("c0182c")), (1.0, hex_color("ff6a72"))]), coverage(gem))
    c.over((1, 1, 1), np.clip(bevel(gem, 8.0), 0, 1) * coverage(gem) * 0.35)
    c.fill(ellipse(c, 44, 40, 10, 5, -0.6), (1, 0.9, 0.9), 0.75)
    c.fill(box(c, 82, 74, 10, 3, 1), BONE, 0.95)
    c.fill(box(c, 82, 74, 3, 10, 1), BONE, 0.95)


def icon_magnet(c):
    arc = np.maximum(ring(c, 64, 64, 30, 24), 64 - c.y)
    arms = union(box(c, 34, 44, 12, 22), box(c, 94, 44, 12, 22))
    body = union(arc, arms)
    tips = union(box(c, 34, 22, 12, 9, 1), box(c, 94, 22, 12, 9, 1))
    shape = union(body, tips)
    drop_shadow(c, shape)
    outline(c, shape, 3.0)
    paint(c, body, hex_color("b5202a"), 0.8, 8.0, 0.12, 80)
    paint(c, tips, hex_color("c8ccd4"), 0.9, 5.0, 0.08, 81)
    c.fill(box(c, 34, 31, 12, 1.2), OUTLINE, 0.6)
    c.fill(box(c, 94, 31, 12, 1.2), OUTLINE, 0.6)
    c.fill(ellipse(c, 26, 52, 2.5, 12), (1, 0.8, 0.8), 0.5)
    sparkle(c, 64, 14, 9, hex_color("fff2c8"))
    sparkle(c, 64, 40, 5, hex_color("fff2c8"), 0.8)


def icon_regeneration(c):
    stem = union(segment(c, 44, 120, 56, 88, 3.0), segment(c, 56, 88, 62, 56, 2.6), segment(c, 62, 56, 66, 32, 2.2))
    left = leaf(c, 38, 74, 26, 11, -2.5)
    right = leaf(c, 88, 80, 26, 11, -0.35)
    small = leaf(c, 80, 48, 16, 7, -0.9)
    leaves = union(left, right, small)
    petals = union(*[circle(c, 66 + math.cos(a) * 11, 26 + math.sin(a) * 11, 9) for a in (i * math.tau / 5 - math.pi * 0.5 for i in range(5))])
    shape = union(stem, leaves, petals)
    glow(c, shape, hex_color("7aff9a"), 8.0, 0.25)
    drop_shadow(c, shape)
    outline(c, shape, 2.5)
    paint(c, stem, hex_color("4a7a2a"), 0.5, 3.0, 0.1, 90)
    c.over(ramp(depth(leaves, 10.0), [(0.0, hex_color("2c6a24")), (1.0, hex_color("8ad050"))]), coverage(leaves))
    for cx, cy, length, angle in ((38, 74, 24, -2.5), (88, 80, 24, -0.35), (80, 48, 14, -0.9)):
        dx, dy = math.cos(angle) * length, math.sin(angle) * length
        c.fill(segment(c, cx - dx, cy - dy, cx + dx, cy + dy, 0.9), hex_color("c8f0a0"), 0.7)
    c.over(ramp(depth(petals, 9.0), [(0.0, hex_color("c8407a")), (1.0, hex_color("ffb4d2"))]), coverage(petals))
    c.fill(circle(c, 66, 26, 5.5), hex_color("ffd84a"))
    for x, y, s in ((100, 30, 6), (24, 40, 5), (106, 108, 5)):
        sparkle(c, x, y, s, hex_color("d8ffd0"))


def icon_power(c):
    ux, uy = 1 / math.sqrt(2), -1 / math.sqrt(2)
    px, py = -uy, ux
    gx, gy = 44, 84
    blade = polygon(c, [(gx + px * 8 + ux * 2, gy + py * 8 + uy * 2), (gx + ux * 62 + px * 6, gy + uy * 62 + py * 6), (gx + ux * 76, gy + uy * 76), (gx + ux * 62 - px * 6, gy + uy * 62 - py * 6), (gx - px * 8 + ux * 2, gy - py * 8 + uy * 2)])
    guard = rotated_box(c, gx, gy, 6, 22, math.atan2(uy, ux), 3)
    grip = segment(c, gx - ux * 6, gy - uy * 6, gx - ux * 24, gy - uy * 24, 5.0)
    pommel = circle(c, gx - ux * 30, gy - uy * 30, 8)
    shape = union(blade, guard, grip, pommel)
    glow(c, blade, hex_color("ffcf5a"), 8.0, 0.35)
    drop_shadow(c, shape)
    outline(c, shape, 2.5)
    side = (c.x - gx) * px + (c.y - gy) * py
    c.over(np.clip(np.where(side[..., None] < 0, np.asarray(STEEL[:3]) * 1.15, np.asarray(STEEL[:3]) * 0.75), 0, 1), coverage(blade))
    c.fill(segment(c, gx + ux * 6, gy + uy * 6, gx + ux * 52, gy + uy * 52, 1.4), hex_color("5a6070"), 0.8)
    c.fill(np.maximum(blade, -blade - 1.6), (1, 1, 1), 0.6)
    paint(c, grip, LEATHER, 0.5, 4.0, 0.1, 100)
    for index in range(4):
        t = 9 + index * 4.5
        c.fill(segment(c, gx - ux * t - px * 5, gy - uy * t - py * 5, gx - ux * t + px * 5, gy - uy * t + py * 5, 0.8), OUTLINE, 0.6)
    gold_fill(c, guard, 5.0)
    gold_fill(c, pommel, 5.0)
    c.fill(circle(c, gx - ux * 30, gy - uy * 30, 3.5), RUBY)


def icon_haste(c):
    glass = polygon(c, [(40, 24), (88, 24), (88, 34), (68, 62), (68, 66), (88, 94), (88, 104), (40, 104), (40, 94), (60, 66), (60, 62), (40, 34)]) - 3.0
    caps = union(box(c, 64, 15, 40, 8, 3), box(c, 64, 113, 40, 8, 3))
    posts = union(box(c, 30, 64, 4.5, 44, 2), box(c, 98, 64, 4.5, 44, 2))
    shape = union(glass, caps, posts)
    drop_shadow(c, shape)
    outline(c, shape, 2.5)
    c.fill(glass, hex_color("9ec8e0"), 0.35)
    sand_top = np.maximum(glass + 3.0, 44 - c.y)
    sand_bottom = np.maximum(glass + 3.0, union(polygon(c, [(42, 104), (86, 104), (64, 84)]), 98 - c.y))
    sand = union(sand_top, sand_bottom, box(c, 64, 80, 1.2, 16))
    paint(c, sand, hex_color("e2b04a"), 0.4, 4.0, 0.25, 110, 3)
    c.fill(ellipse(c, 48, 36, 2.5, 7, -0.5), (1, 1, 1), 0.6)
    c.fill(ellipse(c, 50, 92, 2.0, 5, 0.5), (1, 1, 1), 0.45)
    paint(c, posts, WOOD, 0.6, 3.0, 0.15, 111)
    paint(c, caps, WOOD, 0.7, 5.0, 0.18, 112, 4)
    c.fill(box(c, 64, 15, 34, 1.0), GOLD, 0.7)
    c.fill(box(c, 64, 113, 34, 1.0), GOLD, 0.7)


def icon_area(c):
    disc = circle(c, 64, 64, 50)
    glow(c, disc, hex_color("b070ff"), 10.0, 0.35)
    c.over(np.clip(mix(LEATHER_TOP, LEATHER_BOTTOM, np.clip(c.y / 128.0, 0, 1)) * 1.6, 0, 1), coverage(disc) * 0.9)
    c.fill(ring(c, 64, 64, 48, 6), IRON)
    gold_fill(c, ring(c, 64, 64, 47, 3.5), 2.0)
    gold_fill(c, ring(c, 64, 64, 30, 2.5), 1.5)
    for index in range(8):
        a = index * math.tau / 8 + math.pi / 8
        x, y = 64 + math.cos(a) * 38, 64 + math.sin(a) * 38
        glyph = union(segment(c, x - 2.5, y - 4, x + 2.5, y + 4, 0.8), segment(c, x + 2.5, y - 4, x - 1, y, 0.8))
        c.fill(glyph, hex_color("e0b8ff"), 0.9)
    core = star(c, 64, 64, 18, 7, 4, 0.0)
    glow(c, core, hex_color("c890ff"), 5.0, 0.6)
    c.fill(core, hex_color("f4e6ff"))
    for index in range(4):
        a = index * math.pi * 0.5 - math.pi * 0.5
        dx, dy = math.cos(a), math.sin(a)
        tip = (64 + dx * 63, 64 + dy * 63)
        arrow = polygon(c, [tip, (64 + dx * 50 - dy * 10, 64 + dy * 50 + dx * 10), (64 + dx * 50 + dy * 10, 64 + dy * 50 - dx * 10)])
        outline(c, arrow, 2.0)
        gold_fill(c, arrow, 3.0)


def icon_feast(c):
    meat = union(ellipse(c, 52, 56, 38, 30, -0.75), tapered(c, 56, 60, 86, 90, 20, 8))
    bone = union(segment(c, 82, 86, 102, 106, 6.0), circle(c, 98, 115, 7.5), circle(c, 111, 102, 7.5))
    shape = union(meat, bone)
    drop_shadow(c, shape)
    outline(c, shape, 3.0)
    paint(c, bone, BONE, 0.6, 5.0, 0.1, 120)
    roast = ramp(depth(meat, 20.0) + (noise(c, 6, 121, 3) - 0.5) * 0.35, [(0.0, hex_color("4a1c0a")), (0.45, hex_color("9a4a1a")), (1.0, hex_color("d88a3a"))])
    c.over(roast, coverage(meat))
    c.over((1, 1, 1), np.clip(bevel(meat, 10.0), 0, 1) * coverage(meat) * 0.3)
    c.fill(ellipse(c, 38, 40, 12, 5, -0.75), hex_color("ffd9a0"), 0.55)
    for x, y in ((44, 66), (58, 52), (66, 70), (34, 52)):
        c.fill(segment(c, x - 4, y + 3, x + 4, y - 3, 1.2), hex_color("3a1406"), 0.6)


def icon_dash(c):
    for index, (y, length) in enumerate(((44, 42), (64, 54), (84, 40))):
        streak = tapered(c, 60, y + 10, 60 - length, y + 10 - length * 0.35, 3.0, 0.5)
        c.fill(streak, hex_color("bfe6ff"), 0.6)
    vane = leaf(c, 70, 56, 46, 15, -0.85)
    quill = segment(c, 36, 96, 100, 18, 2.0)
    shape = union(vane, quill)
    drop_shadow(c, shape)
    outline(c, shape, 2.5)
    c.over(ramp(depth(vane, 14.0), [(0.0, hex_color("7fb4e0")), (1.0, hex_color("f4faff"))]), coverage(vane))
    for index in range(9):
        t = -36 + index * 9
        x, y = 70 + math.cos(-0.85) * t, 56 + math.sin(-0.85) * t
        c.fill(segment(c, x, y, x - 14 * math.sin(0.85) - 6, y - 14 * math.cos(0.85) + 6, 0.7), hex_color("7a9cbc"), 0.6)
    paint(c, quill, hex_color("efe6d0"), 0.6, 2.0, 0.05, 130)


STONE_DARK = hex_color("3d3426")
STONE_LIGHT = hex_color("7a6c55")
RS_BROWN = hex_color("3e3529")
PARCHMENT_LIGHT = hex_color("ddcca4")
PARCHMENT_DARK = hex_color("a48c5c")
INK = hex_color("3a2c1a")


def stone(c, seed, block=32):
    mottling = noise(c, 9, seed, 3)
    base = mix(STONE_DARK, STONE_LIGHT, mottling * 0.8 + 0.1)
    rng = np.random.default_rng(seed)
    table = rng.uniform(0.82, 1.12, 64).astype(np.float32)
    horizontal = np.minimum(c.y, c.height - c.y) < np.minimum(c.x, c.width - c.x)
    along = np.where(horizontal, c.x, c.y)
    across = np.where(horizontal, np.minimum(c.y, c.height - c.y), np.minimum(c.x, c.width - c.x))
    block_index = (np.floor(along / block).astype(int) + horizontal.astype(int) * 17 + (c.y > c.height * 0.5).astype(int) * 7 + (c.x > c.width * 0.5).astype(int) * 3) % 64
    base = base * table[block_index][..., None]
    seam = np.minimum(along % block, block - along % block)
    mitre = np.abs(np.minimum(c.y, c.height - c.y) - np.minimum(c.x, c.width - c.x))
    return np.clip(base, 0, 1), np.minimum(seam, mitre * 0.7), across


def panel(directory):
    c = Canvas(256, 256)
    outer = box(c, 128, 128, 127, 127, 6)
    inner = box(c, 128, 128, 104, 104, 2)
    c.fill(outer, OUTLINE)
    band = subtract(outer + 2.0, inner + 1.0)
    base, seam, across = stone(c, 140)
    light = bevel(outer + 2.0, 10.0) * 0.45 - np.exp(-np.maximum(-(inner + 1.0), 0) / 3.0) * 0.25
    stones = np.clip(base * (1.0 + light[..., None]), 0, 1)
    stones = stones * np.where(seam < 1.2, 0.45, np.where(seam < 2.4, 1.18, 1.0))[..., None]
    c.over(np.clip(stones, 0, 1), coverage(band))
    for x, y in ((14, 14), (242, 14), (14, 242), (242, 242)):
        rivet = circle(c, x, y, 5.5)
        c.fill(rivet + 1.0, OUTLINE)
        paint(c, rivet, hex_color("8f8676"), 0.9, 3.0, 0.05, 141)
    c.fill(subtract(inner + 1.0, inner - 1.0), OUTLINE)
    grain = noise(c, 3, 142, 2) * 0.6 + noise(c, 24, 143, 2) * 0.4
    shadow = np.exp(-np.maximum(-inner, 0) / 7.0) * 0.45
    interior = np.asarray(RS_BROWN[:3]) * (0.86 + 0.28 * grain[..., None]) * (1.0 - shadow[..., None])
    c.over(np.clip(interior, 0, 1), coverage(inner - 1.0))
    c.save(directory / "Panel.png")


def parchment(directory):
    c = Canvas(256, 256)
    sheet = box(c, 128, 128, 126, 126, 5)
    edge = np.clip(-sheet / 40.0, 0, 1)
    fibres = noise(c, 2, 150, 2) * 0.5 + noise(c, 14, 151, 3) * 0.5
    base = mix(PARCHMENT_DARK, PARCHMENT_LIGHT, np.clip(edge ** 0.6 * (0.8 + 0.3 * fibres), 0, 1))
    c.over(base, coverage(sheet))
    c.fill(subtract(sheet, sheet + 2.5), INK)
    c.fill(subtract(sheet + 6.0, sheet + 7.0), INK, 0.35)
    c.save(directory / "Parchment.png")


def slot(directory):
    c = Canvas(64, 64)
    shape = box(c, 32, 32, 31, 31, 3)
    c.fill(shape, OUTLINE)
    inner = shape + 2.0
    shading = -bevel(inner, 6.0) * 0.6 + (noise(c, 4, 160, 2) - 0.5) * 0.15
    c.over(np.clip(np.asarray(hex_color("2a231b")[:3]) * (1.0 + shading[..., None]), 0, 1), coverage(inner))
    c.save(directory / "Slot.png")


def button(directory):
    c = Canvas(64, 64)
    shape = box(c, 32, 32, 31, 31, 5)
    c.fill(shape, OUTLINE)
    face = shape + 2.0
    mottling = noise(c, 8, 170, 3)
    base = mix(hex_color("4a4034"), hex_color("74695a"), mottling)
    light = bevel(face, 5.0) * 0.7
    c.over(np.clip(base * (1.0 + light[..., None]), 0, 1), coverage(face))
    c.fill(subtract(face + 4.0, face + 5.0), OUTLINE, 0.3)
    c.save(directory / "Button.png")


def orb_frame(directory):
    c = Canvas(128, 128)
    plate = box(c, 34, 64, 30, 15, 4)
    c.fill(plate, OUTLINE)
    paint(c, plate + 2.0, hex_color("6e6352"), 0.6, 3.0, 0.15, 180)
    c.fill(box(c, 34, 64, 25, 10, 2), hex_color("15120e"))
    rim = ring(c, 88, 64, 31, 9)
    c.fill(rim + 1.5, OUTLINE)
    base = mix(STONE_DARK, STONE_LIGHT, noise(c, 6, 181, 3))
    c.over(np.clip(base * (1.0 + bevel(rim, 6.0)[..., None] * 0.6), 0, 1), coverage(rim))
    c.fill(ring(c, 88, 64, 26.5, 1.2), OUTLINE, 0.8)
    c.save(directory / "OrbFrame.png")


def orb_fill(directory):
    c = Canvas(64, 64)
    sphere = circle(c, 32, 32, 31.5)
    t = np.clip(np.hypot(c.x - 26, c.y - 22) / 44.0, 0, 1)
    c.over(np.clip(np.stack([1.0 - 0.55 * t] * 3, axis=-1), 0, 1), coverage(sphere))
    c.fill(ellipse(c, 24, 18, 10, 5, -0.5), (1, 1, 1), 0.55)
    c.save(directory / "OrbFill.png")


def splat_shape(c, seed, radius):
    rng = np.random.default_rng(seed)
    parts = [circle(c, 32, 32, radius)]
    for index in range(7):
        a = index * math.tau / 7 + rng.uniform(-0.3, 0.3)
        d = radius * rng.uniform(0.55, 0.8)
        parts.append(circle(c, 32 + math.cos(a) * d, 32 + math.sin(a) * d, radius * rng.uniform(0.38, 0.55)))
    return union(*parts) + (noise(c, 5, seed, 2) - 0.5) * 2.5


def hitsplat(c, seed, radius):
    blob = splat_shape(c, seed, radius)
    c.fill(blob, hex_color("2a0000"))
    inner = blob + 2.0
    c.over(ramp(depth(inner, radius * 0.8), [(0.0, hex_color("6e0606")), (0.6, hex_color("b01212")), (1.0, hex_color("d42a1e"))]), coverage(inner))
    c.over((1, 1, 1), np.clip(bevel(inner, 5.0), 0, 1) * coverage(inner) * 0.3)


def hitsplats(directory):
    c = Canvas(64, 64)
    hitsplat(c, 190, 19)
    c.save(directory / "Hitsplat.png")

    c = Canvas(64, 64)
    burst = star(c, 32, 32, 31, 21, 12, 0.0)
    c.fill(burst, OUTLINE)
    c.over(ramp(depth(burst + 2.0, 10.0), [(0.0, hex_color("a0600a")), (1.0, hex_color("ffd24a"))]), coverage(burst + 2.0))
    hitsplat(c, 191, 17)
    c.save(directory / "HitsplatMax.png")


def hint_arrow(directory):
    c = Canvas(64, 64)
    arrow = polygon(c, [(6, 23), (34, 23), (34, 8), (60, 32), (34, 56), (34, 41), (6, 41)])
    c.fill(arrow - 1.0, OUTLINE)
    paint(c, arrow + 2.0, hex_color("ffd800"), 0.7, 5.0, 0.05, 200)
    c.save(directory / "HintArrow.png")


def runescape(directory):
    panel(directory)
    parchment(directory)
    slot(directory)
    button(directory)
    orb_frame(directory)
    orb_fill(directory)
    hitsplats(directory)
    hint_arrow(directory)


def icons(directory):
    directory.mkdir(exist_ok=True)

    for name, draw in (
        ("ArcaneBolt", icon_arcane_bolt),
        ("OrbitingBlades", icon_orbiting_blades),
        ("Nova", icon_nova),
        ("Fireball", icon_fireball),
        ("ChainLightning", icon_chain_lightning),
        ("MeteorStorm", icon_meteor),
        ("SwiftBoots", icon_swift_boots),
        ("Vitality", icon_vitality),
        ("Magnet", icon_magnet),
        ("Regeneration", icon_regeneration),
        ("Power", icon_power),
        ("Haste", icon_haste),
        ("Area", icon_area),
        ("Feast", icon_feast),
        ("Dash", icon_dash),
    ):
        c = Canvas(128, 128)
        draw(c)
        c.save(directory / f"{name}.png")


def main():
    directory = Path(__file__).resolve().parent / "UI"
    directory.mkdir(exist_ok=True)

    heart(directory)
    gem(directory)
    runescape(directory)
    icons(directory / "Icons")


main()
