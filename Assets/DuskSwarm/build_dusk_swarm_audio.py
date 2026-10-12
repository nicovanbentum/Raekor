import math
from pathlib import Path

import aud
import numpy as np


SR = 44100
OUT = Path(__file__).resolve().parent / "Audio"
RNG = np.random.default_rng(11)


def samples(seconds):
    return int(round(seconds * SR))


def time_axis(n):
    return np.arange(n) / SR


def decay(n, seconds):
    return np.exp(-time_axis(n) / seconds)


def attack_release(n, attack, release):
    t = time_axis(n)
    duration = n / SR
    env = np.ones(n)
    if attack > 0:
        env *= np.clip(t / attack, 0, 1)
    if release > 0:
        env *= np.clip((duration - t) / release, 0, 1)
    return env


def noise(n):
    return RNG.uniform(-1.0, 1.0, n)


def spectral_filter(x, low=None, high=None, order=2, tilt=0.0, circular=False):
    n = len(x)
    if not circular:
        x = np.concatenate([x, np.zeros(n)])
    spectrum = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    f = np.maximum(freqs, 1.0)
    gain = np.ones_like(f)
    if low:
        gain /= np.sqrt(1.0 + (low / f) ** (2 * order))
    if high:
        gain /= np.sqrt(1.0 + (f / high) ** (2 * order))
    if tilt:
        gain *= (f / 1000.0) ** tilt
    gain[0] = 0.0
    y = np.fft.irfft(spectrum * gain, len(x))
    return y[:n]


def band_noise(n, low, high, order=2, circular=False):
    return spectral_filter(noise(n), low, high, order, circular=circular)


def formant(x, centers, width=1.3, circular=False):
    out = np.zeros(len(x))
    for center, gain in centers:
        out += spectral_filter(x, center / width, center * width, order=2, circular=circular) * gain
    return out


def svf_bandpass(x, cutoff, q):
    cutoff = np.broadcast_to(cutoff, x.shape)
    y = np.zeros_like(x)
    ic1, ic2 = 0.0, 0.0
    k = 1.0 / q
    for i in range(len(x)):
        g = math.tan(math.pi * min(cutoff[i], SR * 0.45) / SR)
        a1 = 1.0 / (1.0 + g * (g + k))
        v3 = x[i] - ic2
        v1 = a1 * ic1 + g * a1 * v3
        v2 = ic2 + g * v1
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        y[i] = v1
    return y


def tone(freq, n, phase=0.0):
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), (n,))
    return np.sin(phase + 2.0 * np.pi * np.cumsum(freq) / SR)


def additive(freq, n, harmonics, rolloff=1.0, odd_only=False, max_freq=12000.0, damping=0.0):
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), (n,))
    phase = 2.0 * np.pi * np.cumsum(freq) / SR
    t = time_axis(n)
    out = np.zeros(n)
    top = float(np.max(freq))
    for h in range(1, harmonics + 1):
        if odd_only and h % 2 == 0:
            continue
        if h * top > max_freq:
            break
        partial = np.sin(phase * h) / h ** rolloff
        if damping:
            partial *= np.exp(-t * damping * h)
        out += partial
    return out


def pluck(freq, n, brightness=1.5, sustain=3.0):
    return additive(freq, n, 14, rolloff=1.3, max_freq=9000.0, damping=brightness) * decay(n, sustain / max(float(np.mean(freq)) / 220.0, 1.0) ** 0.5)


def bell(freq, n, seconds):
    out = np.zeros(n)
    for ratio, amp, length in ((1.0, 1.0, 1.0), (2.76, 0.45, 0.55), (5.4, 0.25, 0.3), (8.93, 0.12, 0.18)):
        out += tone(freq * ratio, n, RNG.uniform(0, 6.28)) * decay(n, seconds * length) * amp
    return out


def note(name):
    names = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
    pitch, octave = name[:-1], int(name[-1])
    midi = names[pitch] + (octave + 1) * 12
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def place(buffer, sound, start, circular=False):
    start = int(start)
    if circular:
        index = (np.arange(len(sound)) + start) % len(buffer)
        np.add.at(buffer, index, sound)
    else:
        end = min(len(buffer), start + len(sound))
        if end > start:
            buffer[start:end] += sound[: end - start]


def normalize(x, peak):
    return x * (peak / max(np.max(np.abs(x)), 1e-9))


def fade_edges(x, fade_in=0.002, fade_out=0.01):
    n = len(x)
    env = np.ones(n)
    a, r = samples(fade_in), samples(fade_out)
    if a:
        env[:a] = np.linspace(0, 1, a)
    if r:
        env[-r:] *= np.linspace(1, 0, r)
    return x * env


def write(name, data, bitrate=112000):
    OUT.mkdir(parents=True, exist_ok=True)
    data = np.asarray(data, dtype=np.float32)
    if data.ndim == 1:
        data = data.reshape(-1, 1)
        channels = aud.CHANNELS_MONO
    else:
        channels = aud.CHANNELS_STEREO
    data = np.ascontiguousarray(np.clip(data, -1.0, 1.0))
    path = OUT / f"{name}.ogg"
    aud.Sound.buffer(data, SR).write(str(path), rate=SR, channels=channels, format=aud.FORMAT_FLOAT32,
                                     container=aud.CONTAINER_OGG, codec=aud.CODEC_VORBIS, bitrate=bitrate)
    print(f"wrote {path.name} {len(data) / SR:.2f}s")


def boom(n, start_freq, end_freq, drop, seconds):
    t = time_axis(n)
    return tone((start_freq - end_freq) * np.exp(-t * drop) + end_freq, n) * decay(n, seconds)


