#include "passport_audio.h"
#include "passport_adpcm.h"
#include <stdio.h>
#include <string.h>

static uint8_t s_volume = CONFIG_BAYE_MUSIC_VOLUME;

// 5x7 Font for "VOL [0-100]" rendering
static const uint8_t FONT_VOL_5X7[14][5] = {
    {0x07, 0x18, 0x60, 0x18, 0x07}, // 0: 'V'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 1: 'O'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 2: 'L'
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 3: ' '
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 4: '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 5: '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 6: '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 7: '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 8: '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 9: '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 10: '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 11: '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 12: '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 13: '9'
};

static const uint8_t *get_vol_glyph(char c) {
    if (c == 'V' || c == 'v') return FONT_VOL_5X7[0];
    if (c == 'O' || c == 'o') return FONT_VOL_5X7[1];
    if (c == 'L' || c == 'l') return FONT_VOL_5X7[2];
    if (c >= '0' && c <= '9') return FONT_VOL_5X7[4 + (c - '0')];
    return FONT_VOL_5X7[3]; // space
}

int passport_volume_format_text(int volume, char *out_str, size_t max_len) {
    if (!out_str || max_len < 8) return -1;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    return snprintf(out_str, max_len, "VOL %d", volume);
}

void passport_volume_render_bitmap(int volume, uint16_t *buf, int width, int height) {
    if (!buf || width < VOLUME_W || height < VOLUME_H) return;

    for (int i = 0; i < width * height; i++) {
        buf[i] = 0x0000; // Black background
    }

    char text[16];
    passport_volume_format_text(volume, text, sizeof(text));

    int cursor_x = 2;
    int cursor_y = 1; // Font is 7px, centered in 10px

    for (int ci = 0; text[ci] != '\0' && cursor_x + 5 <= width; ci++) {
        const uint8_t *glyph = get_vol_glyph(text[ci]);
        for (int col = 0; col < 5; col++) {
            uint8_t col_bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (col_bits & (1 << row)) {
                    int px = cursor_x + col;
                    int py = cursor_y + row;
                    if (px < width && py < height) {
                        buf[py * width + px] = 0xFFFF; // High-contrast white
                    }
                }
            }
        }
        cursor_x += 6;
    }
}

uint8_t passport_audio_get_volume(void) {
    return s_volume;
}

// --- Binary Asset Declarations ---
#ifdef ESP_PLATFORM
extern const uint8_t _binary_baye_title_16k_adpcm_start[]    asm("_binary_baye_title_16k_adpcm_start");
extern const uint8_t _binary_baye_title_16k_adpcm_end[]      asm("_binary_baye_title_16k_adpcm_end");
extern const uint8_t _binary_baye_strategy_16k_adpcm_start[] asm("_binary_baye_strategy_16k_adpcm_start");
extern const uint8_t _binary_baye_strategy_16k_adpcm_end[]   asm("_binary_baye_strategy_16k_adpcm_end");
extern const uint8_t _binary_baye_battle_16k_adpcm_start[]   asm("_binary_baye_battle_16k_adpcm_start");
extern const uint8_t _binary_baye_battle_16k_adpcm_end[]     asm("_binary_baye_battle_16k_adpcm_end");
extern const uint8_t _binary_baye_victory_16k_adpcm_start[]  asm("_binary_baye_victory_16k_adpcm_start");
extern const uint8_t _binary_baye_victory_16k_adpcm_end[]    asm("_binary_baye_victory_16k_adpcm_end");
extern const uint8_t _binary_baye_defeat_16k_adpcm_start[]   asm("_binary_baye_defeat_16k_adpcm_start");
extern const uint8_t _binary_baye_defeat_16k_adpcm_end[]     asm("_binary_baye_defeat_16k_adpcm_end");

