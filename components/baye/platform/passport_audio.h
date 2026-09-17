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

// Music track enumeration for Baye Passport Enhanced OST
typedef enum {
    BAYE_MUSIC_NONE = 0,
    BAYE_MUSIC_TITLE,
    BAYE_MUSIC_STRATEGY,
    BAYE_MUSIC_BATTLE,
    BAYE_MUSIC_VICTORY,
    BAYE_MUSIC_DEFEAT,
    BAYE_MUSIC_MAX
} baye_music_track_t;

// Asset metadata descriptor
typedef struct {
    baye_music_track_t id;
    const uint8_t     *start;
    const uint8_t     *end;
    bool               loop;
    const char        *name;
} baye_music_asset_t;

// Host-testable pure formatting & bitmap rendering functions
int passport_volume_format_text(int volume, char *out_str, size_t max_len);
void passport_volume_render_bitmap(int volume, uint16_t *buf, int width, int height);

// Volume state and adjustment
uint8_t passport_audio_get_volume(void);
void passport_audio_set_volume(uint8_t volume);
void passport_audio_adjust_volume(int delta);

// Music Manager Controls
// Play track in looping or default mode (deduplicated: no-op if already active)
void passport_audio_play(baye_music_track_t track);

// Play a one-shot track (e.g. VICTORY, DEFEAT) and automatically resume resume_track upon EOF
void passport_audio_play_once(baye_music_track_t track, baye_music_track_t resume_track);

// Stop playback with smooth fade out
void passport_audio_stop(void);

// Get currently playing (or target) track
baye_music_track_t passport_audio_get_track(void);

// Look up asset metadata from table
const baye_music_asset_t *passport_audio_find_asset(baye_music_track_t track);

#ifdef ESP_PLATFORM
#include "esp_err.h"

// Initialize ES8311, I2S master, restore volume from NVS, and launch background music worker
esp_err_t passport_audio_init(void);

// Telemetry & metrics
uint32_t passport_audio_get_loop_count(void);
uint32_t passport_audio_get_task_hwm(void);
uint32_t passport_audio_get_underrun_count(void);
void passport_audio_log_telemetry(void);

// HUD rendering hooks called from single-threaded display flush lifecycle
void passport_audio_hud_force_refresh(void);
void passport_audio_hud_tick(void);

#else
#ifndef _ESP_ERR_T_DEFINED
#define _ESP_ERR_T_DEFINED
typedef int esp_err_t;
#endif
#ifndef ESP_OK
#define ESP_OK 0
#endif
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
static inline esp_err_t passport_audio_init(void) { return ESP_OK; }
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
