#!/usr/bin/env bash
# -*- coding: utf-8 -*-
""":"
exec uv run --with numpy python3 "$0" "$@"
"""
import os
import sys
import math
import struct
import hashlib
import wave
import numpy as np

SAMPLE_RATE = 16000

# Frequencies for notes in Hz
# Chinese Pentatonic scale around A minor: A, C, D, E, G (Gong=C, Shang=D, Jiao=E, Zhi=G, Yu=A)
NOTE_HZ = {
    'REST': 0.0,
    'E2': 82.41,  'G2': 98.00,  'A2': 110.00, 'B2': 123.47, 'C3': 130.81, 'D3': 146.83, 'E3': 164.81,
    'G3': 196.00, 'A3': 220.00, 'B3': 246.94, 'C4': 261.63, 'D4': 293.66, 'E4': 329.63, 'F4': 349.23,
    'G4': 392.00, 'A4': 440.00, 'B4': 493.88, 'C5': 523.25, 'D5': 587.33, 'E5': 659.25, 'G5': 783.99,
    'A5': 880.00, 'B5': 987.77, 'C6': 1046.50, 'D6': 1174.66, 'E6': 1318.51, 'G6': 1567.98, 'A6': 1760.00
}

# --- Standard IMA ADPCM Encoder ---
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

def encode_adpcm(pcm16):
    valprev = 0
    index = 0
    adpcm = bytearray()
    
    # Pad to even length if odd
    if len(pcm16) % 2 != 0:
        pcm16 = np.append(pcm16, pcm16[-1])
        
    for i in range(0, len(pcm16), 2):
        s0 = int(pcm16[i])
        s1 = int(pcm16[i + 1])
        
        # Nibble 0
        step = STEP_TABLE[index]
        diff = s0 - valprev
        sign = 8 if diff < 0 else 0
        if sign: diff = -diff
        mask = 4
        vpdiff = step >> 3
        c0 = 0
        for _ in range(3):
            if diff >= step:
                c0 |= mask
                diff -= step
                vpdiff += step
            step >>= 1
            mask >>= 1
        c0 |= sign
        if sign: valprev -= vpdiff
        else: valprev += vpdiff
        valprev = max(-32768, min(32767, valprev))
        index = max(0, min(88, index + INDEX_TABLE[c0]))
        
        # Nibble 1
        step = STEP_TABLE[index]
        diff = s1 - valprev
        sign = 8 if diff < 0 else 0
        if sign: diff = -diff
        mask = 4
        vpdiff = step >> 3
        c1 = 0
        for _ in range(3):
            if diff >= step:
                c1 |= mask
                diff -= step
                vpdiff += step
            step >>= 1
            mask >>= 1
        c1 |= sign
        if sign: valprev -= vpdiff
        else: valprev += vpdiff
        valprev = max(-32768, min(32767, valprev))
        index = max(0, min(88, index + INDEX_TABLE[c1]))
        
        adpcm.append((c1 << 4) | (c0 & 0x0F))
        
    return bytes(adpcm)

# --- Synthesis Primitives ---

def synth_pulse(freq, duration_s, duty=0.5, attack_s=0.015, decay_s=0.08, sustain=0.7, release_s=0.03, vibrato=False):
    n_samples = int(SAMPLE_RATE * duration_s)
    if n_samples <= 0: return np.zeros(0, dtype=np.float32)
    if freq <= 0: return np.zeros(n_samples, dtype=np.float32)
    
    t = np.arange(n_samples) / SAMPLE_RATE
    if vibrato and duration_s > 0.2:
        vib_delay = 0.1
        vib_t = np.maximum(0, t - vib_delay)
        f_mod = freq + 3.5 * np.sin(2 * np.pi * 5.5 * vib_t) * (vib_t > 0)
        phase = np.cumsum(f_mod / SAMPLE_RATE) % 1.0
    else:
        phase = (freq * t) % 1.0
        
    # Bandlimited-like pulse with anti-aliasing soft transition
    wave = np.where(phase < duty, 1.0, -1.0)
    # Add subtle 2nd harmonic for richer ancient timbre
    wave += 0.25 * np.sin(4 * np.pi * freq * t)
    
    # ADSR Envelope
    att = min(int(SAMPLE_RATE * attack_s), n_samples)
    rel = min(int(SAMPLE_RATE * release_s), n_samples)
    env = np.full(n_samples, sustain, dtype=np.float32)
    if att > 0:
        env[:att] = np.linspace(0.0, 1.0, att)
    dec = min(int(SAMPLE_RATE * decay_s), n_samples - att)
    if dec > 0:
        env[att:att+dec] = np.linspace(1.0, sustain, dec)
    if rel > 0:
        env[-rel:] = np.linspace(env[-rel], 0.0, rel)
        
    return (wave * env).astype(np.float32)

