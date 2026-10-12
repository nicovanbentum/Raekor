import math
from pathlib import Path

import aud
import numpy as np


SR = 44100
OUT = Path(__file__).resolve().parent / "Audio"
RNG = np.random.default_rng(7)


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


def coin():
    n = samples(0.42)
    first = samples(0.07)
    out = np.zeros(n)
    for freq, start, length, amp in ((note("B5"), 0, first, 0.8), (note("E6"), first, n - first, 1.0)):
        body = tone(freq, length) + 0.35 * tone(freq * 2, length) + 0.15 * tone(freq * 3, length)
        place(out, body * decay(length, 0.11 if start else 0.05) * amp, start)
    return fade_edges(normalize(out, 0.8))


def jump():
    n = samples(0.32)
    t = time_axis(n)
    sweep = svf_bandpass(noise(n), 500.0 + 3500.0 * (t / t[-1]) ** 0.7, 2.0)
    blip = tone(260.0 + 520.0 * (t / t[-1]), n) * decay(n, 0.06) * 0.35
    out = sweep * attack_release(n, 0.04, 0.2) + blip
    return fade_edges(normalize(out, 0.7))


def land():
    n = samples(0.3)
    t = time_axis(n)
    thump = tone(95.0 * np.exp(-t * 9.0) + 40.0, n) * decay(n, 0.07)
    scuff = band_noise(n, 300.0, 2500.0) * decay(n, 0.035)
    return fade_edges(normalize(thump + 0.6 * normalize(scuff, 1.0), 0.75))


def step():
    n = samples(0.12)
    t = time_axis(n)
    click = band_noise(n, 1200.0, 6000.0) * decay(n, 0.012)
    thud = tone(120.0 * np.exp(-t * 20.0) + 50.0, n) * decay(n, 0.025)
    return fade_edges(normalize(normalize(click, 1.0) * 0.7 + thud, 0.6))


def slide():
    n = samples(0.6)
    t = time_axis(n)
    grit = 0.6 + 0.4 * spectral_filter(noise(n), None, 40.0)
    sweep = svf_bandpass(noise(n), 3200.0 * np.exp(-t * 2.5) + 600.0, 1.4) * grit
    return fade_edges(normalize(sweep * attack_release(n, 0.03, 0.35), 0.7))


def lane():
    n = samples(0.24)
    t = time_axis(n) / (n / SR)
    cutoff = 700.0 + 2600.0 * np.sin(np.pi * t)
    out = svf_bandpass(noise(n), cutoff, 2.5) * np.sin(np.pi * t) ** 1.5
    return fade_edges(normalize(out, 0.6))


def metal_ring(n, partials, seconds):
    out = np.zeros(n)
    for index, freq in enumerate(partials):
        out += tone(freq, n, RNG.uniform(0, 6.28)) * decay(n, seconds / (1 + index * 0.6)) / (1 + index * 0.4)
    return out


def stumble():
    n = samples(0.5)
    t = time_axis(n)
    thump = tone(110.0 * np.exp(-t * 12.0) + 45.0, n) * decay(n, 0.08)
    ring = metal_ring(n, (523.0, 1307.0, 2210.0, 3145.0), 0.18) * 0.35
    rattle = band_noise(n, 800.0, 5000.0) * decay(n, 0.05) * 0.5
    return fade_edges(normalize(thump + ring + rattle, 0.8))


def crash():
    n = samples(1.8)
    t = time_axis(n)
    boom = tone(70.0 * np.exp(-t * 4.0) + 28.0, n) * decay(n, 0.45)
    burst = spectral_filter(noise(n), 60.0, 3500.0) * decay(n, 0.3)
    clang = metal_ring(n, (311.0, 742.0, 1188.0, 1876.0, 2633.0), 0.5) * 0.4
    debris = np.zeros(n)
    for _ in range(45):
        start = samples(RNG.uniform(0.05, 1.1) ** 1.5)
        length = samples(RNG.uniform(0.01, 0.04))
        grain = band_noise(length, RNG.uniform(1500, 3000), RNG.uniform(5000, 9000)) * decay(length, 0.008)
        place(debris, grain * RNG.uniform(0.2, 0.6) * math.exp(-start / SR * 2.0), start)
    out = boom + 0.7 * normalize(burst, 1.0) + clang + normalize(debris, 0.5)
    return fade_edges(normalize(np.tanh(out * 1.4), 0.9), fade_out=0.2)