static const baye_music_asset_t s_music_assets[] = {
    {
        .id = BAYE_MUSIC_TITLE,
        .start = _binary_baye_title_16k_adpcm_start,
        .end = _binary_baye_title_16k_adpcm_end,
        .loop = true,
        .name = "TITLE"
    },
    {
        .id = BAYE_MUSIC_STRATEGY,
        .start = _binary_baye_strategy_16k_adpcm_start,
        .end = _binary_baye_strategy_16k_adpcm_end,
        .loop = true,
        .name = "STRATEGY"
    },
    {
        .id = BAYE_MUSIC_BATTLE,
        .start = _binary_baye_battle_16k_adpcm_start,
        .end = _binary_baye_battle_16k_adpcm_end,
        .loop = true,
        .name = "BATTLE"
    },
    {
        .id = BAYE_MUSIC_VICTORY,
        .start = _binary_baye_victory_16k_adpcm_start,
        .end = _binary_baye_victory_16k_adpcm_end,
        .loop = false,
        .name = "VICTORY"
    },
    {
        .id = BAYE_MUSIC_DEFEAT,
        .start = _binary_baye_defeat_16k_adpcm_start,
        .end = _binary_baye_defeat_16k_adpcm_end,
        .loop = false,
        .name = "DEFEAT"
    },
};
#else
// Host test mock assets
static const uint8_t s_mock_title[200] = {0};
static const uint8_t s_mock_strategy[200] = {0};
static const uint8_t s_mock_battle[200] = {0};
static const uint8_t s_mock_victory[100] = {0};
static const uint8_t s_mock_defeat[100] = {0};

static const baye_music_asset_t s_music_assets[] = {
    { BAYE_MUSIC_TITLE,    s_mock_title,    s_mock_title + sizeof(s_mock_title),       true,  "TITLE" },
    { BAYE_MUSIC_STRATEGY, s_mock_strategy, s_mock_strategy + sizeof(s_mock_strategy), true,  "STRATEGY" },
    { BAYE_MUSIC_BATTLE,   s_mock_battle,   s_mock_battle + sizeof(s_mock_battle),     true,  "BATTLE" },
    { BAYE_MUSIC_VICTORY,  s_mock_victory,  s_mock_victory + sizeof(s_mock_victory),   false, "VICTORY" },
    { BAYE_MUSIC_DEFEAT,   s_mock_defeat,   s_mock_defeat + sizeof(s_mock_defeat),     false, "DEFEAT" },
};
#endif

#define ASSET_COUNT (sizeof(s_music_assets) / sizeof(s_music_assets[0]))

const baye_music_asset_t *passport_audio_find_asset(baye_music_track_t track) {
    if (track <= BAYE_MUSIC_NONE || track >= BAYE_MUSIC_MAX) {
        return NULL;
    }
    for (size_t i = 0; i < ASSET_COUNT; i++) {
        if (s_music_assets[i].id == track) {
            return &s_music_assets[i];
        }
    }
    return NULL;
}

// Fade states & commands
typedef enum {
    FADE_IDLE = 0,
    FADE_OUT,
    FADE_IN
} audio_fade_state_t;

typedef struct {
    baye_music_track_t track;
    baye_music_track_t resume_track;
    bool play_once;
} audio_cmd_t;

#define AUDIO_TASK_STACK_BYTES 2560
#define AUDIO_CHUNK_SAMPLES    320 // 20ms at 16kHz
#define AUDIO_SAMPLE_RATE      16000

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp_audio.h"
#include "passport_display.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "passport_gui.h"

static const char *TAG = "baye_audio";

static TaskHandle_t            s_audio_task = NULL;
static QueueHandle_t           s_cmd_queue = NULL;
static volatile bool           s_audio_running = false;
static passport_adpcm_stream_t s_stream;
static passport_adpcm_state_t  s_state;
static int16_t                 s_pcm_buf[AUDIO_CHUNK_SAMPLES];
static uint32_t                s_underrun_count = 0;
static uint32_t                s_last_logged_loop = 0;
static int64_t                 s_last_loop_time_us = 0;

static uint16_t                s_vol_widget_buf[VOLUME_W * VOLUME_H];
static volatile bool           s_vol_dirty = false;

// Manager playback state
static baye_music_track_t      s_current_track = BAYE_MUSIC_NONE;
static baye_music_track_t      s_resume_track = BAYE_MUSIC_NONE;
static bool                    s_is_play_once = false;