def crackle(n, count, start, end, low=1500.0, high=8000.0, falloff=2.0):
    out = np.zeros(n)
    for _ in range(count):
        offset = samples(RNG.uniform(start, end) ** 1.4)
        length = samples(RNG.uniform(0.004, 0.02))
        grain = band_noise(length, RNG.uniform(low, low * 2), RNG.uniform(high * 0.6, high)) * decay(length, 0.004)
        place(out, grain * RNG.uniform(0.3, 1.0) * math.exp(-offset / SR * falloff), offset)
    return out


def whoosh(n, start_cutoff, end_cutoff, q=1.2, curve=1.0):
    t = time_axis(n) / (n / SR)
    cutoff = start_cutoff + (end_cutoff - start_cutoff) * t ** curve
    return svf_bandpass(noise(n), cutoff, q)


def growl(freq, n, vowels, roughness=0.4):
    t = time_axis(n)
    rough = 1.0 + roughness * spectral_filter(noise(n), 20.0, 90.0) / 0.05
    body = additive(freq, n, 40, rolloff=1.0, max_freq=5000.0) * np.clip(rough, 0.0, 2.0)
    return formant(body, vowels) + band_noise(n, 300.0, 2500.0) * 0.15 * np.abs(rough)


def arcane_bolt():
    n = samples(0.28)
    t = time_axis(n)
    freq = 1500.0 * np.exp(-t * 9.0) + 520.0
    body = tone(freq + 180.0 * tone(freq * 1.5, n), n) * decay(n, 0.07)
    shimmer = band_noise(n, 3000.0, 9000.0) * decay(n, 0.03) * 0.4
    return fade_edges(normalize(body + shimmer, 0.6))


def bolt_hit():
    n = samples(0.16)
    t = time_axis(n)
    pop = band_noise(n, 1800.0, 7000.0) * decay(n, 0.012)
    ping = tone(1100.0 * np.exp(-t * 12.0) + 500.0, n) * decay(n, 0.035)
    return fade_edges(normalize(normalize(pop, 1.0) * 0.6 + ping * 0.7, 0.55))


def blade_hit():
    n = samples(0.22)
    t = time_axis(n)
    slice_ = svf_bandpass(noise(n), 6500.0 * np.exp(-t * 14.0) + 1800.0, 2.0) * decay(n, 0.05)
    ring = sum(tone(f, n, RNG.uniform(0, 6.28)) * decay(n, 0.06 / (i + 1)) / (i + 1) for i, f in enumerate((2350.0, 3720.0, 5110.0)))
    thwack = band_noise(n, 150.0, 900.0) * decay(n, 0.02)
    return fade_edges(normalize(normalize(slice_, 1.0) * 0.7 + ring * 0.35 + normalize(thwack, 0.5), 0.6))


def enemy_hit():
    n = samples(0.14)
    t = time_axis(n)
    thud = tone(170.0 * np.exp(-t * 18.0) + 70.0, n) * decay(n, 0.03)
    smack = band_noise(n, 250.0, 2200.0) * decay(n, 0.015)
    return fade_edges(normalize(thud + normalize(smack, 0.8), 0.6))


def nova():
    n = samples(1.0)
    t = time_axis(n)
    sub = boom(n, 140.0, 38.0, 6.0, 0.35)
    sweep = whoosh(n, 4200.0, 250.0, 1.0, 0.5) * attack_release(n, 0.005, 0.6)
    shimmer = np.zeros(n)
    for freq in (note("D6"), note("F6"), note("A6"), note("C#7")):
        shimmer += tone(freq * (1.0 + 0.04 * np.exp(-t * 5.0)), n) * decay(n, 0.25)
    out = normalize(sub, 1.0) + normalize(sweep, 0.8) + normalize(shimmer, 0.25)
    return fade_edges(normalize(np.tanh(out * 1.3), 0.8), fade_out=0.15)


def fireball_cast():
    n = samples(0.45)
    t = time_axis(n)
    roar = whoosh(n, 350.0, 1800.0, 0.9, 0.7) * attack_release(n, 0.06, 0.25)
    flicker = 1.0 + 0.4 * np.sin(2 * np.pi * 23.0 * t)
    pops = crackle(n, 10, 0.02, 0.4, 2000.0, 7000.0, 1.0)
    return fade_edges(normalize(normalize(roar * flicker, 1.0) + normalize(pops, 0.35), 0.6))


def explosion(length=1.5, low=55.0, size=1.0):
    n = samples(length)
    sub = boom(n, low * 2.2, low * 0.55, 5.0 / size, 0.35 * size)
    burst = spectral_filter(noise(n), 50.0, 2800.0) * decay(n, 0.22 * size)
    rumble = spectral_filter(noise(n), 25.0, 220.0, order=3) * decay(n, 0.6 * size)
    debris = crackle(n, int(40 * size), 0.04, length * 0.7, 1200.0, 7000.0, 2.5 / size)
    out = normalize(sub, 1.0) + 0.7 * normalize(burst, 1.0) + 0.6 * normalize(rumble, 1.0) + normalize(debris, 0.35)
    return fade_edges(normalize(np.tanh(out * 1.5), 0.9), fade_out=0.25)