def powerup():
    n = samples(0.9)
    out = np.zeros(n)
    for index, name in enumerate(("C6", "E6", "G6", "C7", "E7")):
        start = samples(index * 0.055)
        length = n - start
        freq = note(name)
        body = additive(freq, length, 9, rolloff=1.0, odd_only=True)
        place(out, body * decay(length, 0.16) * (0.6 + index * 0.1), start)
    sparkle = np.zeros(n)
    for _ in range(14):
        start = samples(RNG.uniform(0.15, 0.7))
        length = samples(0.12)
        place(sparkle, tone(RNG.uniform(4000, 7500), length) * decay(length, 0.03) * 0.25, start)
    return fade_edges(normalize(out + sparkle, 0.75), fade_out=0.1)


def start_chime():
    n = samples(0.7)
    out = np.zeros(n)
    for start, name in ((0.0, "G5"), (0.11, "C6")):
        s = samples(start)
        length = n - s
        freq = note(name)
        body = tone(freq, length) + 0.5 * tone(freq * 2, length) + 0.2 * tone(freq * 3.01, length)
        place(out, body * decay(length, 0.22), s)
    return fade_edges(normalize(out, 0.7), fade_out=0.1)


def wood_clack(n):
    t = time_axis(n)
    click = band_noise(n, 1500.0, 7000.0) * decay(n, 0.006)
    body = (tone(720.0, n) + 0.6 * tone(1310.0, n) + 0.3 * tone(2470.0, n)) * decay(n, 0.03)
    return normalize(click, 1.0) * 0.8 + body * 0.5


def skateboard_on():
    n = samples(0.5)
    out = np.zeros(n)
    place(out, wood_clack(samples(0.15)), 0)
    place(out, wood_clack(samples(0.15)) * 0.7, samples(0.12))
    roll = spectral_filter(noise(n), 150.0, 1400.0) * np.clip(time_axis(n) / 0.3, 0, 1) * 0.25
    return fade_edges(normalize(out + roll, 0.75), fade_out=0.15)


def skateboard_loop():
    period = samples(0.37)
    n = period * 8
    t = time_axis(n)
    roll = spectral_filter(noise(n), 120.0, 1100.0, circular=True)
    grit = spectral_filter(noise(n), 2000.0, 6000.0, circular=True) * 0.15
    wobble = 1.0 + 0.15 * np.sin(2 * np.pi * t * (8 / (n / SR)))
    out = (normalize(roll, 1.0) + grit) * wobble
    for index in range(8):
        length = samples(0.05)
        tick = band_noise(length, 600.0, 4000.0) * decay(length, 0.01)
        place(out, normalize(tick, 0.5), index * period + samples(0.02), circular=True)
    return normalize(out, 0.6)


def skateboard_smash():
    n = samples(1.0)
    out = np.zeros(n)
    crack = band_noise(n, 400.0, 9000.0) * decay(n, 0.03)
    out += normalize(crack, 1.0)
    for start in (0.0, 0.004, 0.012):
        length = samples(0.4)
        resonance = sum(tone(f * RNG.uniform(0.97, 1.03), length) * decay(length, 0.06 / (i + 1)) for i, f in enumerate((410.0, 870.0, 1530.0, 2890.0)))
        place(out, resonance * 0.4, samples(start))
    for _ in range(10):
        start = samples(RNG.uniform(0.08, 0.7))
        place(out, wood_clack(samples(0.12)) * RNG.uniform(0.15, 0.45) * math.exp(-start / SR * 3.0), start)
    return fade_edges(normalize(np.tanh(out * 1.3), 0.85), fade_out=0.15)


def whistle():
    n = samples(1.05)
    t = time_axis(n)
    trill = 1.0 + 0.5 * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 34.0 * t)))
    freq = 2850.0 + 90.0 * np.sin(2 * np.pi * 34.0 * t)
    body = tone(freq, n) + 0.25 * tone(freq * 2, n)
    breath = band_noise(n, 2000.0, 5000.0) * 0.15
    env = np.zeros(n)
    for start, end in ((0.0, 0.22), (0.32, 1.0)):
        s, e = samples(start), samples(end)
        env[s:e] = attack_release(e - s, 0.015, 0.05)
    out = (body * trill + breath) * env
    return fade_edges(normalize(out, 0.6))


def train_rumble():
    period = samples(1.2)
    n = period * 4
    t = time_axis(n)
    low = spectral_filter(noise(n), 25.0, 180.0, order=3, circular=True)
    mid = spectral_filter(noise(n), 250.0, 1600.0, circular=True)
    motor = (np.sin(2 * np.pi * 110.0 * t) * 0.5 + np.sin(2 * np.pi * 220.0 * t) * 0.25 + np.sin(2 * np.pi * 330.0 * t) * 0.1)
    out = normalize(low, 1.0) + normalize(mid, 0.35) + motor * 0.25
    for index in range(4):
        for offset in (0.0, 0.13):
            length = samples(0.18)
            clack = metal_ring(length, (480.0, 1150.0, 2300.0), 0.05) * 0.5 + band_noise(length, 300.0, 3000.0) * decay(length, 0.02)
            place(out, normalize(clack, 0.9), index * period + samples(offset), circular=True)
    return normalize(out, 0.7)


