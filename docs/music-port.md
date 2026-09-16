# AI Passport Baye: Audio Subsystem BSP Audit & Music Porting Guide

**Target Hardware:** FoloToy AI Passport (ESP32-C3 revision v1.1, 8MB SPI Flash)  
**Audio Codec:** Everest Semiconductor ES8311 (I2C control + I2S data)  
**Power Amplifier:** NS4150 Class-D Audio PA  
**Reference Source:** Verified hardware baseline from `ai-passport-nt-d`  

---

## 1. Hardware Pinout & Bus Mapping (Single Source of Truth)

The hardware pin assignments are verified from the AI Passport board schematics and confirmed operational in `ai-passport-nt-d`:

| Signal | GPIO | Function | Description |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | `GPIO10` | I2C Data | Shared with CW2017 battery gauge |
| **I2C SCL** | `GPIO7` | I2C Clock | 100 kHz / 400 kHz standard mode |
| **I2S MCLK** | `GPIO6` | Master Clock | Master reference clock to ES8311 ($256 \times f_s$) |
| **I2S BCLK** | `GPIO5` | Bit Clock | Serial audio bit clock |
| **I2S WS / LRCK** | `GPIO3` | Word Select | Left/Right frame sync clock |
| **I2S DOUT** | `GPIO2` | Data Out | MCU $\rightarrow$ ES8311 DAC playback |
| **I2S DIN** | `GPIO4` | Data In | ES8311 ADC $\rightarrow$ MCU microphone |
| **PA CTRL** | `-1` | PA Enable | Hardware strapped active (no MCU GPIO control needed) |

ES8311 7-bit I2C address: `0x18` (8-bit write address: `0x30`).  
CW2017 7-bit I2C address: `0x63`.

---

## 2. Audio Subsystem Architecture

### 2.1 Codec Initialization Pipeline (`bsp_audio.c`)
1. **I2C Control Bus**:
   ```c
   const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&(audio_codec_i2c_cfg_t){
       .port = BSP_I2C_PORT,
       .addr = BSP_I2C_ES8311_ADDR << 1,
       .bus_handle = bsp_i2c_bus(),
   });
   ```
2. **I2S Channel Setup**:
   - Master mode on `I2S_NUM_0`.
   - `dma_desc_num = 8`, `dma_frame_num = 320`.
   - TX buffer headroom: $\sim 160\text{ ms}$, ensuring zero underruns during SPI LCD DMA submissions and Baye AI turn processing.
3. **ES8311 Device Setup**:
   - `es8311_codec_new()` with `codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH`, `master_mode = false` (MCU is I2S master), `use_mclk = true`.
   - Formatted via `bsp_audio_set_format(sample_rate_hz, 16, 1)`.
   - Note: Never manually overwrite ES8311 clock divider registers (REG01..REG06); `esp_codec_dev_open()` calculates exact PLL/divider configurations.

### 2.2 Volume Path
- Digital output volume is controlled via:
  ```c
  void bsp_audio_set_volume(uint8_t percent);
  ```
  Calling `esp_codec_dev_set_out_vol(s_dev, percent)`.
- Recommended default for retro BGM: **60% ~ 75%** (comfortable listening level through internal speaker without clipping).

---

## 3. Audio Format Evaluation for ESP32-C3

ESP32-C3 is a single-core 160MHz RISC-V microcontroller with **no PSRAM** and $\sim 147\text{ KB}$ free SRAM during Baye execution.

| Evaluation Metric | Raw PCM / WAV | IMA ADPCM | Opus Packet Stream | Chiptune Synthesizer |
| :--- | :--- | :--- | :--- | :--- |
| **Data Rate (16kHz mono)** | 32 kB/s | **8 kB/s** | 3 kB/s | **0 kB/s** (algorithmic) |
| **60-Second Flash Size** | 1,920 KB (too big) | **480 KB** (fits partition) | 180 KB | **< 4 KB** code |
| **Decoder SRAM** | 0 bytes | **4 bytes** | 18~25 KB | **~200 bytes** |
| **Audio Task Stack** | 2,048 bytes | **2,048 bytes** | 20,480 bytes (20KB!) | **2,048 bytes** |
| **CPU Usage (160MHz)** | < 0.5% | **< 1.0%** | 15% ~ 25% | **~3%** |
| **Looping Behavior** | Seamless | **Seamless** | Needs cursor reset & pre-skip | **Seamless** |
| **External Dependencies** | None | **None (pure C)** | `esp_audio_codec` | **None (pure C)** |
| **Sound Quality** | Perfect | **Crisp retro** | High fidelity voice/music | Authentic 8-bit retro |

### Strategic Recommendation
1. **Opus Evaluation**:
   While `ai-passport-nt-d` used Opus for compressed voice streams, Opus requires a **20 KiB task stack** plus $18\sim 25\text{ KiB}$ internal decoder heap. In our constrained single-core environment with 64KB battle buffer permanently allocated in SRAM, an Opus decoder consumes nearly 30% of remaining free heap.
2. **IMA ADPCM / Chiptune Winner**:
   - **IMA ADPCM (16 kHz, 4-bit, Mono)**:
     - 4:1 compression ratio (8,000 bytes/sec).
     - Streaming in small chunks of 160 compressed bytes $\rightarrow$ 320 PCM samples (640 bytes).
     - Decoder memory: **4 bytes** (`valprev` and `index`).
     - Task stack: **2,048 bytes**.
     - Seamless looping with zero loop-point click.
     - 0 external library bloat.
   - **Chiptune Synth**:
     - Ideal for retro NES/GB pulse & triangle sound, zero flash audio assets required.

---

## 4. Minimum Audio Architecture & Decoupling

The audio pipeline is strictly isolated from the Baye game engine:

```text
Flash Audio Asset (Embedded or Audio Partition)
        │
        ▼ (Reads small chunk, e.g. 160 bytes)
  Audio Worker Task (2KB stack, Priority 5)
        │
        ▼ (Decodes to 320 PCM samples: 640 bytes)
  Small PCM Chunk Buffer
        │
        ▼ bsp_audio_write()
  ESP-IDF I2S DMA Ring Buffer (8 descriptors × 320 frames)
        │
        ▼
  Everest ES8311 Codec ──> NS4150 PA ──> Speaker
```

- Game task, LCD DMA strips, button ADC, and game timers **never** block on audio writes.
- When audio is disabled via compile-time flag `CONFIG_BAYE_ENHANCED_MUSIC`, zero background tasks and zero audio buffers are allocated.