def synth_plucked(freq, duration_s, decay_rate=12.0):
    """Synthesizes a crystalline ancient plucked instrument (Guzheng/Pipa-like)."""
    n_samples = int(SAMPLE_RATE * duration_s)
    if n_samples <= 0: return np.zeros(0, dtype=np.float32)
    if freq <= 0: return np.zeros(n_samples, dtype=np.float32)
    
    t = np.arange(n_samples) / SAMPLE_RATE
    # Fundamental + decaying harmonics
    h1 = np.sin(2 * np.pi * freq * t)
    h2 = 0.55 * np.sin(4 * np.pi * freq * t) * np.exp(-t * (decay_rate * 1.5))
    h3 = 0.25 * np.sin(6 * np.pi * freq * t) * np.exp(-t * (decay_rate * 2.5))
    env = np.exp(-t * decay_rate)
    wave = (h1 + h2 + h3) * env
    return wave.astype(np.float32)

def synth_triangle(freq, duration_s, attack_s=0.01, release_s=0.04):
    n_samples = int(SAMPLE_RATE * duration_s)
    if n_samples <= 0: return np.zeros(0, dtype=np.float32)
    if freq <= 0: return np.zeros(n_samples, dtype=np.float32)
    
    t = np.arange(n_samples) / SAMPLE_RATE
    phase = (freq * t) % 1.0
    wave = 2.0 * np.abs(2.0 * phase - 1.0) - 1.0
    
    att = min(int(SAMPLE_RATE * attack_s), n_samples)
    rel = min(int(SAMPLE_RATE * release_s), n_samples)
    env = np.ones(n_samples, dtype=np.float32)
    if att > 0: env[:att] = np.linspace(0.0, 1.0, att)
    if rel > 0: env[-rel:] = np.linspace(1.0, 0.0, rel)
    return (wave * env).astype(np.float32)

def synth_gong(duration_s=2.5):
    """Ceremonial ancient Three Kingdoms bronze gong."""
    n_samples = int(SAMPLE_RATE * duration_s)
    t = np.arange(n_samples) / SAMPLE_RATE
    # Metallic cluster frequencies
    f1, f2, f3, f4 = 175.0, 248.0, 382.0, 520.0
    metal = (np.sin(2 * np.pi * f1 * t) * 0.4 +
             np.sin(2 * np.pi * f2 * t) * 0.3 +
             np.sin(2 * np.pi * f3 * t) * 0.2 +
             np.sin(2 * np.pi * f4 * t) * 0.1)
    # Slow shimmering modulation
    shimmer = 1.0 + 0.15 * np.sin(2 * np.pi * 4.2 * t)
    env = np.exp(-t * 2.2)
    return (metal * shimmer * env).astype(np.float32)

def synth_taiko(pitch_hz=72.0, decay_rate=18.0):
    duration_s = 0.16
    n_samples = int(SAMPLE_RATE * duration_s)
    t = np.arange(n_samples) / SAMPLE_RATE
    # Pitch drop
    inst_freq = pitch_hz * np.exp(-t * 30.0)
    phase = 2 * np.pi * np.cumsum(inst_freq) / SAMPLE_RATE
    body = np.sin(phase)
    # Initial transient noise
    noise = np.random.uniform(-0.5, 0.5, n_samples) * np.exp(-t * 80.0)
    env = np.exp(-t * decay_rate)
    return ((body * 0.8 + noise * 0.2) * env).astype(np.float32)

