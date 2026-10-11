import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "UI"))

import numpy as np
from ui_art import Canvas, box, rotated_box, circle, ring, segment, polygon, star, union, subtract, noise, bevel, shade, mix, hex_color, coverage


INK = hex_color("141b33")
NAVY = hex_color("1d2a55")
NAVY_DARK = hex_color("0f1630")
WHITE = hex_color("f6f3ea")
YELLOW = hex_color("ffcf2e")
GOLD = hex_color("ffc23a")
PINK = hex_color("ff4f8b")


def halftone(c, spacing, angle, strength):
    s, co = math.sin(angle), math.cos(angle)
    u = (c.x * co - c.y * s) / spacing
    v = (c.x * s + c.y * co) / spacing
    fu, fv = u - np.floor(u) - 0.5, v - np.floor(v) - 0.5
    radius = 0.45 * np.sqrt(np.clip(strength, 0, 1))
    return np.hypot(fu, fv) * spacing - radius * spacing


def sticker(directory):
    c = Canvas(128, 128)
    c.fill(box(c, 68, 70, 58, 58, 22), INK, 0.55, aa=3.0)
    c.fill(box(c, 64, 64, 60, 60, 24), WHITE)
    inner = box(c, 64, 64, 50, 50, 16)
    c.over(np.clip(mix(NAVY, NAVY_DARK, np.clip(c.y / 128.0, 0, 1)), 0, 1), coverage(inner))
    c.fill(subtract(box(c, 64, 64, 50, 50, 16), box(c, 64, 64, 47, 47, 13)), INK, 0.8)
    c.save(directory / "Sticker.png")


def halftone_tile(directory):
    c = Canvas(64, 64)
    dots = halftone(c, 16.0, 0.0, np.full(c.x.shape, 0.55, dtype=np.float32))
    c.fill(dots, (1, 1, 1), aa=1.2)
    c.save(directory / "Halftone.png")


def splat(directory):
    c = Canvas(256, 256)
    rng = np.random.default_rng(5)
    shape = circle(c, 128, 120, 70) + (noise(c, 22, 9, 3) - 0.5) * 34
    for index in range(14):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(70, 108)
        shape = np.minimum(shape, circle(c, 128 + math.cos(a) * r, 120 + math.sin(a) * r * 0.8, rng.uniform(4, 13)))
    for index in range(6):
        x = 70 + index * 23 + rng.uniform(-6, 6)
        length = rng.uniform(30, 75)
        shape = np.minimum(shape, segment(c, x, 150, x, 150 + length, rng.uniform(3, 6)))
        shape = np.minimum(shape, circle(c, x, 150 + length, rng.uniform(5, 8)))
    c.fill(shape, (1, 1, 1), aa=1.5)
    grain = noise(c, 3, 4, 1)
    c.rgba[..., 3] *= np.clip(0.85 + grain * 0.3, 0, 1)
    c.save(directory / "Splat.png")


def tape(directory):
    c = Canvas(256, 256)
    stripes = np.sin((c.x + c.y) * math.pi / 32.0) > 0
    color = np.where(stripes[..., None], np.array(YELLOW[:3], dtype=np.float32), np.array(INK[:3], dtype=np.float32))
    grime = noise(c, 14, 3, 3)
    color = np.clip(color * (0.8 + 0.35 * grime[..., None]), 0, 1)
    edge = (noise(c, 6, 8, 2) - 0.5) * 10
    band = np.maximum(np.abs(c.y - 128) - 120 - edge * 0.5, -1e3)
    c.over(color, coverage(band) * 0.97)
    c.save(directory / "Tape.png")


def coin(directory):
    c = Canvas(64, 64)
    disc = circle(c, 32, 32, 28)
    c.fill(disc - 2.5, INK)
    c.over(shade(GOLD, bevel(disc, 5.0) * 0.5), coverage(disc))
    c.fill(ring(c, 32, 32, 21, 2.0), hex_color("c98a12"))
    shape = star(c, 32, 33, 14, 6)
    c.over(shade(hex_color("ffe27a"), bevel(shape, 3.0) * 0.6), coverage(shape))
    c.fill(rotated_box(c, 22, 18, 6, 2.5, -0.7, 2.0), (1, 1, 1), 0.8)
    c.save(directory / "Coin.png")


def keycap(directory):
    c = Canvas(128, 128)
    c.fill(box(c, 64, 64, 60, 60, 20), INK)
    c.fill(box(c, 64, 70, 54, 52, 16), hex_color("b9b4a8"))
    top = box(c, 64, 58, 50, 46, 14)
    c.over(np.clip(mix(WHITE, hex_color("dcd7cc"), np.clip(c.y / 128.0, 0, 1)), 0, 1), coverage(top))
    c.save(directory / "Keycap.png")


def ticket(directory):
    c = Canvas(256, 256)
    paper = box(c, 128, 128, 120, 120, 6)
    for index in range(9):
        y = 24 + index * 26
        paper = subtract(paper, circle(c, 8, y, 7))
        paper = subtract(paper, circle(c, 248, y, 7))
    c.fill(box(c, 132, 134, 118, 118, 6) - 2, INK, 0.5, aa=4.0)
    fibers = noise(c, 4, 12, 3)
    base = mix(hex_color("fbf4e2"), hex_color("ece0c4"), np.clip(c.y / 256.0, 0, 1))
    c.over(np.clip(base * (0.92 + 0.12 * fibers[..., None]), 0, 1), coverage(paper))
    for y in (28, 228):
        for x in range(30, 228, 12):
            c.fill(box(c, x, y, 4, 1.2), PINK, 0.8)
    c.fill(subtract(box(c, 128, 128, 104, 104, 4), box(c, 128, 128, 102, 102, 3)), hex_color("c9b48a"), 0.8)
    c.save(directory / "Ticket.png")


def main():
    directory = Path(__file__).resolve().parent / "UI"
    directory.mkdir(exist_ok=True)

    sticker(directory)
    halftone_tile(directory)
    splat(directory)
    tape(directory)
    coin(directory)
    keycap(directory)
    ticket(directory)


main()