// Fade control state (Q15 fixed-point: 0 .. 32768)
static audio_fade_state_t      s_fade_state = FADE_IDLE;
static int32_t                 s_fade_gain = 32768; // 1.0 in Q15
static int32_t                 s_fade_step = 7;     // Default ~300ms fade
static baye_music_track_t      s_target_track = BAYE_MUSIC_NONE;
static baye_music_track_t      s_target_resume = BAYE_MUSIC_NONE;
static bool                    s_target_play_once = false;

static void passport_audio_persist_volume(uint8_t volume) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("baye_cfg", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "volume", volume);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Persisted volume %u%% to NVS (namespace: baye_cfg, key: volume)", (unsigned)volume);
            } else {
                ESP_LOGW(TAG, "Failed to commit volume to NVS: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGW(TAG, "Failed to write volume to NVS: %s (in-memory volume %u%% active)",
                     esp_err_to_name(err), (unsigned)volume);
        }
        nvs_close(handle);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS baye_cfg for writing: %s (in-memory volume %u%% active)",
                 esp_err_to_name(err), (unsigned)volume);
    }
}

static uint8_t passport_audio_load_persisted_volume(void) {
    nvs_handle_t handle;
    uint8_t saved_vol = 0;
    esp_err_t err = nvs_open("baye_cfg", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, "volume", &saved_vol);
        nvs_close(handle);
        if (err == ESP_OK && saved_vol <= 100) {
            ESP_LOGI(TAG, "Restored volume %u%% from NVS (namespace: baye_cfg, key: volume)", (unsigned)saved_vol);
            return saved_vol;
        } else {
            ESP_LOGW(TAG, "Invalid volume in NVS (err=%d, val=%u), fallback to default",
                     err, (unsigned)saved_vol);
        }
    } else {
        ESP_LOGI(TAG, "NVS baye_cfg:volume not present, using default %u%%", (unsigned)CONFIG_BAYE_MUSIC_VOLUME);
    }
    return (uint8_t)CONFIG_BAYE_MUSIC_VOLUME;
}

static void update_volume_widget_on_lcd(int vol) {
    passport_volume_render_bitmap(vol, s_vol_widget_buf, VOLUME_W, VOLUME_H);
    passport_display_draw_bitmap_sync(
        VOLUME_POS_X,
        VOLUME_POS_Y,
        VOLUME_POS_X + VOLUME_W,
        VOLUME_POS_Y + VOLUME_H,
        s_vol_widget_buf
    );
}

void passport_audio_hud_force_refresh(void) {
    s_vol_dirty = false;
    update_volume_widget_on_lcd((int)s_volume);
    ESP_LOGI(TAG, "Volume HUD refreshed: VOL %d", (int)s_volume);
}

void passport_audio_hud_tick(void) {
    if (!s_vol_dirty) return;
    s_vol_dirty = false;
    update_volume_widget_on_lcd((int)s_volume);
}

static void prepare_track_switch(const baye_music_asset_t *asset, bool play_once) {
    passport_adpcm_stream_init(&s_stream, asset->start, (size_t)(asset->end - asset->start), !play_once && asset->loop);
    passport_adpcm_state_reset(&s_state);
    s_last_logged_loop = 0;
    s_last_loop_time_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Switching to track [%s] (%u bytes, loop=%s)",
             asset->name, (unsigned)s_stream.size, s_stream.loop ? "true" : "false");
}

