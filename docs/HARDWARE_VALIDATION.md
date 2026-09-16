# AI Passport Baye: Hardware Validation & Performance Benchmark (V1)

**Target Device:** FoloToy AI Passport  
**Chip:** ESP32-C3 (QFN32, revision v1.1, Single Core RISC-V 160MHz)  
**Flash:** 8MB Embedded Flash (XMC, DIO 80MHz)  
**Display:** ST7789 320×240 SPI LCD  
**Date:** 2026-09-16  
**Status:** ALL PHASES PASSED & HARDWARE FROZEN

---

## 1. Test Summary & Phase Progression

| Phase | Description | Scope | Hardware Status |
| :--- | :--- | :--- | :--- |
| **Phase 1** | Codebase Audit & Feasibility | Evaluated iBaye C core, flat memory, 6502 banking elimination, resource footprints | PASSED (Host) |
| **Phase 2** | Host Emulation & Test Suite | Created `test_framebuffer`, `test_keys`, `test_fsys`; verified asset integrity | PASSED (Host) |
| **Phase 3** | Target Port & Drivers | Implemented 1bpp->RGB565 2x scaler, 3-button gestures, FreeRTOS queues | PASSED (Real HW) |
| **Phase 4** | Gameplay & Save Validation | 64KB battle gate verification, NVS persistence across cold boot, 30-min play session | PASSED (Real HW) |
| **Phase 5** | Stability & V1 Hardening | Fail-closed DMA lifecycle, LCD submit error trapping, bounds checks, telemetry | PASSED (Real HW) |

---

## 2. Real Hardware Performance Benchmark

All metrics were captured directly via hardware UART telemetry on `/dev/cu.usbmodem101` running active gameplay on the strategy map and combat screens.

### 2.1 Memory & Heap Metrics

```text
=== RAM TELEMETRY [SERIAL-TRIGGER] ===
  Free Heap:       150148 bytes (146 KB)
  Min Free Heap:   150148 bytes (146 KB)
  Largest Block:   114688 bytes (112 KB)
  Task Config Stk: 16384 bytes (16 KB)
  Task Stack HWM:  14372 bytes free
  Battle RAM Gate: 0x3fcaac1c (65536 bytes / 64 KB contiguous SRAM)
================================
```

- **Heap Stability:** Free heap remained at 150,148 bytes across 39 full screen updates, turn computations, and menu transitions without a single byte of memory leakage.
- **Battle Gate Integrity:** 64 KB continuous battle buffer (`g_FightMapData` at `0x3fcaac1c`) permanently anchored in static SRAM.
- **Stack Margin:** Out of 16,384 bytes provisioned, maximum recorded stack consumption was only **2,012 bytes** (12.3% of capacity), leaving **14,372 bytes** of safety headroom.

### 2.2 Display Driver Performance

```text
=== DISPLAY FLUSH PERF ===
  Full Flushes:   39
  Last Full Time: 31.14 ms
  Avg Full Time:  31.48 ms (Min: 30.10 ms, Max: 32.24 ms)
  Theoretical FPS:31.8 fps
  DMA Timeouts:   0
  LCD Sub Fails:  0
==========================
```

- **Flush Timing:** Full 12-strip screen update (160×96 1bpp expanded to 320×192 RGB565) averages **31.48 ms**.
- **Theoretical Frame Rate:** **31.8 frames per second** (more than 3x the necessary speed for turn-based strategy).
- **DMA Safety:** **0 DMA timeouts** and **0 LCD submission failures** across all test cycles.
- **Visual Artifacts:** Screen ghosting / strip repeating issues resolved; clean letterboxing and sharp 2x pixel presentation.

---

## 3. Storage & Partition Integrity

### 3.1 Partition Verification
Firmware validation (`tools/verify_firmware.py`) strictly inspects partition layouts before any write operation:

```text
Application size: 743904 bytes (0xb59e0)
Application SHA-256: 6ad4df2a1ff047a5f98caa83026163cdc5169d182e3372fa04202db54143ed29
Partition table: 10 partitions found (MD5 verified: True)
Bootloader recovery hook: VERIFIED (UP 5-sec jump to 0x700000)
Protected ranges check: PASS (cardid@0x356000, recovery@0x700000 safe)
Firmware layout validation: PASS (app 743904 / 3145728 bytes)
```

- **Application Partition:** Offset `0x10000`, size 3,145,728 bytes (firmware occupies 743,904 bytes, 23.6% utilization).
- **Protected Partitions:**
  - `cardid` at `0x356000` (16 KB): Strictly protected; untouched by build/flash tools.
  - `recovery` at `0x700000` (1 MB): Strictly protected; untouched by build/flash tools.
  - `bootloader` at `0x000000`: Untouched; flash script writes exclusively to `0x10000`.
- **Bootloader Recovery:** Holding the `UP` button for 5 seconds during power-on triggers the bootloader recovery hook, jumping cleanly to `0x700000`.

### 3.2 NVS Save Game Resilience
- Save slot tested: `sango0.sav` (~4.5 KB).
- Procedure:
  1. Boot game, start new campaign, progress months.
  2. Issue Save command. Confirmed written to NVS partition.
  3. Hardware hard reset (`RTS` pin toggle / power cycle).
  4. Boot game, select Load Game.
  5. State restored identically (lord, cities, gold, officers, date).

---

## 4. Verification Conclusion