def lightning():
    n = samples(0.75)
    t = time_axis(n)
    snap = band_noise(n, 2500.0, 12000.0) * decay(n, 0.008)
    buzz_freq = 95.0 + 30.0 * spectral_filter(noise(n), None, 30.0) / 0.02
    buzz = additive(np.clip(buzz_freq, 40.0, 200.0), n, 30, rolloff=0.8, max_freq=7000.0)
    buzz = spectral_filter(buzz, 400.0, 6000.0) * decay(n, 0.12)
    sizzle = np.zeros(n)
    for _ in range(70):
        start = samples(RNG.uniform(0.0, 0.45) ** 1.3)
        length = samples(RNG.uniform(0.002, 0.008))
        place(sizzle, noise(length) * RNG.uniform(0.3, 1.0) * math.exp(-start / SR * 6.0), start)
    sizzle = spectral_filter(sizzle, 1500.0, 11000.0)
    thunder = spectral_filter(noise(n), 30.0, 300.0) * decay(n, 0.25) * attack_release(n, 0.02, 0.2)
    out = normalize(snap, 1.0) + normalize(buzz, 0.6) + normalize(sizzle, 0.6) + normalize(thunder, 0.5)
    return fade_edges(normalize(np.tanh(out * 1.2), 0.75), fade_out=0.1)


def bubbles(n, count, start, end, low, high):
    out = np.zeros(n)
    for _ in range(count):
        offset = samples(RNG.uniform(start, end))
        length = samples(RNG.uniform(0.02, 0.06))
        base = RNG.uniform(low, high)
        freq = base * (1.0 + 1.5 * time_axis(length) / (length / SR))
        place(out, tone(freq, length) * np.sin(np.linspace(0, np.pi, length)) * RNG.uniform(0.3, 1.0), offset)
    return out


def poison_burst():
    n = samples(0.8)
    t = time_axis(n)
    splat = boom(n, 220.0, 60.0, 14.0, 0.12)
    squelch = svf_bandpass(noise(n), 1400.0 * np.exp(-t * 6.0) + 250.0, 3.0) * decay(n, 0.15)
    pops = bubbles(n, 26, 0.03, 0.55, 300.0, 900.0)
    out = normalize(splat, 1.0) + normalize(squelch, 0.8) + normalize(pops, 0.4)
    return fade_edges(normalize(np.tanh(out * 1.2), 0.8), fade_out=0.1)


def fuse():
    n = samples(0.9)
    t = time_axis(n)
    hiss = band_noise(n, 2500.0, 9000.0) * np.clip(t / 0.9, 0, 1) ** 1.5
    gurgle = bubbles(n, 22, 0.0, 0.85, 200.0, 600.0) * (0.5 + t / 0.9)
    swell = tone(90.0 + 60.0 * t / 0.9, n) * np.clip(t / 0.9, 0, 1) * 0.4
    return fade_edges(normalize(normalize(hiss, 0.5) + normalize(gurgle, 0.8) + swell, 0.6), fade_out=0.02)


def holy_blast():
    n = samples(2.6)
    t = time_axis(n)
    blast = explosion(2.6, 50.0, 1.4)
    chord = np.zeros(n)
    for name in ("D5", "F#5", "A5", "D6", "A6"):
        chord += bell(note(name), n, 1.2)
    choir = np.zeros(n)
    for name in ("D4", "A4", "F#5"):
        vibrato = 1.0 + 0.006 * np.sin(2 * np.pi * 5.0 * t)
        choir += additive(note(name) * vibrato, n, 16, rolloff=1.4, max_freq=6000.0)
    choir = formant(choir, ((650.0, 1.0), (1100.0, 0.6), (2600.0, 0.25))) * attack_release(n, 0.15, 1.6)
    out = blast * 0.8 + normalize(chord, 0.5) + normalize(choir, 0.4)
    return fade_edges(normalize(out, 0.9), fade_out=0.4)


def slam():
    n = samples(0.9)
    thump = boom(n, 110.0, 32.0, 9.0, 0.22)
    dirt = spectral_filter(noise(n), 60.0, 1200.0) * decay(n, 0.12)
    rumble = spectral_filter(noise(n), 25.0, 160.0, order=3) * decay(n, 0.35)
    pebbles = crackle(n, 18, 0.03, 0.5, 800.0, 4500.0, 3.0)
    out = normalize(thump, 1.0) + normalize(dirt, 0.6) + normalize(rumble, 0.6) + normalize(pebbles, 0.25)
    return fade_edges(normalize(np.tanh(out * 1.4), 0.85), fade_out=0.15)


def ghoul_death():
    n = samples(0.65)
    t = time_axis(n)
    groan = growl(130.0 * np.exp(-t * 1.4), n, ((550.0, 1.0), (850.0, 0.6), (2400.0, 0.15)), 0.5)
    groan *= attack_release(n, 0.03, 0.3)
    splat = band_noise(n, 200.0, 1800.0) * decay(n, 0.05)
    return fade_edges(normalize(normalize(groan, 1.0) + normalize(splat, 0.6), 0.6))


def bone_clack(n, freq):
    click = band_noise(n, 2000.0, 9000.0) * decay(n, 0.004)
    body = (tone(freq, n) + 0.6 * tone(freq * 1.83, n) + 0.3 * tone(freq * 3.1, n)) * decay(n, 0.025)
    return normalize(click, 1.0) * 0.8 + body * 0.5


def bone_shatter():
    n = samples(0.7)
    out = np.zeros(n)
    place(out, bone_clack(samples(0.1), 1300.0) * 1.2, 0)
    for _ in range(16):
        start = samples(RNG.uniform(0.02, 0.6) ** 1.3)
        place(out, bone_clack(samples(0.08), RNG.uniform(900.0, 2400.0)) * RNG.uniform(0.2, 0.7) * math.exp(-start / SR * 3.0), start)
    return fade_edges(normalize(out, 0.65), fade_out=0.05)


