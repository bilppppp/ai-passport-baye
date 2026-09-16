#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef CONFIG_BAYE_ENHANCED_MUSIC
#define CONFIG_BAYE_ENHANCED_MUSIC 1
#endif

#ifndef CONFIG_BAYE_MUSIC_VOLUME
#define CONFIG_BAYE_MUSIC_VOLUME 70
#endif

// HUD layout in top 24px letterbox
#define VOLUME_W 48
#define VOLUME_H 10
#define VOLUME_POS_X 6
#define VOLUME_POS_Y 7

#ifdef __cplusplus
extern "C" {
#endif

// Host-testable pure formatting & bitmap rendering functions
int passport_volume_format_text(int volume, char *out_str, size_t max_len);
void passport_volume_render_bitmap(int volume, uint16_t *buf, int width, int height);

// Volume state and adjustment
uint8_t passport_audio_get_volume(void);
void passport_audio_set_volume(uint8_t volume);
void passport_audio_adjust_volume(int delta);

#ifdef ESP_PLATFORM
#include "esp_err.h"

// Initialize ES8311, I2S master, restore volume from NVS, and launch background BGM task
esp_err_t passport_audio_init(void);

// Stop music playback and terminate worker task
void passport_audio_stop(void);

// Telemetry & metrics
uint32_t passport_audio_get_loop_count(void);
uint32_t passport_audio_get_task_hwm(void);
uint32_t passport_audio_get_underrun_count(void);
void passport_audio_log_telemetry(void);

// HUD rendering hooks called from single-threaded display flush lifecycle
void passport_audio_hud_force_refresh(void);
void passport_audio_hud_tick(void);

#else
typedef int esp_err_t;
#define ESP_OK 0
static inline esp_err_t passport_audio_init(void) { return 0; }
static inline void passport_audio_stop(void) {}
static inline uint32_t passport_audio_get_loop_count(void) { return 0; }
static inline uint32_t passport_audio_get_task_hwm(void) { return 0; }
static inline uint32_t passport_audio_get_underrun_count(void) { return 0; }
static inline void passport_audio_log_telemetry(void) {}
static inline void passport_audio_hud_force_refresh(void) {}
static inline void passport_audio_hud_tick(void) {}
#endif

#ifdef __cplusplus
}
#endif

