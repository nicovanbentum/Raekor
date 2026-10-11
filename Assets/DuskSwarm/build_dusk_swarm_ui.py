import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "UI"))

import numpy as np
from ui_art import Canvas, box, rotated_box, circle, ring, segment, polygon, union, subtract, noise, bevel, shade, mix, hex_color, coverage


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


def frame(directory):
    c = Canvas(128, 128)
    outer = box(c, 64, 64, 60, 60, 14)
    c.fill(box(c, 64, 67, 60, 60, 16) - 2, (0, 0, 0), 0.5, aa=6.0)

    leather = noise(c, 24, 3, 4)
    base = mix(LEATHER_TOP, LEATHER_BOTTOM, np.clip(c.y / 128.0, 0, 1))
    c.over(np.clip(base * (0.8 + 0.4 * leather[..., None]), 0, 1), coverage(outer) * 0.95)

    c.fill(subtract(outer, box(c, 64, 64, 54, 54, 10)), IRON)
    gold_fill(c, subtract(box(c, 64, 64, 56, 56, 12), box(c, 64, 64, 51, 51, 9)), 3.0)
    c.fill(subtract(box(c, 64, 64, 43, 43, 4), box(c, 64, 64, 42, 42, 3)), GOLD, 0.6)

    for cx in (20, 108):
        for cy in (20, 108):
            stud = rotated_box(c, cx, cy, 9, 9, math.pi * 0.25, 1.5)
            c.fill(stud + 1.5, IRON)
            gold_fill(c, stud, 4.0)
            c.fill(circle(c, cx, cy, 3.2), RUBY)
            c.fill(circle(c, cx - 1.0, cy - 1.2, 1.1), (1, 0.8, 0.8), 0.8)

    for cx, cy, angle in ((64, 8, 0), (64, 120, 0), (8, 64, math.pi * 0.5), (120, 64, math.pi * 0.5)):
        gem = rotated_box(c, cx, cy, 5, 5, math.pi * 0.25, 1.0)
        gold_fill(c, gem, 3.0)

    c.save(directory / "Frame.png")


def bar_frame(directory):
    c = Canvas(128, 128)
    outer = box(c, 64, 64, 62, 62, 62)
    inner = box(c, 64, 64, 44, 44, 44)
    c.fill(subtract(outer, inner), IRON)
    gold_fill(c, subtract(box(c, 64, 64, 54, 54, 54), box(c, 64, 64, 48, 48, 48)), 3.0)
    c.fill(subtract(box(c, 64, 64, 46, 46, 46), inner), (0, 0, 0), 0.6)
    c.save(directory / "BarFrame.png")


def bar_sheen(directory):
    c = Canvas(128, 128)
    t = c.y / 128.0
    highlight = np.exp(-((t - 0.28) / 0.12) ** 2) * 0.45
    shadow = np.clip((t - 0.55) / 0.45, 0, 1) * 0.45
    grain = noise(c, 10, 11, 3)
    streak = (np.sin((c.x + c.y * 0.6) * 0.18) * 0.5 + 0.5) ** 8 * 0.12
    c.over((1, 1, 1), highlight + streak)
    c.over((0, 0, 0), shadow + (grain - 0.5) * 0.25)
    c.save(directory / "BarSheen.png")


def rune_ring(directory):
    c = Canvas(128, 128)
    c.fill(circle(c, 64, 66, 60) - 2, (0, 0, 0), 0.5, aa=6.0)
    disc = circle(c, 64, 64, 58)
    c.over(np.clip(mix(LEATHER_TOP, LEATHER_BOTTOM, np.clip(c.y / 128.0, 0, 1)) * 1.2, 0, 1), coverage(disc))
    c.fill(ring(c, 64, 64, 56, 6), IRON)
    gold_fill(c, ring(c, 64, 64, 55, 3.0), 2.0)
    gold_fill(c, ring(c, 64, 64, 41, 2.0), 1.5)

    for index in range(24):
        a = index / 24 * math.tau
        r0, r1 = (44, 52) if index % 3 == 0 else (46, 50)
        ax, ay = 64 + math.cos(a) * r0, 64 + math.sin(a) * r0
        bx, by = 64 + math.cos(a) * r1, 64 + math.sin(a) * r1
        c.fill(segment(c, ax, ay, bx, by, 1.2 if index % 3 == 0 else 0.7), GOLD, 0.9)

    for index in range(8):
        a = index / 8 * math.tau + math.pi / 8
        px, py = 64 + math.cos(a) * 48, 64 + math.sin(a) * 48
        c.fill(rotated_box(c, px, py, 2.2, 2.2, a + math.pi * 0.25), hex_color("9fd8ff"), 0.9)

    c.save(directory / "RuneRing.png")


def heart(directory):
    c = Canvas(64, 64)
    shape = union(circle(c, 23, 25, 13), circle(c, 41, 25, 13), polygon(c, [(11.5, 30), (52.5, 30), (32, 54)]))
    c.fill(shape - 3, (0.08, 0.0, 0.02))
    light = bevel(shape, 7.0)
    c.over(shade(RUBY, light * 0.6), coverage(shape))
    c.fill(rotated_box(c, 22, 21, 5, 3, -0.6, 2.5), (1, 0.85, 0.85), 0.75)
    c.save(directory / "Heart.png")


def skull(directory):
    c = Canvas(64, 64)
    cranium = union(circle(c, 32, 27, 19), box(c, 32, 42, 11, 9, 4))
    c.fill(cranium - 3, (0.06, 0.05, 0.07))
    light = bevel(cranium, 6.0)
    c.over(shade(BONE, light * 0.4 + (noise(c, 5, 2, 2) - 0.5) * 0.15), coverage(cranium))
    for ex in (24, 40):
        c.fill(circle(c, ex, 30, 6), (0.08, 0.05, 0.09))
        c.fill(circle(c, ex, 31, 2.2), hex_color("ff5a3a"), 0.9)
    c.fill(polygon(c, [(32, 36), (29, 42), (35, 42)]), (0.08, 0.05, 0.09))
    for tx in (26, 30, 34, 38):
        c.fill(box(c, tx, 48, 0.6, 3.5), (0.12, 0.09, 0.1), 0.9)
    c.save(directory / "Skull.png")


def banner(directory):
    c = Canvas(256, 256)
    body = polygon(c, [(6, 64), (250, 64), (222, 128), (250, 192), (6, 192), (34, 128)])
    c.fill(body - 4, (0, 0, 0), 0.45, aa=6.0)
    cloth = noise(c, 18, 21, 3)
    base = mix(hex_color("7e1420"), hex_color("3c0710"), np.clip((c.y - 64) / 128.0, 0, 1))
    c.over(np.clip(base * (0.85 + 0.3 * cloth[..., None]), 0, 1), coverage(body))
    trim = subtract(body, polygon(c, [(20, 74), (236, 74), (210, 128), (236, 182), (20, 182), (46, 128)]))
    gold_fill(c, trim, 3.0)
    c.fill(subtract(polygon(c, [(26, 80), (230, 80), (205, 128), (230, 176), (26, 176), (51, 128)]), polygon(c, [(28, 82), (228, 82), (203, 128), (228, 174), (28, 174), (53, 128)])), GOLD, 0.5)
    c.save(directory / "Banner.png")


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


def main():
    directory = Path(__file__).resolve().parent / "UI"
    directory.mkdir(exist_ok=True)

    frame(directory)
    bar_frame(directory)
    bar_sheen(directory)
    rune_ring(directory)
    heart(directory)
    skull(directory)
    banner(directory)
    gem(directory)


main()