static void passport_audio_worker(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Audio worker active (stack=%u, chunk=%u samples)",
             (unsigned)AUDIO_TASK_STACK_BYTES, (unsigned)AUDIO_CHUNK_SAMPLES);

    s_last_loop_time_us = esp_timer_get_time();

    while (s_audio_running) {
        // 1. Process pending command from queue
        audio_cmd_t cmd;
        while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
            if (cmd.track == s_current_track && s_fade_state != FADE_OUT && !cmd.play_once) {
                // Deduplication: already playing requested track
                continue;
            }

            if (cmd.track == BAYE_MUSIC_NONE) {
                // Stop requested: fade out to silence
                s_target_track = BAYE_MUSIC_NONE;
                s_target_resume = BAYE_MUSIC_NONE;
                s_target_play_once = false;
                s_fade_state = FADE_OUT;
                s_fade_step = 10; // ~200ms fade out
                continue;
            }

            const baye_music_asset_t *target_asset = passport_audio_find_asset(cmd.track);
            if (!target_asset) {
                ESP_LOGW(TAG, "Rejecting invalid track ID %d", (int)cmd.track);
                continue;
            }

            if (s_current_track == BAYE_MUSIC_NONE) {
                // Immediately start playing from silence
                prepare_track_switch(target_asset, cmd.play_once);
                s_current_track = cmd.track;
                s_resume_track = cmd.resume_track;
                s_is_play_once = cmd.play_once;
                s_fade_gain = 0;
                s_fade_state = FADE_IN;
                s_fade_step = (cmd.track == BAYE_MUSIC_VICTORY || cmd.track == BAYE_MUSIC_DEFEAT) ? 20 : 10;
            } else {
                // Fade out current track before switching
                s_target_track = cmd.track;
                s_target_resume = cmd.resume_track;
                s_target_play_once = cmd.play_once;
                s_fade_state = FADE_OUT;
                // Fast fade (150ms) for jingles, 300ms for regular BGM
                s_fade_step = (cmd.track == BAYE_MUSIC_VICTORY || cmd.track == BAYE_MUSIC_DEFEAT) ? 14 : 7;
            }
        }

        // If no track is playing and not fading, wait for command
        if (s_current_track == BAYE_MUSIC_NONE && s_fade_state == FADE_IDLE) {
            if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
                xQueueSendToFront(s_cmd_queue, &cmd, 0);
            }
            continue;
        }

        // 2. Read ADPCM samples from stream
        uint32_t loop_before = s_stream.loop_count;
        size_t samples = passport_adpcm_stream_read(&s_stream, &s_state, s_pcm_buf, AUDIO_CHUNK_SAMPLES);

        // Check loop event for telemetry
        if (s_stream.loop_count != loop_before && s_stream.loop_count != s_last_logged_loop) {
            int64_t now_us = esp_timer_get_time();
            int64_t loop_duration_ms = (now_us - s_last_loop_time_us) / 1000;
            s_last_loop_time_us = now_us;
            s_last_logged_loop = s_stream.loop_count;
            ESP_LOGI(TAG, "[%s] Loop #%u completed (cycle duration: %lld ms)",
                     passport_audio_find_asset(s_current_track)->name,
                     (unsigned)s_last_logged_loop, (long long)loop_duration_ms);
        }

        // Handle one-shot EOF -> auto resume
        if (samples == 0 && s_is_play_once) {
            if (s_resume_track != BAYE_MUSIC_NONE) {
                const baye_music_asset_t *res_asset = passport_audio_find_asset(s_resume_track);
                if (res_asset) {
                    ESP_LOGI(TAG, "One-shot track finished, auto-resuming [%s]", res_asset->name);
                    prepare_track_switch(res_asset, false);
                    s_current_track = s_resume_track;
                    s_resume_track = BAYE_MUSIC_NONE;
                    s_is_play_once = false;
                    s_fade_gain = 0;
                    s_fade_state = FADE_IN;
                    s_fade_step = 10;
                    continue;
                }
            }
            s_current_track = BAYE_MUSIC_NONE;
            s_fade_state = FADE_IDLE;
            continue;
        }

        if (samples == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 3. Apply sample-by-sample transient fade gain (Q15 fixed-point)
        for (size_t i = 0; i < samples; i++) {
            if (s_fade_state == FADE_OUT) {
                if (s_fade_gain > s_fade_step) {
                    s_fade_gain -= s_fade_step;
                } else {
                    s_fade_gain = 0;
                    // FADE_OUT reached 0: switch track now!
                    if (s_target_track != BAYE_MUSIC_NONE) {
                        const baye_music_asset_t *t_asset = passport_audio_find_asset(s_target_track);
                        if (t_asset) {
                            prepare_track_switch(t_asset, s_target_play_once);
                            s_current_track = s_target_track;
                            s_resume_track = s_target_resume;
                            s_is_play_once = s_target_play_once;
                            s_target_track = BAYE_MUSIC_NONE;
                            s_fade_state = FADE_IN;
                            s_fade_step = (s_current_track == BAYE_MUSIC_VICTORY || s_current_track == BAYE_MUSIC_DEFEAT) ? 20 : 10;
                        } else {
                            s_current_track = BAYE_MUSIC_NONE;
                            s_fade_state = FADE_IDLE;
                        }
                    } else {
                        s_current_track = BAYE_MUSIC_NONE;
                        s_fade_state = FADE_IDLE;
                    }
                    // Silence remaining samples in this chunk during the switch
                    for (size_t j = i; j < samples; j++) s_pcm_buf[j] = 0;
                    break;
                }
            } else if (s_fade_state == FADE_IN) {
                if (s_fade_gain + s_fade_step < 32768) {
                    s_fade_gain += s_fade_step;
                } else {
                    s_fade_gain = 32768;
                    s_fade_state = FADE_IDLE;
                }
            }

            if (s_fade_gain < 32768) {
                int32_t val = ((int32_t)s_pcm_buf[i] * s_fade_gain) >> 15;
                s_pcm_buf[i] = (int16_t)val;
            }
        }

        // 4. Output to I2S DMA
        esp_err_t err = bsp_audio_write(s_pcm_buf, samples * sizeof(int16_t));
        if (err != ESP_OK) {
            s_underrun_count++;
            ESP_LOGW(TAG, "I2S DMA write underrun (total: %u)", (unsigned)s_underrun_count);
        }
    }

    ESP_LOGI(TAG, "Audio worker task terminating...");
    s_audio_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t passport_audio_init(void) {
#if !CONFIG_BAYE_ENHANCED_MUSIC
    ESP_LOGI(TAG, "Baye Enhanced Music disabled by compile configuration");
    return ESP_OK;
#else
    if (s_audio_running) {
        ESP_LOGW(TAG, "Audio already running");
        return ESP_OK;
    }

    // 1. Initialize NVS (fail-safe)
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_flash_init returned %s; continuing without persistent config",
                 esp_err_to_name(nvs_err));
    }

    // 2. Initialize Codec & I2S hardware
    esp_err_t err = bsp_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // 3. Configure 16kHz 16-bit Mono format
    err = bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_set_format failed: %s", esp_err_to_name(err));
        return err;
    }

    // 4. Load persisted volume
    s_volume = passport_audio_load_persisted_volume();
    bsp_audio_set_volume(s_volume);
    s_vol_dirty = true;
    ESP_LOGI(TAG, "ES8311 initialized: %uHz 16-bit mono, initial volume=%u%%",
             AUDIO_SAMPLE_RATE, (unsigned)s_volume);

    // 5. Create command queue
    if (!s_cmd_queue) {
        s_cmd_queue = xQueueCreate(4, sizeof(audio_cmd_t));
    }

    // 6. Spawn decoupled audio worker task
    s_audio_running = true;
    BaseType_t ret = xTaskCreate(
        passport_audio_worker,
        "baye_audio",
        AUDIO_TASK_STACK_BYTES,
        NULL,
        5,
        &s_audio_task
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio worker task");
        s_audio_running = false;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
#endif
}

