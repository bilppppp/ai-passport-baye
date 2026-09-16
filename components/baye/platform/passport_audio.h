#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef CONFIG_BAYE_ENHANCED_MUSIC
#define CONFIG_BAYE_ENHANCED_MUSIC 1
#endif

#ifndef CONFIG_BAYE_MUSIC_VOLUME
#define CONFIG_BAYE_MUSIC_VOLUME 70
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_PLATFORM
#include "esp_err.h"

// Initialize ES8311, I2S master, and launch decoupled background BGM task
esp_err_t passport_audio_init(void);

// Adjust master volume (0..100 percent)
void passport_audio_set_volume(uint8_t volume);

// Stop music playback and terminate worker task
void passport_audio_stop(void);

// Telemetry & metrics
uint32_t passport_audio_get_loop_count(void);
uint32_t passport_audio_get_task_hwm(void);
uint32_t passport_audio_get_underrun_count(void);
void passport_audio_log_telemetry(void);

#else
static inline int passport_audio_init(void) { return 0; }
static inline void passport_audio_set_volume(uint8_t v) { (void)v; }
static inline void passport_audio_stop(void) {}
static inline uint32_t passport_audio_get_loop_count(void) { return 0; }
static inline uint32_t passport_audio_get_task_hwm(void) { return 0; }
static inline uint32_t passport_audio_get_underrun_count(void) { return 0; }
static inline void passport_audio_log_telemetry(void) {}
#endif

#ifdef __cplusplus
}
#endif