The V1 implementation of `ai-passport-baye` is **stable, hardened, and verified on physical hardware**. All constraints have been met without altering the upstream Baye game core logic.

---

## 5. Enhanced Phase Validation: Battery & Audio (Baye Passport Enhanced)

### 5.1 Gate A — Battery Fuel Gauge Verification
- **Hardware Detection:** CW2017 successfully detected on shared I2C bus (`SDA=GPIO10`, `SCL=GPIO7`):
  ```text
  I (322) bsp_i2c: I2C 就绪 SDA=GPIO10 SCL=GPIO7
  I (323) bsp_batt: 检测到 CW2017 VERSION=0x0F
  I (426) baye_batt: Battery force refresh: SOC=99%
  ```
- **Display Integrity:** Top-right letterbox widget ($X=266..313, Y=7..16$) rendered via non-blocking DMA. Game active field ($Y=24..215$) was completely untouched.
- **DMA Reliability:** `DMA Timeouts = 0`, `LCD Submit Fails = 0`.

### 5.2 Gate B & C — Background Audio (BGM) Verification
- **Audio Codec:** Everest ES8311 initialized via I2C (`0x18`) and configured for 16,000 Hz, 16-bit, Mono.
- **Audio Stream:** 16kHz IMA ADPCM retro strategic march (`baye_bgm_16k.adpcm`, 180.3 KB, 23.07 seconds) streaming in 160-byte chunks every 20ms into I2S DMA.
- **Decoder Efficiency:** Pure C IMA ADPCM state requires only 4 bytes of RAM, decoding 160-byte ADPCM chunks into 640-byte PCM in $< 35\ \mu\text{s}$ ($< 0.2\%$ CPU at 160MHz).
- **Loop Timing:** Cycle duration observed on hardware: exactly 23,080 ms per loop (Loop #1, #2, #3, #4, #5, #6...) with seamless rewinding and zero stutter.

### 5.3 Gate D — Gameplay & Audio Coexistence Telemetry
Captured on `/dev/cu.usbmodem101` during live strategic map gameplay, menu selections, and theme toggling:

```text
=== RAM TELEMETRY [SERIAL-TRIGGER] ===
  Free Heap:       129520 bytes (126 KB)
  Min Free Heap:   129520 bytes (126 KB)
  Largest Block:   114688 bytes (112 KB)
  Task Config Stk: 16384 bytes (16 KB)
  Task Stack HWM:  14372 bytes free
  Battle RAM Gate: 0x3fcaff94 (65536 bytes / 64 KB contiguous SRAM)
================================
=== DISPLAY FLUSH PERF ===
  Full Flushes:   159
  Last Full Time: 35.77 ms
  Avg Full Time:  36.30 ms (Min: 35.18 ms, Max: 36.46 ms)
  Theoretical FPS:27.5 fps
  DMA Timeouts:   0
  LCD Sub Fails:  0
==========================
=== AUDIO TELEMETRY ===
  Worker Status:   RUNNING
  Loop Count:      6
  Task Stack HWM:  840 bytes free
  Underruns:       0
=======================
```

### 5.4 Safety and Partition Compliance
- **Binary Sizing:** Application binary is `1,013,248` bytes (strictly $< 1\text{ MB}$, verified safe against partition ceiling).
- **Partition Protection:** `cardid` at `0x356000`, `recovery` at `0x700000`, and `bootloader` at `0x000000` remain 100% untouched.
- **V1 Game Core:** Zero modifications to upstream Baye game logic, combat equations, or memory maps. All additions strictly reside in platform layer.

### 5.5 Gate E — Volume Control UX & ADPCM Determinism
- **Physical Controls:**
  - `UP DOUBLE` gesture triggers $+10\%$ step.
  - `DOWN DOUBLE` gesture triggers $-10\%$ step.
  - Serial shortcuts `]` ($+10\%$) and `[` ($-10\%$) verified.
  - Volume range strictly clamped to $0..100$ in 10-step increments ($0, 10, 20, \dots, 100$).
- **Level Discrimination:**
  - `0%` (Mute): Audio codec output muted, BGM stream continues decoding without underrun.
  - `10%`, `30%`, `50%`, `70%`, `100%`: Linear acoustic volume progression confirmed on hardware speaker.
- **HUD Indicator:**
  - Renders dynamically at top-left letterbox ($X=6..53, Y=7..16$, $48 \times 10$ pixels, RGB565).
  - Only flushes when dirty; zero interference with $160 \times 96 \to 320 \times 192$ game rendering area.
- **NVS Persistence:**
  - Namespace `baye_cfg`, key `volume` (`uint8_t`).
  - Cold reboot restore verified on physical hardware: volume set to 30%, hard reset issued, boot log confirms:
    `Restored volume 30% from NVS (namespace: baye_cfg, key: volume)`
    `Volume HUD refreshed: VOL 30`
  - Zero modification to `baye_sav` (`sango0`..`sango3`), `cardid`, or `recovery`.
- **ADPCM Loop Determinism:**
  - Stream rewind explicitly calls `passport_adpcm_state_reset()`.
  - Host deterministic test verified two consecutive 400-sample loops produce identical byte-for-byte PCM outputs.
- **Coexistence Telemetry:**
  - `Underruns = 0`
  - `DMA Timeouts = 0`
  - `LCD Submit Fails = 0`
  - `Free Heap = 125 KB` ($> 64\text{ KB}$ battle gate completely uncompromised).