def bat_death():
    n = samples(0.32)
    t = time_axis(n)
    freq = 4200.0 * np.exp(-t * 4.0) + 1400.0 + 260.0 * np.sin(2 * np.pi * 55.0 * t)
    screech = additive(freq, n, 4, rolloff=1.0) * attack_release(n, 0.01, 0.2)
    flap = band_noise(n, 300.0, 1500.0) * decay(n, 0.04) * 0.5
    return fade_edges(normalize(screech + flap, 0.5))


def wraith_death():
    n = samples(1.1)
    t = time_axis(n)
    freq = 760.0 * np.exp(-t * 1.6) + 220.0
    wail = tone(freq * (1.0 + 0.02 * np.sin(2 * np.pi * 7.0 * t)), n) + 0.3 * tone(freq * 2.01, n)
    breath = spectral_filter(noise(n), 500.0, 3000.0) * 0.3
    out = (wail + breath) * attack_release(n, 0.06, 0.7)
    return fade_edges(normalize(out, 0.55), fade_out=0.1)


def brute_death():
    n = samples(1.2)
    t = time_axis(n)
    groan = growl(85.0 * np.exp(-t * 0.8), n, ((450.0, 1.0), (750.0, 0.6), (2200.0, 0.1)), 0.6) * attack_release(n, 0.05, 0.6)
    fall = np.zeros(n)
    place(fall, slam()[: samples(0.8)] * 0.8, samples(0.38))
    return fade_edges(normalize(normalize(groan, 1.0) * 0.8 + fall, 0.85), fade_out=0.1)


def brute_roar():
    n = samples(1.4)
    t = time_axis(n)
    freq = 70.0 + 25.0 * np.sin(np.pi * np.clip(t / 1.2, 0, 1))
    roar = growl(freq, n, ((480.0, 1.0), (900.0, 0.7), (2500.0, 0.2)), 0.8) * attack_release(n, 0.15, 0.6)
    return fade_edges(normalize(np.tanh(normalize(roar, 1.0) * 1.8), 0.8), fade_out=0.1)


def arrow_shot():
    n = samples(0.32)
    t = time_axis(n)
    twang = pluck(196.0 * (1.0 + 0.03 * np.exp(-t * 30.0)), n, 2.5, 0.15)
    swish = whoosh(n, 2500.0, 900.0, 1.4) * attack_release(n, 0.03, 0.2) * 0.6
    return fade_edges(normalize(normalize(twang, 1.0) + normalize(swish, 0.5), 0.55))


def lich_cast():
    n = samples(1.0)
    t = time_axis(n)
    choir = np.zeros(n)
    for name in ("D3", "D#3", "A3"):
        choir += additive(note(name) * (1.0 + 0.008 * np.sin(2 * np.pi * 6.0 * t)), n, 20, rolloff=1.3, max_freq=5000.0)
    choir = formant(choir, ((500.0, 1.0), (800.0, 0.6), (2500.0, 0.2))) * attack_release(n, 0.3, 0.4)
    surge = whoosh(n, 300.0, 3000.0, 1.0, 2.0) * attack_release(n, 0.5, 0.3)
    out = normalize(choir, 0.8) + normalize(surge, 0.6)
    return fade_edges(normalize(out, 0.7), fade_out=0.1)


def lich_rise():
    n = samples(3.2)
    t = time_axis(n)
    rumble = spectral_filter(noise(n), 20.0, 140.0, order=3) * attack_release(n, 0.8, 1.2)
    choir = np.zeros(n)
    for name in ("D2", "D3", "D#3", "A3", "D4"):
        choir += additive(note(name) * (1.0 + 0.01 * np.sin(2 * np.pi * 4.5 * t + RNG.uniform(0, 6))), n, 24, rolloff=1.2, max_freq=5000.0)
    choir = formant(choir, ((450.0, 1.0), (750.0, 0.5), (2400.0, 0.2))) * attack_release(n, 1.2, 1.2)
    roar = np.zeros(n)
    place(roar, growl(60.0 + 20.0 * np.sin(np.pi * time_axis(samples(1.6)) / 1.6), samples(1.6), ((420.0, 1.0), (800.0, 0.7), (2300.0, 0.25)), 1.0) * attack_release(samples(1.6), 0.2, 0.8), samples(1.3))
    hit = np.zeros(n)
    place(hit, boom(samples(1.5), 90.0, 30.0, 4.0, 0.5), samples(1.3))
    out = normalize(rumble, 0.7) + normalize(choir, 0.6) + normalize(roar, 0.7) + normalize(hit, 0.8)
    return fade_edges(normalize(np.tanh(out * 1.2), 0.9), fade_out=0.3)


def lich_death():
    n = samples(3.0)
    t = time_axis(n)
    freq = 420.0 * np.exp(-t * 0.9) + 60.0
    wail = growl(freq, n, ((600.0, 1.0), (1000.0, 0.6), (2600.0, 0.3)), 0.6) * attack_release(n, 0.1, 1.5)
    ghost = tone(freq * 2.0 * (1.0 + 0.02 * np.sin(2 * np.pi * 6.0 * t)), n) * attack_release(n, 0.2, 1.8) * 0.3
    rumble = spectral_filter(noise(n), 20.0, 180.0, order=3) * attack_release(n, 0.3, 1.5)
    out = normalize(wail, 0.8) + ghost + normalize(rumble, 0.6)
    return fade_edges(normalize(out, 0.85), fade_out=0.4)


