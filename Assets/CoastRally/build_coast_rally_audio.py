import math
from pathlib import Path

import aud
import numpy as np


SR = 44100
OUT = Path(__file__).resolve().parent / "Audio"
RNG = np.random.default_rng(23)


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


def tone(freq, n, phase=0.0):
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), (n,))
    return np.sin(phase + 2.0 * np.pi * np.cumsum(freq) / SR)


def additive(freq, n, harmonics, rolloff=1.0, odd_only=False, max_freq=12000.0):
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), (n,))
    phase = 2.0 * np.pi * np.cumsum(freq) / SR
    out = np.zeros(n)
    top = float(np.max(freq))
    for h in range(1, harmonics + 1):
        if odd_only and h % 2 == 0:
            continue
        if h * top > max_freq:
            break
        out += np.sin(phase * h) / h ** rolloff
    return out


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


def periodic(n, cycles, phase=0.0):
    return np.sin(2.0 * np.pi * cycles * np.arange(n) / n + phase)


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


def engine():
    n = samples(2.0)
    firing = 100.0
    t = time_axis(n)
    pulse = (0.5 + 0.5 * np.sin(2.0 * np.pi * firing * t)) ** 6
    body = additive(firing, n, 14, rolloff=1.1)
    crank = np.sin(2.0 * np.pi * firing * 0.5 * t) * 0.6
    rumble = 1.0 + 0.18 * np.sin(2.0 * np.pi * 12.5 * t) + 0.08 * np.sin(2.0 * np.pi * 37.5 * t)
    intake = normalize(spectral_filter(noise(n), 300.0, 2600.0, circular=True), 1.0) * (0.25 + 0.75 * pulse)
    exhaust = normalize(spectral_filter(noise(n), 60.0, 500.0, circular=True), 1.0) * pulse
    mix = (body * 0.8 + crank + intake * 0.22 + exhaust * 0.5) * rumble
    shaped = np.tanh(normalize(mix, 1.0) * 1.8)
    shaped = spectral_filter(shaped, 30.0, 5000.0, circular=True)
    return normalize(shaped, 0.8)


def skid():
    n = samples(3.0)
    squeal = np.zeros(n)
    for base, cycles, depth, gain in ((1180.0, 2, 45.0, 1.0), (1730.0, 3, 60.0, 0.6), (2410.0, 4, 80.0, 0.35)):
        freq = base + depth * periodic(n, cycles)
        squeal += tone(freq, n) * gain * (0.75 + 0.25 * periodic(n, cycles * 3, 1.3))
    hiss = normalize(spectral_filter(noise(n), 700.0, 4000.0, circular=True), 1.0)
    out = normalize(squeal, 1.0) * 0.7 + hiss * 0.45
    return normalize(spectral_filter(out, 400.0, 6000.0, circular=True), 0.7)


def wind():
    n = samples(6.0)
    rush = normalize(spectral_filter(noise(n), 90.0, 1100.0, tilt=-0.4, circular=True), 1.0)
    whistle = normalize(spectral_filter(noise(n), 1400.0, 2600.0, order=4, circular=True), 1.0) * 0.2
    swell = 0.8 + 0.12 * periodic(n, 2) + 0.08 * periodic(n, 5, 0.7)
    return normalize((rush + whistle) * swell, 0.7)


def gull(n):
    t = time_axis(n)
    freq = 1500.0 + 900.0 * np.sin(np.linspace(0.0, np.pi, n)) - 500.0 * t / (n / SR)
    voice = additive(freq, n, 5, rolloff=1.4) * np.sin(np.linspace(0.0, np.pi, n)) ** 1.5
    rasp = band_noise(n, 1500.0, 4000.0) * 0.15 * np.sin(np.linspace(0.0, np.pi, n))
    return voice + rasp


