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


def note(name):
    names = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
    pitch, octave = name[:-1], int(name[-1])
    midi = names[pitch] + (octave + 1) * 12
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def resonant_lowpass(x, cutoff, resonance):
    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=np.float64), x.shape)
    g = np.tan(np.pi * np.minimum(cutoff, SR * 0.45) / SR)
    k = 2.0 - 2.0 * resonance
    a1 = 1.0 / (1.0 + g * (g + k))
    y = np.zeros_like(x)
    ic1, ic2 = 0.0, 0.0
    for i in range(len(x)):
        v3 = x[i] - ic2
        v1 = a1[i] * ic1 + g[i] * a1[i] * v3
        v2 = ic2 + g[i] * v1
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        y[i] = v2
    return y


def saw(freq, n, max_freq=9000.0):
    return additive(freq, n, 48, rolloff=1.0, max_freq=max_freq)


def supersaw(freq, n, voices=5, spread=0.012, max_freq=8000.0):
    out = np.zeros(n)
    for index in range(voices):
        detune = 1.0 + spread * (index - (voices - 1) * 0.5) / max(voices - 1, 1)
        out += np.roll(saw(freq * detune, n, max_freq), int(RNG.integers(0, 2000)))
    return out / voices


def music():
    bpm = 140.0
    beat = 60.0 / bpm
    sixteenth = beat / 4.0
    bars = 32
    n = samples(bars * 4 * beat)
    left = np.zeros(n)
    right = np.zeros(n)

    def at(beats):
        return int(round(beats * beat * SR))

    def put(sound, beats, pan=0.5, gain=1.0):
        place(left, sound * gain * math.sqrt(1.0 - pan), at(beats), circular=True)
        place(right, sound * gain * math.sqrt(pan), at(beats), circular=True)

    def section_of(bar):
        return ("intro", "build", "drop", "drop", "break", "build", "drop", "outro")[bar // 4]

    kick_n = samples(0.32)
    kt = time_axis(kick_n)
    kick = tone(50.0 + 130.0 * np.exp(-kt * 38.0), kick_n) * decay(kick_n, 0.17)
    kick = np.tanh(kick * 1.6) + band_noise(kick_n, 2000.0, 8000.0) * decay(kick_n, 0.003) * 0.4

    snare_n = samples(0.28)
    snare = normalize(band_noise(snare_n, 900.0, 9000.0) * decay(snare_n, 0.075), 0.8)
    snare += tone(185.0 * np.exp(-time_axis(snare_n) * 3.0), snare_n) * decay(snare_n, 0.05) * 0.5

    hat_n = samples(0.06)
    hat = normalize(band_noise(hat_n, 7500.0, 17000.0) * decay(hat_n, 0.012), 0.22)
    open_n = samples(0.32)
    open_hat = normalize(band_noise(open_n, 6500.0, 17000.0) * decay(open_n, 0.09), 0.2)
    crash_n = samples(1.6)
    crash = normalize(band_noise(crash_n, 4000.0, 16000.0) * decay(crash_n, 0.55), 0.35)

    break_kicks = (0, 6, 10)
    break_snares = (4, 12)
    ghost_snares = (7, 9, 14, 15)

    progression = (
        ("D2", ("D4", "F4", "A4")),
        ("A#1", ("A#3", "D4", "F4")),
        ("C2", ("C4", "E4", "G4")),
        ("A1", ("A3", "C4", "E4")),
    )
    acid_pattern = (
        (0, 0, True, False), (2, 12, False, False), (3, 0, False, True), (4, 0, False, False),
        (6, 10, True, False), (7, 12, False, True), (8, 0, False, False), (10, 7, True, False),
        (11, 0, False, False), (12, 12, False, True), (13, 15, True, False), (14, 0, False, False),
    )

    bass_freq = np.full(n, note("D2"))
    bass_gate = np.zeros(n)
    bass_accent = np.zeros(n)

    pump = np.ones(n)
    duck_n = samples(beat * 0.8)
    duck = 1.0 - 0.45 * np.exp(-time_axis(duck_n) / 0.08)

    for bar in range(bars):
        section = section_of(bar)
        root, chord = progression[bar % 4]
        base = bar * 4

        if bar % 8 == 0:
            put(crash, base, 0.6, 0.9)

        four_floor = section in ("build", "drop", "outro")
        breakbeat = section in ("build", "drop", "break")

        if four_floor or section == "intro":
            for b in range(4):
                put(kick, base + b, 0.5, 0.6)
                index = (np.arange(duck_n) + at(base + b)) % n
                pump[index] = np.minimum(pump[index], duck)

        if breakbeat:
            for step in break_kicks:
                if not four_floor or step % 4:
                    put(kick, base + step * 0.25, 0.5, 0.45)
            for step in break_snares:
                put(snare, base + step * 0.25, 0.48, 0.85)
            for step in ghost_snares:
                put(snare, base + step * 0.25, 0.55, 0.28)

        for step in range(16):
            if step % 2 == 1 or section in ("drop", "outro"):
                put(hat, base + step * 0.25, 0.7 if step % 2 else 0.3, 1.4 if step % 2 else 0.7)
        if section != "intro":
            for b in range(4):
                put(open_hat, base + b + 0.5, 0.72, 0.8)

        for step, semis, accent, slide in acid_pattern:
            start = at(base + step * 0.25)
            length = samples(sixteenth * (1.0 if slide else 0.55))
            index = (np.arange(length) + start) % n
            bass_freq[index] = note(root) * 2.0 * 2.0 ** (semis / 12.0)
            bass_gate[index] = 1.0
            bass_accent[index] = np.maximum(bass_accent[index], (1.0 if accent else 0.45) * decay(length, 0.09))

        if section in ("build", "drop", "outro"):
            for stab in (0.0, 0.75, 1.5, 2.5, 3.25):
                length = samples(beat * 0.38)
                body = np.zeros(length)
                for name in chord:
                    body += supersaw(note(name) * 2.0, length, voices=3, spread=0.01, max_freq=7000.0)
                body = spectral_filter(body, 200.0, 4500.0)
                body *= decay(length, 0.1) * attack_release(length, 0.002, 0.03)
                gain = 0.34 if section == "drop" else 0.24
                pan = 0.25 if stab % 1 else 0.75
                put(normalize(body, 1.0), base + stab, pan, gain)
                put(normalize(body, 1.0), base + stab + 0.06, 1.0 - pan, gain * 0.55)

        if section in ("intro", "break", "outro") or bar % 2 == 0:
            length = samples(beat * (4 if section in ("intro", "break", "outro") else 8))
            pad = np.zeros(length)
            for name in chord:
                pad += supersaw(note(name), length, voices=4, spread=0.008, max_freq=3000.0)
            pad = spectral_filter(pad, 120.0, 1800.0) * attack_release(length, beat * 0.8, beat * 1.2)
            pad_gain = 0.16 if section != "break" else 0.24
            put(normalize(pad, 1.0), base, 0.2, pad_gain)
            put(normalize(pad, 1.0), base + 0.05, 0.8, pad_gain)

        if section == "drop":
            tones = [note(name) * 2.0 for name in chord] + [note(chord[0]) * 4.0]
            for step in range(16):
                freq = tones[(step + (step // 4)) % len(tones)]
                length = samples(sixteenth * 0.9)
                body = additive(freq, length, 9, rolloff=1.0, odd_only=True, max_freq=8000.0)
                body *= decay(length, 0.07) * attack_release(length, 0.002, 0.02)
                put(body, base + step * 0.25, 0.62, 0.13)
                put(body * 0.5, base + step * 0.25 + 0.75, 0.2, 0.13)

        if section == "drop" and bar % 4 == 0:
            length = samples(beat * 3.5)
            t = time_axis(length)
            bend = np.where(t < 0.12, 2.0 ** (-7.0 * (1.0 - t / 0.12) / 12.0), 1.0) * (1.0 + 0.004 * np.sin(2.0 * np.pi * 5.5 * t))
            hoover = supersaw(note(root) * 4.0 * bend, length, voices=7, spread=0.03, max_freq=6000.0)
            hoover = spectral_filter(hoover, 150.0, 3200.0) * attack_release(length, 0.02, beat)
            put(normalize(hoover, 1.0), base, 0.4, 0.2)
            put(normalize(hoover, 1.0), base + 0.04, 0.6, 0.14)

        if bar == bars - 1 or bar == 15:
            length = samples(beat * 4)
            sweep = band_noise(length, 800.0, 12000.0) * np.linspace(0.0, 1.0, length) ** 2
            put(normalize(sweep, 1.0), base, 0.5, 0.18)

    sweep = 0.5 - 0.5 * np.cos(2.0 * np.pi * np.arange(n) / samples(8 * 4 * beat))
    cutoff = 320.0 + 3600.0 * sweep ** 1.5 + 3000.0 * bass_accent
    acid = saw(bass_freq, n, max_freq=9000.0) * bass_gate
    preroll = samples(2.0)
    acid = resonant_lowpass(np.concatenate([acid[-preroll:], acid]), np.concatenate([cutoff[-preroll:], cutoff]), 0.9)[preroll:]
    acid = np.tanh(normalize(acid, 1.0) * 2.4)
    left += acid * 0.34
    right += acid * 0.3

    mixed = np.stack([left, right], axis=1) * pump[:, None]
    mixed = np.tanh(normalize(mixed, 1.0) * 1.25)
    return normalize(mixed, 0.8)


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
    write("Music", music(), bitrate=160000)


main()
