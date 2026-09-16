# Baye Passport Enhanced: Background Music (BGM) Specification

**Document Version:** 1.0  
**Phase:** Baye Passport Enhanced — Music Edition  
**Target Hardware:** FoloToy AI Passport (ESP32-C3 revision v1.1, 160MHz RISC-V, 8MB SPI Flash, No PSRAM)  
**Audio Hardware:** Everest Semiconductor ES8311 I2S Codec + NS4150 Class-D Audio PA  

---

## 1. Feature Definition & Architectural Scope

> **Important Clarification:**  
> The background music playback system is an **Enhanced Platform Feature** (`Baye Passport Enhanced`) added exclusively for the FoloToy AI Passport.  
> As proven by binary and source archeology of `dat.lib` and the original BBK C core, the original *Sango / Baye* (三国霸业) on BBK electronic dictionaries was 100% silent (except for piezoelectric 1-bit timer key clicks). This music capability is **not** a recreation of any original game soundtrack, but a newly engineered retro enhancement designed to operate harmoniously alongside the native C game engine.

When disabled via `CONFIG_BAYE_ENHANCED_MUSIC=0`, zero background audio tasks are created, zero audio buffers are allocated, and the firmware reverts completely to the silent V1 baseline.

---

## 2. Audio Asset & Copyright Boundary

| Attribute | Specification / Value |
| :--- | :--- |
| **Asset Name** | `baye_bgm_16k.adpcm` |
| **Asset Location** | `components/baye/assets/baye_bgm_16k.adpcm` |
| **Asset Generator** | `tools/generate_baye_bgm.py` |
| **Musical Genre** | 8-bit / 16-bit Retro Chiptune Military Strategic March |
| **Musical Scale** | Chinese Ancient Pentatonic (羽调式 / A Minor Pentatonic: A, C, D, E, G) |
| **Voices / Channels** | 4 synthesized voices (Pulse 1 50% Lead, Pulse 2 25% Arpeggios, Triangle Bass, Noise March Cadence) |
| **Copyright Status** | **100% Original Algorithmic Composition & Synthesis (Public Domain / CC0 equivalent)** |
| **Distribution Status** | Freely redistributable and embeddable in open-source firmware. Zero commercial sample encumbrance. |
| **Original Format** | 16-bit signed PCM synthesized at 16,000 Hz |
| **Device Encoded Format** | **IMA ADPCM (4-bit DVI/IMA standard nibbles)** |
| **Sample Rate** | 16,000 Hz |
| **Bit Depth** | 16-bit signed decoded PCM |
| **Audio Channels** | 1 (Mono) |
| **Bitrate** | **64 kbps** (8,000 bytes/sec) |
| **Track Duration** | **23.07 seconds** (10 bars, 40 beats at 104 BPM) |
| **Compressed Size** | **184,600 bytes (180.3 KB)** |
| **Flash Storage** | Directly embedded in Flash `.rodata` via MMU cache mapping (0 SRAM bytes consumed) |

---

## 3. Playback Pipeline & RAM Allocation

The audio engine streams directly from Flash memory in small 20-millisecond chunks, decoding on the fly into a tiny PCM buffer, and pushing into the ESP-IDF I2S DMA queue:

```text
Flash Memory (0x3c05fc88..0x3c08cda0, 180.3 KB)
       │
       ▼ (160 bytes ADPCM chunk per 20ms)
Audio Worker Task (`baye_audio`, Priority 5)
       │
       ▼ (IMA ADPCM Decode in passport_adpcm.c)
Decoded PCM Buffer (320 samples × 16-bit = 640 bytes)
       │
       ▼ (bsp_audio_write -> I2S DMA)
I2S DMA Transmit Queue (8 descriptors × 320 frames = 160 ms headroom)
       │
       ▼
Everest ES8311 Codec ──> NS4150 Class-D PA ──> Speaker
```

### Exact Memory Footprint Breakdown