def swarm():
    n = samples(2.4)
    out = np.zeros(n)
    for _ in range(26):
        start = samples(RNG.uniform(0.0, 1.8))
        length = samples(RNG.uniform(0.12, 0.3))
        t = time_axis(length)
        freq = RNG.uniform(3000.0, 5500.0) * (1.0 - 0.3 * t / t[-1]) + 300.0 * np.sin(2 * np.pi * RNG.uniform(40, 70) * t)
        place(out, tone(freq, length) * np.sin(np.linspace(0, np.pi, length)) * RNG.uniform(0.2, 0.6), start)
    t = time_axis(n)
    flaps = band_noise(n, 200.0, 1600.0) * (0.5 + 0.5 * np.sin(2 * np.pi * 14.0 * t)) ** 4
    out = normalize(out, 0.6) + normalize(flaps, 0.5) * attack_release(n, 0.4, 0.8)
    return fade_edges(normalize(out * attack_release(n, 0.2, 0.7), 0.6), fade_out=0.2)


def player_hurt():
    n = samples(0.4)
    t = time_axis(n)
    thump = boom(n, 160.0, 60.0, 12.0, 0.08)
    sting = (tone(note("D#5"), n) + tone(note("A5"), n)) * decay(n, 0.12) * 0.35
    crunch = band_noise(n, 500.0, 4000.0) * decay(n, 0.03)
    return fade_edges(normalize(thump + sting + normalize(crunch, 0.6), 0.75))


def player_death():
    n = samples(2.4)
    out = np.zeros(n)
    for index, name in enumerate(("A4", "F4", "D4", "A3")):
        start = samples(index * 0.22)
        length = n - start
        place(out, pluck(note(name), length, 0.8, 1.2) * (1.0 - index * 0.1), start)
    pad = np.zeros(n)
    for name in ("D3", "F3", "A3"):
        pad += additive(note(name), n, 16, rolloff=1.2, max_freq=4000.0)
    pad = spectral_filter(pad, None, 1500.0) * attack_release(n, 0.4, 1.2)
    hit = boom(n, 120.0, 35.0, 6.0, 0.4)
    return fade_edges(normalize(normalize(out, 0.7) + normalize(pad, 0.35) + normalize(hit, 0.6), 0.8), fade_out=0.3)


def dash():
    n = samples(0.32)
    t = time_axis(n) / 0.32
    air = whoosh(n, 700.0, 4000.0, 1.0, 0.6) * np.sin(np.pi * t) ** 1.5
    sparkle = tone(note("A6") * (1.0 + 0.5 * t), n) * decay(n, 0.08) * 0.2
    return fade_edges(normalize(normalize(air, 1.0) + sparkle, 0.55))


def gem():
    n = samples(0.3)
    out = bell(note("E6"), n, 0.12) + 0.4 * bell(note("B6"), n, 0.08)
    return fade_edges(normalize(out, 0.6))


def level_up():
    n = samples(1.4)
    out = np.zeros(n)
    for index, name in enumerate(("D5", "F#5", "A5", "D6", "F#6")):
        start = samples(index * 0.07)
        length = n - start
        place(out, pluck(note(name), length, 0.6, 1.2) * (0.7 + index * 0.08), start)
    shimmer = np.zeros(n)
    for _ in range(20):
        start = samples(RNG.uniform(0.2, 1.0))
        length = samples(0.25)
        place(shimmer, bell(RNG.uniform(3000.0, 6000.0), length, 0.05) * 0.2, start)
    return fade_edges(normalize(out + shimmer, 0.75), fade_out=0.2)


def select():
    n = samples(0.09)
    out = tone(note("A5"), n) * decay(n, 0.02) + band_noise(n, 2000.0, 7000.0) * decay(n, 0.004) * 0.6
    return fade_edges(normalize(out, 0.4))


def choose():
    n = samples(0.6)
    out = np.zeros(n)
    for start, name in ((0.0, "A5"), (0.08, "D6")):
        s = samples(start)
        place(out, bell(note(name), n - s, 0.25), s)
    return fade_edges(normalize(out, 0.6), fade_out=0.1)


def potion():
    n = samples(1.0)
    t = time_axis(n)
    glugs = bubbles(n, 14, 0.0, 0.45, 250.0, 500.0)
    rise = np.zeros(n)
    for index, name in enumerate(("D5", "A5", "D6", "F#6", "A6")):
        start = samples(0.3 + index * 0.08)
        length = n - start
        place(rise, bell(note(name), length, 0.35) * 0.5, start)
    return fade_edges(normalize(normalize(glugs, 0.6) + normalize(rise, 0.6), 0.65), fade_out=0.2)


def magnet():
    n = samples(0.9)
    t = time_axis(n)
    freq = 300.0 * 2.0 ** (2.0 * t / 0.9)
    warble = tone(freq, n) * (0.6 + 0.4 * np.sin(2 * np.pi * 18.0 * t)) + 0.4 * tone(freq * 1.5, n)
    out = warble * attack_release(n, 0.05, 0.25)
    return fade_edges(normalize(out, 0.55))


def chest_open():
    n = samples(1.1)
    t = time_axis(n)
    creak_freq = 70.0 + 50.0 * t + 25.0 * np.abs(spectral_filter(noise(n), None, 12.0)) / 0.01
    creak = additive(np.clip(creak_freq, 40.0, 220.0), n, 40, rolloff=0.7, max_freq=6000.0)
    creak = formant(creak, ((900.0, 1.0), (2100.0, 0.6))) * attack_release(n, 0.05, 0.4) * (t < 0.6)
    latch = np.zeros(n)
    place(latch, bone_clack(samples(0.1), 700.0), 0)
    shine = np.zeros(n)
    for index, name in enumerate(("A5", "D6", "F#6", "A6", "D7")):
        start = samples(0.45 + index * 0.06)
        place(shine, bell(note(name), n - start, 0.4) * 0.4, start)
    out = normalize(creak, 0.4) + normalize(latch, 0.5) + normalize(shine, 0.6)
    return fade_edges(normalize(out, 0.75), fade_out=0.2)