void passport_audio_play(baye_music_track_t track) {
#if CONFIG_BAYE_ENHANCED_MUSIC
    if (!s_cmd_queue) return;
    audio_cmd_t cmd = {
        .track = track,
        .resume_track = BAYE_MUSIC_NONE,
        .play_once = false
    };
    xQueueSend(s_cmd_queue, &cmd, 0);
#else
    (void)track;
#endif
}

void passport_audio_play_once(baye_music_track_t track, baye_music_track_t resume_track) {
#if CONFIG_BAYE_ENHANCED_MUSIC
    if (!s_cmd_queue) return;
    audio_cmd_t cmd = {
        .track = track,
        .resume_track = resume_track,
        .play_once = true
    };
    xQueueSend(s_cmd_queue, &cmd, 0);
#else
    (void)track;
    (void)resume_track;
#endif
}

void passport_audio_stop(void) {
#if CONFIG_BAYE_ENHANCED_MUSIC
    if (!s_cmd_queue) return;
    audio_cmd_t cmd = {
        .track = BAYE_MUSIC_NONE,
        .resume_track = BAYE_MUSIC_NONE,
        .play_once = false
    };
    xQueueSend(s_cmd_queue, &cmd, 0);
#endif
}

baye_music_track_t passport_audio_get_track(void) {
    if (s_target_track != BAYE_MUSIC_NONE) {
        return s_target_track;
    }
    return s_current_track;
}

