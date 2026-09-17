# Baye Passport Enhanced: Background Music (BGM) Specification & Soundtrack Architecture

**Document Version:** 2.0  
**Phase:** Baye Passport Enhanced — Full Music Integration  
**Target Hardware:** FoloToy AI Passport (ESP32-C3 revision v1.1, 160MHz RISC-V, 8MB SPI Flash, No PSRAM)  
**Audio Hardware:** Everest Semiconductor ES8311 I2S Codec + NS4150 Class-D Audio PA  

---

## 1. Feature Definition & Architectural Scope

> **Historical & Archeological Clarification:**  
> The background music playback system is an **Enhanced Platform Feature** (`Baye Passport Enhanced`) added exclusively for the FoloToy AI Passport.  
> As established by source code and binary archeology, the BBK platform provided audio, melody, volume, and speech-capable system facilities. However, no background-music playback path has been identified in the Baye game core currently available to us (the Baye source code does not call `SysPlayMelody`, `SysStopMelody`, or `SysSetVolume`, and explicitly disables the system key sound during gameplay).  
> Therefore, this soundtrack is an **Original Soundtrack (OST)** composed and synthesized specifically for Baye Passport Enhanced, rather than a restoration of an existing historical game core soundtrack.

When disabled via `CONFIG_BAYE_ENHANCED_MUSIC=0`, zero background audio tasks are created, zero audio buffers are allocated, and the firmware reverts completely to the silent V1 baseline.

---

## 2. Complete 5-Track Original Soundtrack (OST) Catalog

All tracks are synthesized at 16,000 Hz, 16-bit Mono, and compressed using 4-bit standard IMA ADPCM (DVI/IMA table):

| Track ID | Track Name | Musical Theme & Motif | Duration | Samples | ADPCM Size | Loop Mode | In-Game Trigger Point | SHA-256 Checksum (ADPCM) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `1` | `TITLE` | Majestic Imperial Palace Theme (Gong mode / 宮调式) | 32.30s | 516,880 | 258,440 B (252.4 KB) | Looping | Boot, Title Movie, Main Menu | `26dedcba4f810149bb892c90c7414df8bb36b9576fe9fce73eb8b22e13292415` |
| `2` | `STRATEGY` | Broad Strategic Governance & Realm (Zhi mode / 徵调式) | 61.27s | 980,352 | 490,176 B (478.7 KB) | Looping | World Map & Strategy Loop | `ec524e2f25f1712a843588daaf0bc8e38d781b4d0bc78d8a7c1dd30a7d57221d` |
| `3` | `BATTLE` | Intense Tactical March & Skirmish (Yu mode / 羽调式) | 35.29s | 564,640 | 282,320 B (275.7 KB) | Looping | Manual Tactical Combat Map | `8795aaba352055615ee5e76a60e03e588d923ee6d2fe2b947c6a992fc07cfc7d` |
| `4` | `VICTORY` | Triumphant Fanfare & Celebration (Shang mode / 商调式) | 6.00s | 96,000 | 48,000 B (46.9 KB) | One-Shot | Battle Won (`FGT_WON`) | `bdebd4e2439775080e7d5a5cf2b012ebfe95fa809ef41b18ca7ba8648356396f` |
| `5` | `DEFEAT` | Somber Retreat & Regrouping (Yu Minor / 羽调式) | 9.00s | 144,000 | 72,000 B (70.3 KB) | One-Shot | Battle Lost (`FGT_LOSE`) | `532fd8a2769ee38b05da39542a2754668f49eb2a0aa15c71c480112ddad84c1f` |

- **Total Music Duration:** 143.87 seconds
- **Total Compressed Storage:** 1,150,936 bytes (1.10 MB)
- **Flash Mapping:** Directly embedded into `.rodata` segment via CMake `EMBED_FILES` and memory-mapped at runtime. Consumes **0 SRAM bytes** for track storage.
- **Generator:** `tools/generate_baye_music_pack.py` (pure algorithmic synthesis using NumPy; zero external audio assets or proprietary sound libraries; deterministic seed for exact reproducibility).
- **Preview Files:** High-quality preview WAV files generated into `build/music-preview/`.

---

## 3. Music Manager Architecture & State Machine

### 3.1 Task & Decoupling Model

The playback architecture decouples the game engine and input handling from audio decoding:
- **Audio Worker Task (`baye_audio`):** Priority 5, 2560 bytes stack.
- **Command Queue (`s_cmd_queue`):** FreeRTOS queue (`xQueueCreate(4, sizeof(audio_cmd_t))`), supporting non-blocking IPC commands.
- **Single Decoder State:** Only one decoder state (`passport_adpcm_state_t`) and one PCM chunk buffer (`int16_t s_pcm_buf[320]`, 640 bytes) exist. Multiple tracks never decode simultaneously.