def train_horn():
    n = samples(1.6)
    t = time_axis(n)
    vibrato = 1.0 + 0.004 * np.sin(2 * np.pi * 5.5 * t)
    out = np.zeros(n)
    for freq in (277.0, 349.0, 415.0):
        out += additive(freq * vibrato, n, 30, rolloff=1.0, max_freq=6000.0)
    out = spectral_filter(out, 200.0, 2600.0) + spectral_filter(out, 600.0, 1400.0) * 1.5
    out *= attack_release(n, 0.06, 0.35)
    return fade_edges(normalize(np.tanh(normalize(out, 1.0) * 1.6), 0.8))


def ambience():
    n = samples(24.0)
    left = normalize(spectral_filter(noise(n), 30.0, 450.0, circular=True), 0.5)
    right = normalize(spectral_filter(noise(n), 30.0, 450.0, circular=True), 0.5)
    shared = normalize(spectral_filter(noise(n), 400.0, 2500.0, tilt=-0.5, circular=True), 0.12)
    left += shared
    right += shared

    for _ in range(6):
        length = samples(RNG.uniform(3.0, 5.0))
        start = samples(RNG.uniform(0.0, 24.0))
        swell = np.sin(np.linspace(0, np.pi, length)) ** 2
        car = spectral_filter(noise(length), 120.0, RNG.uniform(900.0, 1600.0)) * swell
        car = normalize(car, RNG.uniform(0.25, 0.4))
        pan = np.linspace(RNG.uniform(0, 1), RNG.uniform(0, 1), length)
        place(left, car * (1 - pan), start, circular=True)
        place(right, car * pan, start, circular=True)

    for _ in range(10):
        start = samples(RNG.uniform(0.0, 24.0))
        pan = RNG.uniform(0.1, 0.9)
        base = RNG.uniform(3200.0, 4800.0)
        for chirp in range(RNG.integers(2, 5)):
            length = samples(RNG.uniform(0.05, 0.1))
            freq = base + 1200.0 * np.sin(np.linspace(0, np.pi, length))
            bird = tone(freq, length) * np.sin(np.linspace(0, np.pi, length)) * 0.05
            place(left, bird * (1 - pan), start + chirp * samples(0.13), circular=True)
            place(right, bird * pan, start + chirp * samples(0.13), circular=True)

    length = samples(0.9)
    honk = (additive(410.0, length, 12) + additive(505.0, length, 12)) * attack_release(length, 0.03, 0.2)
    honk = normalize(spectral_filter(honk, 300.0, 1800.0), 0.06)
    place(left, honk * 0.4, samples(15.3), circular=True)
    place(right, honk * 0.6, samples(15.3), circular=True)

    stereo = np.stack([left, right], axis=1)
    return normalize(stereo, 0.6)