def ambience():
    n = samples(24.0)
    left = np.zeros(n)
    right = np.zeros(n)

    for side, phase in ((left, 0.0), (right, 1.9)):
        surf = normalize(spectral_filter(noise(n), 60.0, 900.0, tilt=-0.3, circular=True), 1.0)
        swell = np.clip(0.35 + 0.65 * (0.5 + 0.5 * periodic(n, 4, phase)) ** 3 + 0.15 * periodic(n, 7, phase * 0.5), 0.0, 1.2)
        foam = normalize(spectral_filter(noise(n), 1500.0, 7000.0, circular=True), 1.0) * (0.5 + 0.5 * periodic(n, 4, phase - 0.6)) ** 6
        side += surf * swell * 0.7 + foam * 0.18

    breeze = normalize(spectral_filter(noise(n), 200.0, 1800.0, tilt=-0.5, circular=True), 0.12)
    left += breeze
    right += breeze

    for _ in range(5):
        start = samples(RNG.uniform(0.0, 24.0))
        pan = RNG.uniform(0.15, 0.85)
        for call in range(RNG.integers(2, 4)):
            length = samples(RNG.uniform(0.25, 0.4))
            cry = gull(length) * 0.09
            place(left, cry * (1.0 - pan), start + call * samples(0.45), circular=True)
            place(right, cry * pan, start + call * samples(0.45), circular=True)

    for _ in range(8):
        start = samples(RNG.uniform(0.0, 24.0))
        pan = RNG.uniform(0.1, 0.9)
        base = RNG.uniform(3000.0, 4600.0)
        for chirp in range(RNG.integers(2, 5)):
            length = samples(RNG.uniform(0.05, 0.09))
            freq = base + 900.0 * np.sin(np.linspace(0, np.pi, length))
            bird = tone(freq, length) * np.sin(np.linspace(0, np.pi, length)) * 0.035
            place(left, bird * (1 - pan), start + chirp * samples(0.12), circular=True)
            place(right, bird * pan, start + chirp * samples(0.12), circular=True)

    return normalize(np.stack([left, right], axis=1), 0.6)


def impact():
    n = samples(0.7)
    t = time_axis(n)
    thud = tone(110.0 * np.exp(-t * 6.0) + 40.0, n) * decay(n, 0.12)
    crunch = band_noise(n, 900.0, 5000.0) * decay(n, 0.07)
    rattle = np.zeros(n)
    for freq in (520.0, 870.0, 1310.0, 2050.0):
        rattle += np.sin(2.0 * np.pi * freq * t + RNG.uniform(0, 6)) * decay(n, RNG.uniform(0.06, 0.16))
    body = band_noise(n, 80.0, 700.0) * decay(n, 0.1)
    out = normalize(thud, 1.0) + normalize(crunch, 0.6) + normalize(rattle, 0.35) + normalize(body, 0.5)
    return normalize(fade_edges(np.tanh(out * 1.4)), 0.9)


def land():
    n = samples(0.5)
    t = time_axis(n)
    thump = tone(75.0 * np.exp(-t * 4.0) + 35.0, n) * decay(n, 0.11)
    spring = np.sin(2.0 * np.pi * 240.0 * t) * decay(n, 0.05) * 0.2
    dirt = band_noise(n, 150.0, 2500.0) * decay(n, 0.08)
    return normalize(fade_edges(normalize(thump, 1.0) + spring + normalize(dirt, 0.45)), 0.85)


def splash():
    n = samples(1.6)
    t = time_axis(n)
    hit = band_noise(n, 250.0, 5000.0) * decay(n, 0.25) * np.clip(t / 0.01, 0, 1)
    wash = band_noise(n, 120.0, 1800.0) * np.exp(-((t - 0.45) / 0.35) ** 2)
    out = normalize(hit, 1.0) + normalize(wash, 0.6)
    for _ in range(18):
        length = samples(RNG.uniform(0.03, 0.07))
        freq = RNG.uniform(400.0, 1100.0) * np.exp(np.linspace(0, 0.6, length))
        bubble = tone(freq, length) * np.sin(np.linspace(0, np.pi, length)) * 0.2
        place(out, bubble, samples(RNG.uniform(0.15, 1.2)))
    return normalize(fade_edges(out, fade_out=0.1), 0.85)