def fanfare(notes, beat, length, chord):
    n = samples(length)
    out = np.zeros(n)
    for step, name in notes:
        start = samples(step * beat)
        s_len = n - start
        horn = additive(note(name), s_len, 18, rolloff=1.1, max_freq=6000.0) * attack_release(s_len, 0.03, 0.0) * decay(s_len, 0.5)
        place(out, horn + pluck(note(name), s_len, 0.8, 1.0) * 0.6, start)
    pad = np.zeros(n)
    for name in chord:
        pad += additive(note(name), n, 12, rolloff=1.3, max_freq=4000.0)
    pad *= attack_release(n, 0.3, length * 0.5)
    return fade_edges(normalize(normalize(out, 0.8) + normalize(pad, 0.3), 0.8), fade_out=0.3)


def treasure():
    return fanfare(((0, "D5"), (1, "A5"), (2, "D6"), (2.5, "F#6")), 0.11, 1.5, ("D4", "F#4", "A4"))


def victory():
    return fanfare(((0, "D5"), (1, "F#5"), (2, "A5"), (3, "D6"), (5, "C6"), (6, "D6"), (7, "F#6")), 0.16, 3.4, ("D3", "A3", "D4", "F#4"))


def game_over():
    n = samples(3.0)
    out = np.zeros(n)
    for index, name in enumerate(("D5", "C5", "A#4", "A4")):
        start = samples(index * 0.38)
        length = n - start
        place(out, pluck(note(name), length, 0.5, 1.6), start)
    pad = np.zeros(n)
    for name in ("D3", "F3", "A3"):
        pad += additive(note(name), n, 14, rolloff=1.3, max_freq=3000.0)
    pad *= attack_release(n, 0.8, 1.2)
    bell_tone = np.zeros(n)
    place(bell_tone, bell(note("D4"), samples(2.0), 0.9) * 0.5, samples(1.4))
    return fade_edges(normalize(normalize(out, 0.6) + normalize(pad, 0.3) + normalize(bell_tone, 0.4), 0.75), fade_out=0.4)


def ambience():
    duration = 30.0
    n = samples(duration)
    t = time_axis(n)

    def slow(cycles, phase):
        return np.sin(2 * np.pi * cycles * t / duration + phase)

    left = np.zeros(n)
    right = np.zeros(n)

    for channel, phase in ((left, 0.0), (right, 1.7)):
        wind = spectral_filter(noise(n), 60.0, 700.0, circular=True)
        gust = 0.55 + 0.25 * slow(2, phase) + 0.15 * slow(5, phase * 2.0) + 0.05 * slow(11, phase)
        howl = spectral_filter(noise(n), 380.0, 520.0, order=4, circular=True)
        howl_amp = np.clip(slow(3, phase + 0.8), 0, 1) ** 2
        channel += normalize(wind, 0.5) * gust + normalize(howl, 0.18) * howl_amp

    for pan, pitch, period, phase in ((0.2, 4400.0, 0.75, 0.0), (0.8, 4700.0, 0.6, 0.3)):
        cycle = (t + phase) % period
        gate = ((cycle < 0.09) * (np.sin(2 * np.pi * 33.0 * cycle) > 0)).astype(np.float64)
        gate = spectral_filter(gate, None, 400.0, circular=True)
        chirp = tone(pitch, n) * gate * (0.5 + 0.5 * np.clip(slow(4, pan * 4.0), 0, 1))
        left += chirp * 0.035 * (1 - pan)
        right += chirp * 0.035 * pan

    for start, pan in ((6.0, 0.3), (19.5, 0.75)):
        for offset, length, freq in ((0.0, 0.45, 390.0), (0.6, 0.25, 370.0), (0.95, 0.7, 380.0)):
            s = samples(length)
            hoot = tone(freq * (1.0 - 0.05 * time_axis(s) / length), s) * np.sin(np.linspace(0, np.pi, s)) ** 2
            hoot = spectral_filter(hoot, None, 900.0) * 0.06
            place(left, hoot * (1 - pan), samples(start + offset), circular=True)
            place(right, hoot * pan, samples(start + offset), circular=True)

    for start, pan in ((12.2, 0.85), (25.0, 0.15)):
        for index in range(3):
            s = samples(0.28)
            caw_t = time_axis(s)
            caw = additive(560.0 * (1.0 - 0.25 * caw_t / 0.28), s, 20, rolloff=0.9, max_freq=5000.0)
            caw = formant(caw, ((1200.0, 1.0), (2400.0, 0.5))) * np.sin(np.linspace(0, np.pi, s)) ** 0.7
            caw = normalize(spectral_filter(caw, None, 2500.0), 0.035)
            place(left, caw * (1 - pan), samples(start + index * 0.42), circular=True)
            place(right, caw * pan, samples(start + index * 0.42), circular=True)

    for _ in range(9):
        length = samples(RNG.uniform(0.8, 1.6))
        start = samples(RNG.uniform(0.0, duration))
        pan = RNG.uniform(0.1, 0.9)
        rustle = band_noise(length, 1500.0, 6000.0) * np.sin(np.linspace(0, np.pi, length)) ** 2
        rustle *= 0.5 + 0.5 * np.abs(spectral_filter(noise(length), None, 25.0)) / 0.02
        rustle = normalize(rustle, 0.05)
        place(left, rustle * (1 - pan), start, circular=True)
        place(right, rustle * pan, start, circular=True)

    stereo = np.stack([left, right], axis=1)
    return normalize(stereo, 0.55)


