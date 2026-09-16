#!/usr/bin/env python3
"""
Generate an authentic retro Three Kingdoms (三国) strategic BGM in 16kHz IMA ADPCM.
100% original composition in ancient Chinese pentatonic scale (羽调式 - A Minor Pentatonic).
Produces seamless looping chiptune audio with 4 classic voices:
1. Pulse 1 (50% duty lead melody)
2. Pulse 2 (25% duty counterpoint & arpeggio)
3. Triangle (bassline)
4. Noise (military marching snare / taiko cadence)
"""

import numpy as np
import struct
import os

SAMPLE_RATE = 16000
BPM = 104
BEAT_SAMPLES = int(SAMPLE_RATE * 60 / BPM)
SIXTEENTH = BEAT_SAMPLES // 4

# Chinese Pentatonic scale in A minor (A, C, D, E, G)
# MIDI Note to Hz: f = 440 * 2^((note - 69) / 12)
NOTE_HZ = {
    'A2': 110.0,  'C3': 130.81, 'D3': 146.83, 'E3': 164.81, 'G3': 196.0,
    'A3': 220.0,  'B3': 246.94, 'C4': 261.63, 'D4': 293.66, 'E4': 329.63,
    'G4': 392.0,  'A4': 440.0,  'B4': 493.88, 'C5': 523.25, 'D5': 587.33,
    'E5': 659.25, 'G5': 783.99, 'A5': 880.0,  'REST': 0.0
}

# 10-bar composition (40 beats = 160 sixteenth notes)
# Measures 1-4: Theme Introduction (Main Motive)
# Measures 5-8: Development & Tension
# Measures 9-10: Stately Resolution leading seamlessly back to M1
TOTAL_BARS = 10
TOTAL_BEATS = TOTAL_BARS * 4
TOTAL_SAMPLES = TOTAL_BEATS * BEAT_SAMPLES

print(f"Generating Baye Retro BGM: {TOTAL_BARS} bars, {TOTAL_BEATS} beats, {TOTAL_SAMPLES / SAMPLE_RATE:.2f} seconds")

# Voice 1: Pulse 50% (Lead Melody) - notes and durations in sixteenths
# (note_name, sixteenth_duration)
MELODY = [
    # M1-M4: Main Motive
    ('A4', 4), ('C5', 4), ('D5', 6), ('E5', 2),
    ('G5', 4), ('E5', 4), ('D5', 6), ('REST', 2),
    ('C5', 4), ('D5', 4), ('E5', 4), ('A4', 4),
    ('G4', 6), ('A4', 2), ('A4', 8),
    
    # M5-M8: Strategy & Expansion
    ('E5', 4), ('G5', 4), ('A5', 6), ('G5', 2),
    ('E5', 4), ('D5', 4), ('C5', 4), ('D5', 4),
    ('E5', 6), ('D5', 2), ('C5', 4), ('A4', 4),
    ('G4', 4), ('E4', 4), ('A4', 8),
    
    # M9-M10: Stately Cadence & Seamless Turnaround to M1
    ('D5', 4), ('C5', 4), ('A4', 6), ('G4', 2),
    ('A4', 12), ('REST', 4),
]

# Voice 2: Pulse 25% (Arpeggio Counterpoint - 20 half-bars = 10 bars)
ARPEGGIO_CHORDS = [
    # M1-M4
    ['A3', 'C4', 'E4', 'A4'], ['A3', 'C4', 'E4', 'A4'],
    ['G3', 'B3', 'D4', 'G4'], ['G3', 'B3', 'D4', 'G4'],
    ['F3', 'A3', 'C4', 'F4'], ['F3', 'A3', 'C4', 'F4'],
    ['E3', 'G3', 'B3', 'E4'], ['A3', 'C4', 'E4', 'A4'],
    # M5-M8
    ['C4', 'E4', 'G4', 'C5'], ['G3', 'B3', 'D4', 'G4'],
    ['A3', 'C4', 'E4', 'A4'], ['D3', 'F3', 'A3', 'D4'],
    ['E3', 'G3', 'B3', 'E4'], ['F3', 'A3', 'C4', 'F4'],
    ['G3', 'B3', 'D4', 'G4'], ['A3', 'C4', 'E4', 'A4'],
    # M9-M10
    ['D3', 'F3', 'A3', 'D4'], ['E3', 'G3', 'B3', 'E4'],
    ['A3', 'C4', 'E4', 'A4'], ['A3', 'C4', 'E4', 'A4'],
]

# Voice 3: Bassline (Triangle)
BASS_NOTES = [
    # M1-M4
    ('A2', 8), ('A2', 8), ('G2', 8), ('G2', 8),
    ('F2', 8), ('F2', 8), ('E2', 8), ('A2', 8),
    # M5-M8
    ('C3', 8), ('G2', 8), ('A2', 8), ('D2', 8),
    ('E2', 8), ('F2', 8), ('G2', 8), ('A2', 8),
    # M9-M10
    ('D2', 8), ('E2', 8),
    ('A2', 12), ('REST', 4),
]

# Synthesize Voice 1 (Pulse 50%)
v1 = np.zeros(TOTAL_SAMPLES, dtype=np.float32)
cur_sample = 0
for note, dur_16 in MELODY:
    n_samples = dur_16 * SIXTEENTH
    if cur_sample + n_samples > TOTAL_SAMPLES:
        n_samples = TOTAL_SAMPLES - cur_sample
    freq = NOTE_HZ.get(note, 0.0)
    if freq > 0:
        t = np.arange(n_samples) / SAMPLE_RATE
        # Square wave 50% duty
        wave = np.where(np.sin(2 * np.pi * freq * t) >= 0, 1.0, -1.0)
        # ADSR Envelope
        env = np.ones(n_samples, dtype=np.float32)
        att = min(int(SAMPLE_RATE * 0.01), n_samples)
        rel = min(int(SAMPLE_RATE * 0.02), n_samples)
        if att > 0: env[:att] = np.linspace(0.0, 1.0, att)
        if rel > 0: env[-rel:] = np.linspace(1.0, 0.2, rel)
        v1[cur_sample:cur_sample + n_samples] = wave * env * 0.35
    cur_sample += n_samples