def synth_wood_clapper():
    duration_s = 0.06
    n_samples = int(SAMPLE_RATE * duration_s)
    t = np.arange(n_samples) / SAMPLE_RATE
    tone = np.sin(2 * np.pi * 840.0 * t) * 0.6 + np.sin(2 * np.pi * 1420.0 * t) * 0.4
    env = np.exp(-t * 70.0)
    return (tone * env).astype(np.float32)

def synth_snare():
    duration_s = 0.09
    n_samples = int(SAMPLE_RATE * duration_s)
    t = np.arange(n_samples) / SAMPLE_RATE
    noise = np.random.uniform(-1.0, 1.0, n_samples)
    snap = np.sin(2 * np.pi * 210.0 * t) * np.exp(-t * 45.0)
    env = np.exp(-t * 32.0)
    return ((noise * 0.7 + snap * 0.3) * env).astype(np.float32)

# --- Song Builder Utility ---

class SongBuilder:
    def __init__(self, bpm, total_beats):
        self.bpm = bpm
        self.total_beats = total_beats
        self.beat_samples = int(SAMPLE_RATE * 60.0 / bpm)
        self.sixteenth_samples = self.beat_samples // 4
        self.total_samples = total_beats * self.beat_samples
        self.track = np.zeros(self.total_samples, dtype=np.float32)
        
    def add_notes(self, voice_type, note_list, start_beat=0, volume=0.35, **kwargs):
        cur_sample = int(start_beat * self.beat_samples)
        for note, dur_16 in note_list:
            dur_s = (dur_16 * self.sixteenth_samples) / SAMPLE_RATE
            freq = NOTE_HZ.get(note, 0.0)
            if freq > 0:
                if voice_type == 'pulse':
                    wave = synth_pulse(freq, dur_s, **kwargs)
                elif voice_type == 'pluck':
                    wave = synth_plucked(freq, dur_s, **kwargs)
                elif voice_type == 'triangle':
                    wave = synth_triangle(freq, dur_s, **kwargs)
                else:
                    wave = np.zeros(int(dur_s * SAMPLE_RATE), dtype=np.float32)
                end_sample = min(self.total_samples, cur_sample + len(wave))
                add_len = end_sample - cur_sample
                if add_len > 0:
                    self.track[cur_sample:end_sample] += wave[:add_len] * volume
            cur_sample += dur_16 * self.sixteenth_samples
            if cur_sample >= self.total_samples:
                break
                
    def add_percussion_pattern(self, pattern_fn):
        pattern_fn(self)

    def render(self, normalize_peak=0.88):
        peak = np.max(np.abs(self.track))
        if peak > 0:
            norm = (self.track / peak) * normalize_peak
        else:
            norm = self.track
        pcm16 = np.clip(norm * 32767.0, -32768, 32767).astype(np.int16)
        return pcm16