### 3.2 Transient Fade & Cross-Scene Switching

- **Q15 Fixed-Point Gain Scaling:**
  $$\text{PCM}_{\text{out}}[i] = \frac{\text{PCM}_{\text{raw}}[i] \times \text{gain}}{32768}$$
  - Operates purely in software without touching the ES8311 Master Volume or NVS storage.
  - Linear sample-by-sample ramp prevents audio pops and clicks.
- **Deduplication:** Calling `passport_audio_play(track)` with the currently playing track is a zero-latency no-op (no restart, no hitching).
- **One-Shot Auto-Resume:** Jingles (`VICTORY`, `DEFEAT`) play once to completion (`samples == 0`). Upon reaching EOF, the worker automatically reconfigures the stream for the specified `resume_track` (`STRATEGY`), resets the ADPCM decoder state, and fades smoothly in.

---

## 4. Baye Core Integration Hooks

1. **Title & Strategy Lifecycle (`components/baye/core/gamEng.c`):**
   - Hook in `GamBaYeEng()` before `GamMovie(MAIN_SPE)` starts `BAYE_MUSIC_TITLE`.
   - Main loop iteration ensures returning from gameplay immediately cross-fades to `BAYE_MUSIC_TITLE`.
   - Exiting `GamMainChose()` into `GameDevDrv()` triggers `passport_audio_play(BAYE_MUSIC_STRATEGY)`.
2. **Combat Lifecycle (`components/baye/core/Fight.c`):**
   - Entering manual battle inside `GamFight()` triggers `passport_audio_play(BAYE_MUSIC_BATTLE)`.
   - Exiting `GamFight()` checks combat outcome:
     - `g_FgtOver == FGT_WON`: `passport_audio_play_once(BAYE_MUSIC_VICTORY, BAYE_MUSIC_STRATEGY)`
     - `g_FgtOver == FGT_LOSE`: `passport_audio_play_once(BAYE_MUSIC_DEFEAT, BAYE_MUSIC_STRATEGY)`
     - Other (retreat/disengage): `passport_audio_play(BAYE_MUSIC_STRATEGY)`
   - Auto-combat simulations (`FGT_AUTO`) bypass tactical audio switching, maintaining strategy music uninterrupted.

---

## 5. Volume Controls, HUD & NVS Safety

- **Controls:**
  - `UP DOUBLE`: $+10\%$ volume (clamped at $100\%$)
  - `DOWN DOUBLE`: $-10\%$ volume (clamped at $0\%$)
  - Serial console: `]` ($+10\%$), `[` ($-10\%$), `0` (stop), `1`..`5` (direct audition).
- **HUD Indicator:** Top letterbox ($X=6, Y=7, 48\times 10\text{px}$), displaying `VOL 0` to `VOL 100` alongside battery percentage ($X=266$). Renders synchronously on display flush.
- **NVS Data Isolation:**
  - Namespace: `baye_cfg`
  - Key: `volume` (`u8`)
  - Fail-safe runtime: Never invokes `nvs_flash_erase()`. Volume save failures log warnings and retain in-RAM volume without affecting save files (`baye_sav`) or system partitions.

---

## 6. Hardware Verification & Performance Metrics

Verified on physical FoloToy AI Passport hardware (ESP32-C3 revision v1.1):

| Metric | Target | Actual Measured | Status |
| :--- | :--- | :--- | :--- |
| **Audio Underruns** | 0 | 0 | PASS |
| **Display DMA Timeouts** | 0 | 0 | PASS |
| **LCD Submit Fails** | 0 | 0 | PASS |
| **Battle RAM Gate** | Contiguous 64 KB SRAM | Intact (Allocated at `0x3fcc036c`) | PASS |
| **Free Heap (Gameplay)**| $> 100\text{ KB}$ | 126,820 bytes (123.8 KB) | PASS |
| **Min Free Heap** | $> 80\text{ KB}$ | 126,748 bytes (123.7 KB) | PASS |
| **Display FPS** | $> 25\text{ fps}$ | 29.5 fps | PASS |
| **App Partition Usage** | $< 3.0\text{ MB}$ (3,145,728 B) | 1,980,928 bytes (62.9%) | PASS |
| **Cold Boot Volume Restore** | $70\%$ from NVS | $70\%$ restored accurately | PASS |