| Memory Item | Allocation Size | Lifetime / Location |
| :--- | :--- | :--- |
| **Compressed Input Buffer** | 160 bytes | Audio task stack |
| **ADPCM Decoder State** | 4 bytes (`int16 valprev`, `int8 index`) | Static `.bss` |
| **Decoded PCM Chunk Buffer**| 640 bytes (320 int16 samples) | Static `.bss` (`s_pcm_buf`) |
| **Audio Worker Task Stack** | 2,560 bytes (2.5 KiB) | FreeRTOS internal heap |
| **I2S DMA Descriptors & Ring**| $\sim 5,120$ bytes | Internal DMA-capable SRAM |
| **Total Audio SRAM Overhead**| **$< 8.5\text{ KB}$** | Safe within $> 140\text{ KB}$ free SRAM headroom |

---

## 4. Loop Behavior & Timing

- **Boundary Transition:** At the end of byte 184,600, the stream cursor cleanly wraps around to offset 0 and explicitly resets the ADPCM decoder state (`passport_adpcm_state_reset`) to ensure cycle-to-cycle mathematical determinism:
  ```c
  if (stream->offset >= stream->size) {
      if (stream->loop) {
          stream->offset = 0;
          stream->loop_count++;
          passport_adpcm_state_reset(state);
      }
  }
  ```
- **Loop Boundary Gap:** **$< 1\text{ ms}$ (near zero / seamless)**. Because the last measure resolves cleanly on the tonic $A4$ followed by a matched rest, and decoder state is reset, the loop transition exhibits zero DC pop, clicking, or cumulative sample drift.
- **Loop Telemetry:** Each loop increment is captured by `passport_audio_worker` with timestamp and logged to the console:
  ```text
  I (...) baye_audio: BGM Loop #1 completed (cycle duration: 23072 ms, seamless rewind)
  ```

---

## 5. Volume Control UX & NVS Persistence

### 5.1 Control Inputs
- **Hardware Buttons (ADC Ladder):**
  - `UP DOUBLE`: Volume $+10\%$ (clamped to max $100\%$)
  - `DOWN DOUBLE`: Volume $-10\%$ (clamped to min $0\%$, pure mute)
  - Existing `CLICK` and `LONG` key gestures remain 100% unaltered.
- **Serial Console Shortcuts:**
  - `]`: Volume $+10\%$
  - `[`: Volume $-10\%$

### 5.2 Top Letterbox Volume HUD
- **Location:** Top-left letterbox ($X=6..53, Y=7..16$, dimension $48 \times 10$ pixels, RGB565).
- **Presentation:** High-contrast retro white text on black background (`VOL 0` .. `VOL 100`) mirroring the battery indicator in the top-right letterbox ($X=266..313, Y=7..16$).
- **Lifecycle & DMA Safety:** Renders exclusively within the display pipeline via non-blocking DMA. Only flushes when the volume state changes (`dirty` flag).

### 5.3 NVS Persistence Specification
- **Namespace:** `baye_cfg`
- **Key:** `volume` (`uint8_t`, range $0..100$)
- **Behavior:** Only written upon deliberate volume adjustments. Restored on system boot; falls back to `CONFIG_BAYE_MUSIC_VOLUME` (default 70%) if unset.
- **Partition Protection:** Operates in separate namespace `baye_cfg`; strictly isolates and preserves all game saves (`sango*` in `baye_sav`), factory `cardid`, and `recovery` partitions.

---

## 6. Coexistence with LCD DMA & Game Loop

1. **Scheduling Isolation:**
   The Baye C engine executes inside `baye_game` (Priority 5, 16 KiB stack).  
   The Audio worker executes inside `baye_audio` (Priority 5, 2.5 KiB stack).  
   FreeRTOS round-robin time slicing and natural I2S DMA blocking ensure the audio worker only wakes up every 20 ms for $\sim 50\ \mu\text{s}$ to decode 160 bytes.
2. **Display Stability Verification:**
   The ST7789 SPI LCD flush pipeline retains its fail-closed DMA completion wait (`bsp_display_wait_trans_done`).  
   Target metrics remain:
   - `DMA Timeout Count = 0`
   - `LCD Submit Fail Count = 0`