# ==============================================================================
# TRACK 1: TITLE (Opening / Chronicle Epic, 14 Bars @ 104 BPM = ~32.3s)
# ==============================================================================
def compose_title():
    np.random.seed(101)
    BPM = 104
    TOTAL_BARS = 14
    TOTAL_BEATS = TOTAL_BARS * 4
    sb = SongBuilder(BPM, TOTAL_BEATS)
    
    # Voice 1: Stately Heroic Lead (Pulse 50% / Brass timbre)
    lead_notes = [
        # Bars 1-2: Stately Opening Fanfare / Prologue
        ('A3', 8), ('E4', 8), ('A4', 8), ('REST', 8),
        # Bars 3-6: Heroic Theme in A Minor Pentatonic
        ('A4', 4), ('C5', 4), ('D5', 6), ('E5', 2),
        ('G5', 4), ('E5', 4), ('D5', 8),
        ('C5', 4), ('D5', 4), ('E5', 4), ('A4', 4),
        ('G4', 6), ('A4', 2), ('A4', 8),
        # Bars 7-10: Historical Chronicle & Vast Empire (Ascending high register)
        ('E5', 4), ('G5', 4), ('A5', 6), ('G5', 2),
        ('E5', 4), ('D5', 4), ('C5', 4), ('D5', 4),
        ('E5', 4), ('G5', 4), ('E5', 4), ('D5', 4),
        ('C5', 6), ('A4', 2), ('A4', 8),
        # Bars 11-14: Stately Imperial Cadence & Turnaround
        ('D5', 4), ('C5', 4), ('A4', 4), ('G4', 4),
        ('E4', 4), ('G4', 4), ('A4', 8),
        ('A4', 8), ('REST', 8), # Grand cadence
        ('REST', 16) # Seamless breathing space before Bar 1
    ]
    sb.add_notes('pulse', lead_notes, start_beat=0, volume=0.36, duty=0.5, attack_s=0.015, decay_s=0.05, sustain=0.75, vibrato=True)
    
    # Voice 2: Regal Arpeggios (Plucked Pipa texture)
    chords = [
        # B1-2
        ['A3', 'C4', 'E4'], ['A3', 'C4', 'E4'],
        # B3-6
        ['A3', 'C4', 'E4', 'A4'], ['G3', 'B3', 'D4', 'G4'],
        ['C4', 'E4', 'G4', 'C5'], ['A3', 'C4', 'E4', 'A4'],
        # B7-10
        ['C4', 'E4', 'G4', 'C5'], ['D4', 'F4', 'A4', 'D5'],
        ['E4', 'G4', 'B4', 'E5'], ['A3', 'C4', 'E4', 'A4'],
        # B11-14
        ['D4', 'F4', 'A4', 'D5'], ['E4', 'G4', 'B4', 'E5'],
        ['A3', 'C4', 'E4', 'A4'], ['A3', 'C4', 'E4', 'A4']
    ]
    cur_sample = 0
    for bar_idx, chord in enumerate(chords):
        for step in range(16): # 16 sixteenths per bar
            note = chord[step % len(chord)]
            freq = NOTE_HZ[note]
            wave = synth_plucked(freq, 0.25, decay_rate=14.0)
            end_s = min(sb.total_samples, cur_sample + len(wave))
            sb.track[cur_sample:end_s] += wave[:end_s - cur_sample] * 0.16
            cur_sample += sb.sixteenth_samples
            
    # Voice 3: Stately Triangle Bass
    bass_notes = [
        ('A2', 16), ('A2', 16),
        ('A2', 16), ('G2', 16), ('C3', 16), ('A2', 16),
        ('C3', 16), ('D3', 16), ('E3', 16), ('A2', 16),
        ('D3', 16), ('E3', 16), ('A2', 16), ('REST', 16)
    ]
    sb.add_notes('triangle', bass_notes, start_beat=0, volume=0.32)
    
    # Percussion: Ceremonial Gongs + Marching Taiko
    def title_perc(s):
        # Opening gong on Beat 0 and Beat 40
        for b in [0, 40]:
            g = synth_gong(3.5) * 0.38
            pos = b * s.beat_samples
            s.track[pos:pos + len(g)] += g[:s.total_samples - pos]
        # Steady stately pulse
        for b in range(4, s.total_beats - 4):
            pos = b * s.beat_samples
            if b % 2 == 0:
                tk = synth_taiko(70.0, 16.0) * 0.30
                s.track[pos:pos + len(tk)] += tk[:s.total_samples - pos]
            else:
                sn = synth_snare() * 0.18
                s.track[pos:pos + len(sn)] += sn[:s.total_samples - pos]
    sb.add_percussion_pattern(title_perc)
    
    return sb.render()