void passport_audio_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    uint8_t old_vol = s_volume;
    s_volume = volume;

    bsp_audio_set_volume(volume);

    if (s_volume != old_vol) {
        passport_audio_persist_volume(s_volume);
        s_vol_dirty = true;

        MsgType msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = VM_TIMER;
        GuiPushMsg(&msg);
    }
}

void passport_audio_adjust_volume(int delta) {
    int cur = (int)passport_audio_get_volume();
    int target = cur + delta;
    if (target < 0) target = 0;
    if (target > 100) target = 100;
    passport_audio_set_volume((uint8_t)target);
}

uint32_t passport_audio_get_loop_count(void) {
    return s_stream.loop_count;
}

uint32_t passport_audio_get_task_hwm(void) {
    if (!s_audio_task) return 0;
    return (uint32_t)uxTaskGetStackHighWaterMark(s_audio_task);
}

uint32_t passport_audio_get_underrun_count(void) {
    return s_underrun_count;
}

void passport_audio_log_telemetry(void) {
    UBaseType_t hwm = passport_audio_get_task_hwm();
    const baye_music_asset_t *curr = passport_audio_find_asset(s_current_track);
    ESP_LOGI(TAG, "=== AUDIO TELEMETRY ===");
    ESP_LOGI(TAG, "  Worker Status:   %s", s_audio_running ? "RUNNING" : "STOPPED");
    ESP_LOGI(TAG, "  Current Track:   %s (ID %d)", curr ? curr->name : "NONE", (int)s_current_track);
    ESP_LOGI(TAG, "  Current Volume:  %u%%", (unsigned)s_volume);
    ESP_LOGI(TAG, "  Fade State/Gain: %d / %d", (int)s_fade_state, (int)s_fade_gain);
    ESP_LOGI(TAG, "  Loop Count:      %u", (unsigned)s_stream.loop_count);
    ESP_LOGI(TAG, "  Task Stack HWM:  %u bytes free", (unsigned)hwm);
    ESP_LOGI(TAG, "  Underruns:       %u", (unsigned)s_underrun_count);
    ESP_LOGI(TAG, "=======================");
}

#else
// Host stubs and test mocks for unit test suite
static baye_music_track_t s_host_track = BAYE_MUSIC_NONE;
static baye_music_track_t s_host_resume = BAYE_MUSIC_NONE;
static bool s_host_play_once = false;

void passport_audio_play(baye_music_track_t track) {
    s_host_track = track;
    s_host_play_once = false;
    s_host_resume = BAYE_MUSIC_NONE;
}

void passport_audio_play_once(baye_music_track_t track, baye_music_track_t resume_track) {
    s_host_track = track;
    s_host_play_once = true;
    s_host_resume = resume_track;
}

void passport_audio_stop(void) {
    s_host_track = BAYE_MUSIC_NONE;
    s_host_play_once = false;
    s_host_resume = BAYE_MUSIC_NONE;
}

baye_music_track_t passport_audio_get_track(void) {
    return s_host_track;
}

void passport_audio_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    s_volume = volume;
}

void passport_audio_adjust_volume(int delta) {
    int cur = (int)passport_audio_get_volume();
    int target = cur + delta;
    if (target < 0) target = 0;
    if (target > 100) target = 100;
    passport_audio_set_volume((uint8_t)target);
}
#endif
