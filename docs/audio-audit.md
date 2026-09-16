# AI Passport Baye: Audio Subsystem Audit & ES8311 Roadmap

**Date:** 2026-09-16  
**Target Hardware:** FoloToy AI Passport (ESP32-C3 revision v1.1, 8MB Flash)  
**Audio Hardware:** Everest Semiconductor ES8311 I2S Codec + NS4150 Class-D PA  
**Status:** V1 Silent Baseline Verified & Future Roadmap Defined

---

## 1. Upstream Audio Architecture & Engine Archeology

### 1.1 Original BBK Hardware Context
The original *Sango / Baye* (三国霸业) was written for the BBK (步步高) electronic dictionary series (BBK 9288, 9388, A-one). These devices possessed:
- **Audio Generation:** A single-frequency 1-bit timer square wave routed to an unamplified piezoelectric ceramic buzzer.
- **Audio Capabilities:** No DAC, no PCM sound channels, no digital synthesizer, and no wave-table synthesis.
- **Audio Assets in ROM:** Analysis of `dat.lib` (196,890 bytes) reveals 100% of the asset entries are bitmaps, maps, city configurations, general/officer statistics, event strings, and logic matrices. There are **zero** sound samples, audio waveforms, or music tracks anywhere in `dat.lib`.

### 1.2 Call Sites in Baye Engine Core
A complete symbol audit of the Baye engine codebase reveals only two active audio-related call sites:
1. `comIn.c: GamEngineInit()`:
   ```c
   g_GamKeySound = SysGetKeySound(); /* Back up dictionary OS key click state */
   SysSetKeySound(false);           /* Disable key click beeps during gameplay */
   ```
2. `comIn.c: GamConRst()`:
   ```c
   SysSetKeySound(g_GamKeySound);   /* Restore dictionary OS key click state on exit */
   ```
Other headers in `dictsys.h` (e.g. `SysPlayMelody`, `SysStopMelody`, `SysSetVolume`) were dictionary OS shell functions and are **never called** by any module of the Baye game engine.

**Conclusion:** The native Baye game experience is historically and functionally **completely silent**.

---

## 2. Platform Layer Implementation (`passport_sys.c`)

In `components/baye/platform/passport_sys.c`:
```c
FAR U8 SysGetKeySound(void) {
    return 0; // Key click sound permanently off
}

FAR void SysSetKeySound(U8 keySoundFlag) {
    (void)keySoundFlag; // Safe no-op
}
```

### 2.1 Why Silent Baseline is Strictly Preserved for V1
1. **Resource Preservation:** The ESP32-C3 is a single-core 160MHz RISC-V microcontroller. Running the game task, SPI LCD DMA transfers, and timer ticks in a single-core environment with ~146 KB free heap is stable and jitter-free. Initializing an audio I2S driver + codec background task would consume unnecessary internal DMA buffers and heap.
2. **Authenticity:** The original handheld experience was silent except for physical key clicks.
3. **Power Consumption:** The FoloToy AI Passport is a battery-powered device. Keeping the NS4150 Class-D PA and ES8311 codec unpowered/muted saves substantial quiescent current.

---

## 3. Future ES8311 Minimal Sound Roadmap (V2 Non-Breaking Proposal)

Should key-click beeps or vintage buzzer tones be desired in a future phase, the following minimal, zero-interference architecture MUST be followed:

### 3.1 Hardware Map on FoloToy AI Passport
- **I2C Control:** ES8311 registers configured over I2C (address `0x18`).
- **I2S Audio Stream:** Standard I2S master transmitter (`BCLK`, `WS`, `DOUT`).
- **PA Enable:** NS4150 PA enable GPIO (active high).

### 3.2 Constraints & Design Rules
1. **Zero Heap / DMA Bloat:** Do NOT allocate large circular PCM buffers or audio pipeline frameworks (e.g. `esp_audio`, ADF).
2. **Precomputed ROM Wavetable:** A static 64-sample 1 KHz sine or square wave in `.rodata` (~128 bytes of flash).
3. **One-Shot Non-Blocking DMA:**
   - On key press or combat action: pulse NS4150 PA high, transmit 500 samples (~10 ms duration at 44.1 kHz), pull PA low.
   - Zero background task: do NOT create an audio FreeRTOS task. Use DMA completion ISR or simple timer trigger.
4. **Compile-Time Feature Flag:** Guard all audio code with `#ifdef CONFIG_BAYE_ENABLE_AUDIO`. When disabled, compile footprint is 0 bytes.