# ==============================================================================
# TRACK 2: STRATEGY (Contemplative Ancient Court / Farmland, 24 Bars @ 94 BPM = ~61.3s)
# ==============================================================================
def compose_strategy():
    np.random.seed(202)
    BPM = 94
    TOTAL_BARS = 24
    TOTAL_BEATS = TOTAL_BARS * 4
    sb = SongBuilder(BPM, TOTAL_BEATS)
    
    # Theme in D Shang / A Yu pentatonic: Peaceful, durable, spacious
    lead_notes = [
        # Intro (Bars 1-2): Gentle court prelude
        ('D4', 8), ('E4', 8), ('G4', 8), ('A4', 8),
        # Theme A (Bars 3-6): Peaceful Governance
        ('D5', 6), ('C5', 2), ('A4', 4), ('G4', 4),
        ('A4', 6), ('C5', 2), ('D5', 8),
        ('E5', 4), ('D5', 4), ('C5', 4), ('A4', 4),
        ('G4', 6), ('E4', 2), ('D4', 8),
        # Theme A' (Bars 7-10): Harvest & City Expansion
        ('A4', 4), ('C5', 4), ('D5', 4), ('E5', 4),
        ('G5', 6), ('E5', 2), ('D5', 8),
        ('C5', 4), ('A4', 4), ('C5', 4), ('D5', 4),
        ('E5', 8), ('D5', 8),
        # Theme B (Bars 11-14): Strategic Deliberation & Diplomacy
        ('E5', 6), ('G5', 2), ('A5', 4), ('G5', 4),
        ('E5', 4), ('D5', 4), ('C5', 8),
        ('D5', 4), ('E5', 4), ('G4', 4), ('A4', 4),
        ('C5', 6), ('D5', 2), ('D5', 8),
        # Theme B' (Bars 15-18): Map Movement & Seasons Changing
        ('A4', 4), ('D5', 4), ('E5', 6), ('G5', 2),
        ('A5', 4), ('G5', 4), ('E5', 8),
        ('G5', 4), ('E5', 4), ('D5', 4), ('C5', 4),
        ('A4', 6), ('G4', 2), ('A4', 8),
        # Cadence (Bars 19-22): Gentle Resolution
        ('D5', 4), ('C5', 4), ('A4', 6), ('G4', 2),
        ('E4', 4), ('G4', 4), ('A4', 8),
        ('C5', 4), ('A4', 4), ('G4', 4), ('E4', 4),
        ('D4', 12), ('REST', 4),
        # Bars 23-24: Peaceful silence / harp transition back to Bar 1
        ('D4', 8), ('REST', 8),
        ('REST', 16)
    ]
    sb.add_notes('pluck', lead_notes, start_beat=0, volume=0.34, decay_rate=6.5)
    
    # Counterpoint: Soft ancient flute / pulse 25%
    counter_notes = [
        ('REST', 32), # Silent during intro
        ('A3', 16), ('D4', 16), ('C4', 16), ('G3', 16),
        ('D4', 16), ('E4', 16), ('A4', 16), ('D4', 16),
        ('A3', 16), ('G3', 16), ('F3', 16), ('E3', 16),
        ('D3', 16), ('A3', 16), ('G3', 16), ('A3', 16),
        ('D4', 16), ('A3', 16), ('C4', 16), ('D4', 16),
        ('REST', 32)
    ]
    sb.add_notes('pulse', counter_notes, start_beat=0, volume=0.18, duty=0.25, attack_s=0.03, decay_s=0.1, sustain=0.6)
    
    # Voice 3: Warm Triangle Bass
    bass_notes = [
        ('D3', 16), ('D3', 16),
        ('D3', 16), ('A2', 16), ('C3', 16), ('D3', 16),
        ('D3', 16), ('G2', 16), ('A2', 16), ('D3', 16),
        ('C3', 16), ('A2', 16), ('G2', 16), ('D3', 16),
        ('D3', 16), ('E3', 16), ('C3', 16), ('A2', 16),
        ('D3', 16), ('A2', 16), ('C3', 16), ('D3', 16),
        ('D3', 16), ('REST', 16)
    ]
    sb.add_notes('triangle', bass_notes, start_beat=0, volume=0.28)
    
    # Percussion: Light wooden clapper & gentle soft taiko (non-fatiguing)
    def strat_perc(s):
        for b in range(s.total_beats):
            pos = b * s.beat_samples
            if b % 4 == 0:
                # Soft deep pulse
                tk = synth_taiko(60.0, 14.0) * 0.22
                s.track[pos:pos + len(tk)] += tk[:s.total_samples - pos]
            elif b % 2 == 1:
                # Traditional wooden clapper on off-beats
                clp = synth_wood_clapper() * 0.16
                s.track[pos:pos + len(clp)] += clp[:s.total_samples - pos]
    sb.add_percussion_pattern(strat_perc)
    
    return sb.render()