def reset():
    n = samples(0.6)
    t = time_axis(n)
    whoosh = band_noise(n, 300.0, 3500.0) * np.sin(np.linspace(0, np.pi, n)) ** 2
    chime = np.zeros(n)
    for index, freq in enumerate((784.0, 1046.5, 1568.0)):
        start = samples(0.12 + index * 0.08)
        length = n - start
        place(chime, additive(freq, length, 3, rolloff=2.0) * decay(length, 0.18), start)
    return normalize(fade_edges(normalize(whoosh, 0.5) + normalize(chime, 0.6), fade_out=0.05), 0.75)


def cone():
    n = samples(0.35)
    t = time_axis(n)
    hollow = np.sin(2.0 * np.pi * 310.0 * t) * decay(n, 0.05) + np.sin(2.0 * np.pi * 740.0 * t) * decay(n, 0.03) * 0.5
    slap = band_noise(n, 500.0, 3000.0) * decay(n, 0.02)
    return normalize(fade_edges(normalize(hollow, 1.0) + normalize(slap, 0.6)), 0.8)


def barrel():
    n = samples(0.9)
    t = time_axis(n)
    drum = np.zeros(n)
    for freq, gain, length in ((176.0, 1.0, 0.28), (283.0, 0.7, 0.2), (419.0, 0.5, 0.15), (607.0, 0.35, 0.1), (1190.0, 0.2, 0.06)):
        drum += np.sin(2.0 * np.pi * freq * t + RNG.uniform(0, 6)) * decay(n, length) * gain
    clang = band_noise(n, 1500.0, 6000.0) * decay(n, 0.03)
    return normalize(fade_edges(normalize(drum, 1.0) + normalize(clang, 0.4)), 0.85)


def crate():
    n = samples(0.5)
    t = time_axis(n)
    crack = band_noise(n, 900.0, 6000.0) * decay(n, 0.025)
    knock = (np.sin(2.0 * np.pi * 210.0 * t) + 0.5 * np.sin(2.0 * np.pi * 455.0 * t)) * decay(n, 0.06)
    splinter = np.zeros(n)
    for _ in range(6):
        length = samples(0.02)
        place(splinter, band_noise(length, 2000.0, 7000.0) * decay(length, 0.006), samples(RNG.uniform(0.02, 0.2)))
    return normalize(fade_edges(normalize(crack, 0.9) + normalize(knock, 1.0) + normalize(splinter, 0.4)), 0.85)


def hay():
    n = samples(0.45)
    thump = band_noise(n, 80.0, 700.0) * decay(n, 0.09)
    rustle = band_noise(n, 2000.0, 8000.0) * decay(n, 0.12) * np.clip(time_axis(n) / 0.02, 0, 1)
    return normalize(fade_edges(normalize(thump, 1.0) + normalize(rustle, 0.35)), 0.75)


def select():
    n = samples(0.12)
    t = time_axis(n)
    click = (np.sin(2.0 * np.pi * 1320.0 * t) + 0.4 * np.sin(2.0 * np.pi * 2640.0 * t)) * decay(n, 0.025)
    tick = band_noise(n, 2500.0, 8000.0) * decay(n, 0.004)
    return normalize(fade_edges(normalize(click, 1.0) + normalize(tick, 0.4)), 0.6)


def main():
    sounds = {
        "Impact": impact,
        "Land": land,
        "Splash": splash,
        "Reset": reset,
        "Cone": cone,
        "Barrel": barrel,
        "Crate": crate,
        "Hay": hay,
    }

    for name, build in sounds.items():
        write(name, build(), bitrate=96000)

    write("Engine", engine(), bitrate=128000)
    write("Skid", skid(), bitrate=96000)
    write("Wind", wind(), bitrate=96000)
    write("Ambience", ambience(), bitrate=128000)
    write("Select", select(), bitrate=96000)


main()