# Synthesize Voice 2 (Pulse 25% Arpeggios)
v2 = np.zeros(TOTAL_SAMPLES, dtype=np.float32)
for bar_idx in range(len(ARPEGGIO_CHORDS)):
    chord = ARPEGGIO_CHORDS[bar_idx]
    bar_start = bar_idx * (BEAT_SAMPLES * 2)
    # 8 sixteenth notes in half a bar
    for step in range(8):
        step_start = bar_start + step * SIXTEENTH
        step_samples = SIXTEENTH
        if step_start + step_samples > TOTAL_SAMPLES:
            break
        note = chord[step % len(chord)]
        freq = NOTE_HZ.get(note, 0.0)
        if freq > 0:
            t = np.arange(step_samples) / SAMPLE_RATE
            phase = (freq * t) % 1.0
            wave = np.where(phase < 0.25, 1.0, -1.0)
            env = np.linspace(1.0, 0.3, step_samples)
            v2[step_start:step_start + step_samples] = wave * env * 0.18

# Synthesize Voice 3 (Triangle Bass)
v3 = np.zeros(TOTAL_SAMPLES, dtype=np.float32)
cur_sample = 0
for note, dur_16 in BASS_NOTES:
    n_samples = dur_16 * SIXTEENTH
    if cur_sample + n_samples > TOTAL_SAMPLES:
        n_samples = TOTAL_SAMPLES - cur_sample
    freq = NOTE_HZ.get(note, 0.0)
    if freq > 0:
        t = np.arange(n_samples) / SAMPLE_RATE
        phase = (freq * t) % 1.0
        wave = 2.0 * np.abs(2.0 * phase - 1.0) - 1.0
        v3[cur_sample:cur_sample + n_samples] = wave * 0.30
    cur_sample += n_samples

# Synthesize Voice 4 (Noise Cadence / Taiko March)
v4 = np.zeros(TOTAL_SAMPLES, dtype=np.float32)
np.random.seed(42) # Deterministic retro noise
for beat in range(TOTAL_BEATS):
    beat_start = beat * BEAT_SAMPLES
    # Beats 0 & 2: Kick / Taiko drum (low-pitched rumble)
    # Beats 1 & 3: Snare / Clatter (sharp noise burst)
    is_snare = (beat % 2 == 1)
    dur_samples = int(SAMPLE_RATE * (0.08 if is_snare else 0.12))
    if beat_start + dur_samples <= TOTAL_SAMPLES:
        raw_noise = np.random.uniform(-1.0, 1.0, dur_samples).astype(np.float32)
        if not is_snare:
            # Lowpass filter for taiko kick
            t = np.arange(dur_samples) / SAMPLE_RATE
            sine_body = np.sin(2 * np.pi * 75 * np.exp(-t * 20) * t)
            drum = (raw_noise * 0.4 + sine_body * 0.6) * np.exp(-t * 25)
            v4[beat_start:beat_start + dur_samples] = drum * 0.35
        else:
            t = np.arange(dur_samples) / SAMPLE_RATE
            snare = raw_noise * np.exp(-t * 35)
            v4[beat_start:beat_start + dur_samples] = snare * 0.22

# Mix all 4 voices
mixed = v1 + v2 + v3 + v4
peak = np.max(np.abs(mixed))
if peak > 0:
    mixed = (mixed / peak) * 0.88 # Safe headroom

# 16-bit PCM samples
pcm16 = (mixed * 32767.0).astype(np.int16)

# IMA ADPCM Encoder
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]

def encode_sample(sample, valprev, index):
    step = STEP_TABLE[index]
    diff = sample - valprev
    sign = 8 if diff < 0 else 0
    if sign: diff = -diff
    
    mask = 4
    vpdiff = step >> 3
    code = 0
    for _ in range(3):
        if diff >= step:
            code |= mask
            diff -= step
            vpdiff += step
        step >>= 1
        mask >>= 1
    
    code |= sign
    if sign: valprev -= vpdiff
    else: valprev += vpdiff
    valprev = max(-32768, min(32767, valprev))
    
    index += INDEX_TABLE[code]
    index = max(0, min(88, index))
    return code, valprev, index

adpcm_bytes = bytearray()
valprev = 0
index = 0

for i in range(0, len(pcm16) - 1, 2):
    c0, valprev, index = encode_sample(int(pcm16[i]), valprev, index)
    c1, valprev, index = encode_sample(int(pcm16[i + 1]), valprev, index)
    adpcm_bytes.append((c1 << 4) | (c0 & 0x0F))

out_path = os.path.join(os.path.dirname(__file__), "..", "components", "baye", "assets", "baye_bgm_16k.adpcm")
out_path = os.path.abspath(out_path)
with open(out_path, "wb") as f:
    f.write(adpcm_bytes)

print(f"Successfully generated Baye Strategic BGM: {out_path}")
print(f"ADPCM Bytes: {len(adpcm_bytes)} bytes ({len(adpcm_bytes) / 1024:.1f} KB)")
print(f"PCM Samples: {len(pcm16)} samples")
print(f"Playback duration: {len(pcm16) / SAMPLE_RATE:.2f} seconds")