# ==============================================================================
# TRACK 3: BATTLE (Tactical Combat Grid, 20 Bars @ 136 BPM = ~35.3s)
# ==============================================================================
def compose_battle():
    np.random.seed(303)
    BPM = 136
    TOTAL_BARS = 20
    TOTAL_BEATS = TOTAL_BARS * 4
    sb = SongBuilder(BPM, TOTAL_BEATS)
    
    # Aggressive Galloping Lead: Pulse 50%
    lead_notes = [
        # Intro (Bars 1-2): Horn call & war drums
        ('E5', 4), ('E5', 4), ('A5', 8),
        ('E5', 4), ('D5', 4), ('C5', 4), ('A4', 4),
        # Charge 1 (Bars 3-6): Driving battle theme
        ('A4', 2), ('A4', 2), ('C5', 4), ('D5', 4), ('E5', 4),
        ('G5', 4), ('E5', 4), ('D5', 4), ('C5', 4),
        ('D5', 2), ('E5', 2), ('D5', 4), ('C5', 4), ('A4', 4),
        ('G4', 4), ('A4', 4), ('A4', 8),
        # Engagement 2 (Bars 7-10): Intense cavalry clash
        ('E5', 2), ('E5', 2), ('G5', 4), ('A5', 4), ('C6', 4),
        ('A5', 4), ('G5', 4), ('E5', 4), ('D5', 4),
        ('E5', 4), ('G5', 4), ('D5', 4), ('E5', 4),
        ('C5', 4), ('A4', 4), ('A4', 8),
        # Flanking & Siege (Bars 11-14): Syncopated urgency
        ('D5', 2), ('D5', 2), ('D5', 4), ('C5', 4), ('D5', 4),
        ('E5', 2), ('E5', 2), ('E5', 4), ('D5', 4), ('E5', 4),
        ('G5', 4), ('E5', 4), ('D5', 4), ('C5', 4),
        ('A4', 4), ('C5', 4), ('D5', 8),
        # Climactic Charge (Bars 15-18): Fast driving peak
        ('A5', 4), ('G5', 4), ('E5', 4), ('D5', 4),
        ('E5', 2), ('G5', 2), ('E5', 4), ('D5', 4), ('C5', 4),
        ('D5', 4), ('C5', 4), ('A4', 4), ('G4', 4),
        ('A4', 12), ('REST', 4),
        # Turnaround (Bars 19-20): Cadence to loop seamlessly back to Intro
        ('E5', 4), ('D5', 4), ('C5', 4), ('G4', 4),
        ('A4', 8), ('REST', 8)
    ]
    sb.add_notes('pulse', lead_notes, start_beat=0, volume=0.38, duty=0.5, attack_s=0.008, decay_s=0.04, sustain=0.8, vibrato=True)
    
    # Fast 16th-note galloping arpeggio bass/counter
    arp_notes = [
        # Repeated galloping pattern: A3-C4-D4-E4
        ('A3', 2), ('C4', 2), ('D4', 2), ('E4', 2)
    ] * (TOTAL_BEATS * 2) # Every 2 beats
    sb.add_notes('pluck', arp_notes, start_beat=0, volume=0.18, decay_rate=18.0)
    
    # Heavy driving bassline
    bass_notes = [
        ('A2', 4), ('A2', 4), ('A2', 4), ('A2', 4)
    ] * (TOTAL_BARS)
    sb.add_notes('triangle', bass_notes, start_beat=0, volume=0.34)
    
    # Heavy Galloping War Drums (Kick, Snare, Rolls)
    def battle_perc(s):
        for b in range(s.total_beats):
            pos = b * s.beat_samples
            # Gallop rhythm: Kick on 0, 2; Snare on 1, 3
            if b % 2 == 0:
                tk = synth_taiko(75.0, 20.0) * 0.38
                s.track[pos:pos + len(tk)] += tk[:s.total_samples - pos]
            else:
                sn = synth_snare() * 0.28
                s.track[pos:pos + len(sn)] += sn[:s.total_samples - pos]
            # Rapid 16th note rolls on Bar 18-19
            if b >= 72:
                for sub in range(4):
                    r_pos = pos + sub * s.sixteenth_samples
                    sn_roll = synth_snare() * 0.15
                    s.track[r_pos:r_pos + len(sn_roll)] += sn_roll[:s.total_samples - r_pos]
    sb.add_percussion_pattern(battle_perc)
    
    return sb.render()