def music():
    bpm = 128.0
    beat = 60.0 / bpm
    bars = 16
    n = samples(bars * 4 * beat)
    left = np.zeros(n)
    right = np.zeros(n)

    def at(beats):
        return int(round(beats * beat * SR))

    def put(sound, beats, pan=0.5, gain=1.0):
        place(left, sound * gain * math.sqrt(1 - pan), at(beats), circular=True)
        place(right, sound * gain * math.sqrt(pan), at(beats), circular=True)

    kick_n = samples(0.35)
    kt = time_axis(kick_n)
    kick = tone(48.0 + 110.0 * np.exp(-kt * 30.0), kick_n) * decay(kick_n, 0.16)
    kick += band_noise(kick_n, 1500.0, 6000.0) * decay(kick_n, 0.004) * 0.3

    clap_n = samples(0.3)
    clap = np.zeros(clap_n)
    for offset in (0.0, 0.011, 0.022):
        length = clap_n - samples(offset)
        place(clap, band_noise(length, 900.0, 5000.0) * decay(length, 0.012 if offset < 0.02 else 0.09), samples(offset))
    clap = normalize(clap, 0.7) + tone(190.0, clap_n) * decay(clap_n, 0.05) * 0.25

    hat_n = samples(0.08)
    hat = normalize(band_noise(hat_n, 7000.0, 16000.0) * decay(hat_n, 0.018), 0.25)
    open_hat = normalize(band_noise(samples(0.3), 6000.0, 16000.0) * decay(samples(0.3), 0.08), 0.2)

    progression = (("A2", ("A3", "C4", "E4")), ("F2", ("F3", "A3", "C4")), ("C3", ("G3", "C4", "E4")), ("G2", ("G3", "B3", "D4")))
    lead_patterns = (
        ("E5", "C5", "A4", "C5", "E5", "G5", "E5", "C5"),
        ("C5", "A4", "F4", "A4", "C5", "F5", "C5", "A4"),
        ("E5", "G5", "C6", "G5", "E5", "C5", "E5", "G5"),
        ("D5", "B4", "G4", "B4", "D5", "G5", "D5", "B4"),
    )

    pump = np.ones(n)
    duck_n = samples(beat * 0.9)
    duck = 1.0 - 0.6 * np.exp(-time_axis(duck_n) / 0.09)

    for bar in range(bars):
        chord_index = bar % 4
        bass_note, chord = progression[chord_index]
        section = bar // 4

        for b in range(4):
            put(kick, bar * 4 + b, 0.5, 0.95)
            start = at(bar * 4 + b)
            index = (np.arange(duck_n) + start) % n
            pump[index] = np.minimum(pump[index], duck)
            if b in (1, 3):
                put(clap, bar * 4 + b, 0.5, 0.55)
            put(hat, bar * 4 + b + 0.5, 0.65, 0.8)
            if section >= 1:
                put(hat, bar * 4 + b + 0.25, 0.35, 0.35)
                put(hat, bar * 4 + b + 0.75, 0.35, 0.35)
            if b == 3 and section >= 2:
                put(open_hat, bar * 4 + b + 0.5, 0.7, 0.6)

        bass_freq = note(bass_note)
        for eighth in range(8):
            length = samples(beat * 0.45)
            octave = 2.0 if eighth % 2 == 1 else 1.0
            body = additive(bass_freq * octave, length, 12, rolloff=1.2, max_freq=2200.0)
            body *= attack_release(length, 0.004, 0.05) * (0.6 + 0.4 * decay(length, 0.12))
            put(body, bar * 4 + eighth * 0.5, 0.5, 0.42)

        for stab in (0.5, 1.5, 2.5, 3.25):
            length = samples(beat * 0.4)
            body = np.zeros(length)
            for name in chord:
                for detune in (0.997, 1.0, 1.003):
                    body += additive(note(name) * detune, length, 16, rolloff=1.0, max_freq=5000.0)
            body *= decay(length, 0.11) * attack_release(length, 0.003, 0.03)
            put(normalize(body, 1.0), bar * 4 + stab, 0.3 if stab % 1 == 0.5 else 0.7, 0.22)

        if section >= 1:
            pattern = lead_patterns[chord_index]
            for step, name in enumerate(pattern):
                if section == 1 and step % 2 == 1:
                    continue
                length = samples(beat * 0.45)
                body = additive(note(name), length, 11, rolloff=1.0, odd_only=True, max_freq=7000.0)
                body *= decay(length, 0.14) * attack_release(length, 0.002, 0.03)
                put(body, bar * 4 + step * 0.5, 0.55, 0.16)
                put(body * 0.45, bar * 4 + step * 0.5 + 0.75, 0.15, 0.16)
                put(body * 0.3, bar * 4 + step * 0.5 + 1.5, 0.85, 0.16)

        if section == 3 and bar % 2 == 1:
            length = samples(beat * 2)
            freq = note(chord[2]) * 2 * np.exp(np.linspace(0, math.log(2), length))
            riser = additive(freq, length, 6, odd_only=True) * np.linspace(0, 1, length) ** 2 * 0.08
            put(riser, bar * 4 + 2, 0.5, 1.0)

    drums_and_music = np.stack([left, right], axis=1)
    pumped = drums_and_music * pump[:, None]
    mixed = np.tanh(normalize(pumped, 1.0) * 1.3)
    return normalize(mixed, 0.8)


def main():
    sounds = {
        "Coin": coin,
        "Jump": jump,
        "Land": land,
        "Step": step,
        "Slide": slide,
        "Lane": lane,
        "Stumble": stumble,
        "Crash": crash,
        "PowerUp": powerup,
        "Start": start_chime,
        "SkateboardOn": skateboard_on,
        "SkateboardLoop": skateboard_loop,
        "SkateboardSmash": skateboard_smash,
        "Whistle": whistle,
        "TrainRumble": train_rumble,
        "TrainHorn": train_horn,
    }

    for name, build in sounds.items():
        write(name, build(), bitrate=96000)

    write("Ambience", ambience(), bitrate=128000)
    write("Music", music(), bitrate=160000)


main()