class Track:
    def __init__(self, bpm, bars):
        self.beat = 60.0 / bpm
        self.n = samples(bars * 4 * self.beat)
        self.left = np.zeros(self.n)
        self.right = np.zeros(self.n)

    def at(self, beats):
        return int(round(beats * self.beat * SR))

    def put(self, sound, beats, pan=0.5, gain=1.0):
        place(self.left, sound * gain * math.sqrt(1 - pan), self.at(beats), circular=True)
        place(self.right, sound * gain * math.sqrt(pan), self.at(beats), circular=True)

    def mix(self, drive, peak):
        stereo = np.stack([self.left, self.right], axis=1)
        return normalize(np.tanh(normalize(stereo, 1.0) * drive), peak)


def taiko(seconds=0.6, freq=62.0):
    n = samples(seconds)
    t = time_axis(n)
    body = tone(freq * 1.9 * np.exp(-t * 25.0) + freq, n) * decay(n, seconds * 0.4)
    skin = band_noise(n, 150.0, 1500.0) * decay(n, 0.02)
    return normalize(body + normalize(skin, 0.4), 1.0)


def frame_drum():
    n = samples(0.3)
    t = time_axis(n)
    body = tone(180.0 * np.exp(-t * 20.0) + 140.0, n) * decay(n, 0.06)
    jingle = band_noise(n, 5000.0, 12000.0) * decay(n, 0.05) * 0.35
    return normalize(body + jingle, 0.8)


def string_pad(names, seconds, attack=0.6, release=0.9, cutoff=1600.0):
    n = samples(seconds)
    t = time_axis(n)
    out = np.zeros(n)
    for name in names:
        for detune in (0.996, 1.0, 1.004):
            vibrato = 1.0 + 0.003 * np.sin(2 * np.pi * 5.0 * t + RNG.uniform(0, 6))
            out += additive(note(name) * detune * vibrato, n, 24, rolloff=1.0, max_freq=5000.0)
    out = spectral_filter(out, 80.0, cutoff)
    return normalize(out * attack_release(n, attack, release), 1.0)


def choir_pad(names, seconds, vowel=((650.0, 1.0), (1080.0, 0.5), (2650.0, 0.2))):
    n = samples(seconds)
    t = time_axis(n)
    out = np.zeros(n)
    for name in names:
        for detune in (0.997, 1.003):
            vibrato = 1.0 + 0.007 * np.sin(2 * np.pi * 5.3 * t + RNG.uniform(0, 6))
            out += additive(note(name) * detune * vibrato, n, 20, rolloff=1.3, max_freq=5000.0)
    out = formant(out, vowel)
    return normalize(out * attack_release(n, 0.5, 0.8), 1.0)


def bowed(name, seconds):
    n = samples(seconds)
    t = time_axis(n)
    vibrato = 1.0 + 0.005 * np.sin(2 * np.pi * 5.5 * t) * np.clip(t / 0.3, 0, 1)
    out = additive(note(name) * vibrato, n, 18, rolloff=1.1, max_freq=6000.0)
    out = spectral_filter(out, 200.0, 3500.0)
    return normalize(out * attack_release(n, 0.08, min(0.3, seconds * 0.4)), 1.0)


def up(name, octaves):
    return name[:-1] + str(int(name[-1]) + octaves)


def music():
    track = Track(96.0, 16)
    bar_seconds = track.beat * 4

    progression = (
        ("D2", ("D4", "F4", "A4"), ("D3", "F3", "A3")),
        ("A#1", ("A#3", "D4", "F4"), ("A#2", "D3", "F3")),
        ("G1", ("G3", "A#3", "D4"), ("G2", "A#2", "D3")),
        ("A1", ("A3", "C#4", "E4"), ("A2", "C#3", "E3")),
    )

    melodies = (
        ((0, "A4", 1.5), (1.5, "D5", 0.5), (2, "F5", 1.0), (3, "E5", 1.0)),
        ((0, "D5", 1.5), (1.5, "F5", 0.5), (2, "D5", 1.0), (3, "A#4", 1.0)),
        ((0, "G4", 1.0), (1, "A#4", 1.0), (2, "D5", 1.5), (3.5, "C5", 0.5)),
        ((0, "C#5", 2.0), (2, "E5", 1.0), (3, "A4", 1.0)),
    )

    big = taiko(0.7, 55.0)
    small = taiko(0.4, 85.0)
    frame = frame_drum()

    for bar in range(16):
        chord_index = bar % 4
        bass_note, pad_notes, arp_notes = progression[chord_index]
        section = bar // 4
        base = bar * 4

        track.put(string_pad(pad_notes, bar_seconds + 0.9), base, 0.5, 0.22)

        arp = (arp_notes[0], arp_notes[1], arp_notes[2], up(arp_notes[0], 1), arp_notes[2], arp_notes[1], up(arp_notes[0], 1), up(arp_notes[1], 1))
        for step, name in enumerate(arp):
            length = samples(track.beat * 1.5)
            track.put(pluck(note(name), length, 1.2, 0.9) * attack_release(length, 0.002, 0.1), base + step * 0.5, 0.3 + 0.4 * (step % 2), 0.18)

        bass = additive(note(bass_note), samples(bar_seconds), 20, rolloff=1.2, max_freq=1200.0) * attack_release(samples(bar_seconds), 0.05, 0.4)
        track.put(normalize(bass, 1.0), base, 0.5, 0.2 if section == 0 else 0.3)

        if section >= 1:
            track.put(big, base, 0.5, 0.5)
            track.put(small, base + 2.5, 0.4, 0.3)
            track.put(big, base + 3, 0.5, 0.35)
            for beat in (1, 2, 3):
                track.put(frame, base + beat, 0.65, 0.18)
            if section >= 2:
                for beat in (0.5, 1.5, 2.5, 3.5):
                    track.put(frame, base + beat, 0.35, 0.1)

        if section >= 2:
            track.put(choir_pad(arp_notes, bar_seconds + 0.8), base, 0.5, 0.2)
            for start, name, length in melodies[chord_index]:
                melody_name = up(name, 1) if section == 3 and length <= 1.0 else name
                track.put(bowed(melody_name, track.beat * length + 0.25), base + start, 0.55, 0.2)

        if bar == 15:
            for roll in range(8):
                track.put(small, base + 2 + roll * 0.25, 0.5, 0.15 + roll * 0.03)

    return track.mix(1.2, 0.8)