# ==============================================================================
# TRACK 4: VICTORY (Triumphant Fanfare, 3 Bars @ 120 BPM = ~6.0s, loop=False)
# ==============================================================================
def compose_victory():
    np.random.seed(404)
    BPM = 120
    TOTAL_BARS = 3
    TOTAL_BEATS = TOTAL_BARS * 4
    sb = SongBuilder(BPM, TOTAL_BEATS)
    
    # Bright ascending triumphant fanfare
    fanfare = [
        # Bar 1: Ascending flourish
        ('A4', 2), ('C5', 2), ('D5', 2), ('E5', 2), ('G5', 4), ('A5', 4),
        # Bar 2: Majestic victory declaration
        ('C6', 6), ('A5', 2), ('G5', 4), ('A5', 4),
        # Bar 3: Grand final sustained chord & tail
        ('A5', 12), ('REST', 4)
    ]
    sb.add_notes('pulse', fanfare, start_beat=0, volume=0.42, duty=0.5, attack_s=0.01, decay_s=0.06, sustain=0.85, vibrato=True)
    
    # Harmony counter-pulse (3rd/5th intervals)
    harmony = [
        ('E4', 2), ('G4', 2), ('A4', 2), ('C5', 2), ('D5', 4), ('E5', 4),
        ('G5', 6), ('E5', 2), ('D5', 4), ('E5', 4),
        ('E5', 12), ('REST', 4)
    ]
    sb.add_notes('pulse', harmony, start_beat=0, volume=0.25, duty=0.375)
    
    # Bass resolution
    bass = [
        ('A2', 8), ('E3', 8),
        ('C3', 8), ('G3', 8),
        ('A2', 12), ('REST', 4)
    ]
    sb.add_notes('triangle', bass, start_beat=0, volume=0.35)
    
    # Celebration Drums & Gong
    def victory_perc(s):
        # Grand celebratory gong at onset
        g = synth_gong(3.0) * 0.45
        s.track[0:len(g)] += g[:s.total_samples]
        # Triumphant taiko cadences
        for b in [0, 1, 2, 4, 5, 6, 8]:
            pos = b * s.beat_samples
            tk = synth_taiko(75.0, 18.0) * 0.32
            s.track[pos:pos + len(tk)] += tk[:s.total_samples - pos]
            sn = synth_snare() * 0.22
            s.track[pos + s.sixteenth_samples * 2:pos + s.sixteenth_samples * 2 + len(sn)] += sn[:s.total_samples - (pos + s.sixteenth_samples * 2)]
    sb.add_percussion_pattern(victory_perc)
    
    return sb.render()