def boss_music():
    track = Track(120.0, 16)
    bar_seconds = track.beat * 4

    progression = (
        ("D2", ("D3", "F3", "A3")),
        ("D2", ("D3", "F3", "A3")),
        ("A#1", ("A#2", "D3", "F3")),
        ("A1", ("A2", "C#3", "E3")),
    )

    big = taiko(0.6, 50.0)
    small = taiko(0.35, 90.0)
    frame = frame_drum()

    stab_n = samples(track.beat * 0.45)

    for bar in range(16):
        chord_index = bar % 4
        bass_note, chord = progression[chord_index]
        section = bar // 4
        base = bar * 4

        for eighth in range(8):
            length = samples(track.beat * 0.48)
            octave = 2.0 if eighth % 4 == 3 else 1.0
            body = additive(note(bass_note) * octave, length, 24, rolloff=1.0, max_freq=2500.0)
            body *= attack_release(length, 0.003, 0.04) * (0.5 + 0.5 * decay(length, 0.08))
            track.put(normalize(body, 1.0), base + eighth * 0.5, 0.5, 0.32)

        for beat in range(4):
            track.put(big, base + beat, 0.5, 0.55 if beat in (0, 2) else 0.4)
            track.put(frame, base + beat + 0.5, 0.6, 0.2)
            if section >= 1:
                track.put(small, base + beat + 0.75, 0.35, 0.2)

        if section >= 1:
            for stab in (0.0, 1.5, 3.0):
                body = np.zeros(stab_n)
                for name in chord:
                    body += additive(note(up(name, 1)), stab_n, 20, rolloff=1.0, max_freq=5000.0)
                body = spectral_filter(body, 150.0, 2800.0) * decay(stab_n, 0.18) * attack_release(stab_n, 0.01, 0.05)
                track.put(normalize(body, 1.0), base + stab, 0.5, 0.25)

        track.put(choir_pad([up(name, 1) for name in chord], bar_seconds + 0.6, ((500.0, 1.0), (850.0, 0.6), (2500.0, 0.2))), base, 0.5, 0.18 if section < 2 else 0.26)

        if section >= 2:
            pattern = (chord[0], chord[1], chord[2], chord[1])
            for step in range(16):
                name = up(pattern[step % 4], 2)
                length = samples(track.beat * 0.24)
                body = additive(note(name), length, 10, rolloff=1.2, max_freq=7000.0) * attack_release(length, 0.004, 0.04)
                track.put(normalize(body, 1.0), base + step * 0.25, 0.3 + 0.4 * (step % 2), 0.08)

        if section == 3 and bar % 2 == 1:
            for roll in range(8):
                track.put(small, base + 2 + roll * 0.25, 0.5, 0.15 + roll * 0.04)

    return track.mix(1.4, 0.8)


def main():
    sounds = {
        "Bolt": arcane_bolt,
        "BoltHit": bolt_hit,
        "BladeHit": blade_hit,
        "EnemyHit": enemy_hit,
        "Nova": nova,
        "FireballCast": fireball_cast,
        "Explosion": explosion,
        "MeteorImpact": lambda: explosion(2.2, 38.0, 1.8),
        "Lightning": lightning,
        "PoisonBurst": poison_burst,
        "Fuse": fuse,
        "HolyBlast": holy_blast,
        "Slam": slam,
        "GhoulDeath": ghoul_death,
        "BoneShatter": bone_shatter,
        "BatDeath": bat_death,
        "WraithDeath": wraith_death,
        "BruteDeath": brute_death,
        "BruteRoar": brute_roar,
        "ArrowShot": arrow_shot,
        "LichCast": lich_cast,
        "LichRise": lich_rise,
        "LichDeath": lich_death,
        "Swarm": swarm,
        "PlayerHurt": player_hurt,
        "PlayerDeath": player_death,
        "Dash": dash,
        "Gem": gem,
        "LevelUp": level_up,
        "Select": select,
        "Choose": choose,
        "Potion": potion,
        "Magnet": magnet,
        "ChestOpen": chest_open,
        "Treasure": treasure,
        "Victory": victory,
        "GameOver": game_over,
    }

    for name, build in sounds.items():
        write(name, build(), bitrate=96000)

    write("Ambience", ambience(), bitrate=128000)
    write("Music", music(), bitrate=160000)
    write("BossMusic", boss_music(), bitrate=160000)


main()