# ==============================================================================
# TRACK 5: DEFEAT (Solemn Retreat / Mournful Cadence, 3 Bars @ 80 BPM = ~9.0s, loop=False)
# ==============================================================================
def compose_defeat():
    np.random.seed(505)
    BPM = 80
    TOTAL_BARS = 3
    TOTAL_BEATS = TOTAL_BARS * 4
    sb = SongBuilder(BPM, TOTAL_BEATS)
    
    # Expressive descending minor pentatonic phrase
    mournful_lead = [
        # Bar 1: Descending sigh
        ('E5', 6), ('D5', 2), ('C5', 4), ('A4', 4),
        # Bar 2: Solemn decline
        ('G4', 4), ('E4', 4), ('D4', 6), ('C4', 2),
        # Bar 3: Low tonic rest & quiet fade
        ('A3', 12), ('REST', 4)
    ]
    sb.add_notes('pluck', mournful_lead, start_beat=0, volume=0.38, decay_rate=4.5)
    
    # Low sustained drone (Triangle)
    drone = [
        ('A2', 16),
        ('E2', 16),
        ('A2', 12), ('REST', 4)
    ]
    sb.add_notes('triangle', drone, start_beat=0, volume=0.30)
    
    # Muted solemn percussion
    def defeat_perc(s):
        # Muted gong at Bar 1 and Bar 3
        for b in [0, 8]:
            g = synth_gong(4.0) * 0.25
            pos = b * s.beat_samples
            s.track[pos:pos + len(g)] += g[:s.total_samples - pos]
        # Occasional quiet heartbeat taiko
        for b in [0, 4, 8]:
            pos = b * s.beat_samples
            tk = synth_taiko(55.0, 10.0) * 0.25
            s.track[pos:pos + len(tk)] += tk[:s.total_samples - pos]
    sb.add_percussion_pattern(defeat_perc)
    
    return sb.render()

# ==============================================================================
# Main Generator & Exporter
# ==============================================================================
def export_track(name, pcm16, assets_dir, preview_dir):
    duration_s = len(pcm16) / SAMPLE_RATE
    adpcm = encode_adpcm(pcm16)
    
    # Write ADPCM binary asset
    adpcm_path = os.path.join(assets_dir, f"baye_{name}_16k.adpcm")
    with open(adpcm_path, "wb") as f:
        f.write(adpcm)
        
    # Write WAV preview if preview_dir specified
    if preview_dir:
        wav_path = os.path.join(preview_dir, f"baye_{name}_16k.wav")
        with wave.open(wav_path, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(pcm16.tobytes())
            
    sha256 = hashlib.sha256(adpcm).hexdigest()
    return {
        'name': name,
        'duration_s': duration_s,
        'pcm_samples': len(pcm16),
        'adpcm_bytes': len(adpcm),
        'sha256': sha256,
        'path': adpcm_path
    }

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.abspath(os.path.join(base_dir, ".."))
    assets_dir = os.path.join(project_dir, "components", "baye", "assets")
    preview_dir = os.path.join(project_dir, "build", "music-preview")
    os.makedirs(assets_dir, exist_ok=True)
    os.makedirs(preview_dir, exist_ok=True)
    
    print("==================================================")
    print("  Baye Passport Enhanced — Full Soundtrack Generator")
    print("  Target: 16 kHz Mono IMA ADPCM (4-bit DVI/IMA)")
    print("==================================================")
    
    tracks = [
        ('title', compose_title()),
        ('strategy', compose_strategy()),
        ('battle', compose_battle()),
        ('victory', compose_victory()),
        ('defeat', compose_defeat())
    ]
    
    reports = []
    total_bytes = 0
    total_duration = 0.0
    
    for name, pcm in tracks:
        rep = export_track(name, pcm, assets_dir, preview_dir)
        reports.append(rep)
        total_bytes += rep['adpcm_bytes']
        total_duration += rep['duration_s']
        print(f"[{rep['name'].upper():8s}] {rep['duration_s']:5.2f}s | {rep['pcm_samples']:7d} samples | {rep['adpcm_bytes']:7d} bytes ({rep['adpcm_bytes']/1024:5.1f} KB) | SHA256: {rep['sha256'][:12]}...")
        
    print("--------------------------------------------------")
    print(f"Total Tracks:       5")
    print(f"Total Duration:     {total_duration:.2f} seconds ({total_duration/60:.2f} minutes)")
    print(f"Total Music Assets: {total_bytes} bytes ({total_bytes/1024:.1f} KB / {total_bytes/(1024*1024):.2f} MB)")
    print(f"Assets Directory:   {assets_dir}")
    print(f"WAV Previews:       {preview_dir}")
    print("==================================================")

if __name__ == "__main__":
    main()
