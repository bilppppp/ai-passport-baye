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
        buf[i] = 0x0000;
    }

    char text[16];
    passport_volume_format_text(volume, text, sizeof(text));

    int cursor_x = 2;
    int cursor_y = 1;

    for (int ci = 0; text[ci] != '\0' && cursor_x + 5 <= width; ci++) {
        const uint8_t *glyph = get_vol_glyph(text[ci]);
        for (int col = 0; col < 5; col++) {
            uint8_t col_bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (col_bits & (1 << row)) {
                    int px = cursor_x + col;
                    int py = cursor_y + row;
                    if (px < width && py < height) {
                        buf[py * width + px] = 0xFFFF;
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

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp_audio.h"
#include "passport_display.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "passport_gui.h"

static const char *TAG = "baye_audio";

#define AUDIO_TASK_STACK_BYTES 2560
#define AUDIO_CHUNK_SAMPLES    320 // 20ms at 16kHz
#define AUDIO_SAMPLE_RATE      16000

// Binary asset symbols linked via target_add_binary_data
extern const uint8_t _binary_baye_bgm_16k_adpcm_start[] asm("_binary_baye_bgm_16k_adpcm_start");
extern const uint8_t _binary_baye_bgm_16k_adpcm_end[]   asm("_binary_baye_bgm_16k_adpcm_end");

static TaskHandle_t            s_audio_task = NULL;
static volatile bool           s_audio_running = false;
static passport_adpcm_stream_t s_stream;
static passport_adpcm_state_t  s_state;
static int16_t                 s_pcm_buf[AUDIO_CHUNK_SAMPLES];
static uint32_t                s_underrun_count = 0;
static uint32_t                s_last_logged_loop = 0;
static int64_t                 s_last_loop_time_us = 0;

static uint16_t                s_vol_widget_buf[VOLUME_W * VOLUME_H];
static volatile bool           s_vol_dirty = false;

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

static void passport_audio_worker(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Audio worker task active (stack=%u bytes, chunk=%u samples)",
             (unsigned)AUDIO_TASK_STACK_BYTES, (unsigned)AUDIO_CHUNK_SAMPLES);

    s_last_loop_time_us = esp_timer_get_time();

    while (s_audio_running) {
        uint32_t loop_before = s_stream.loop_count;

        size_t samples = passport_adpcm_stream_read(
            &s_stream,
            &s_state,
            s_pcm_buf,
            AUDIO_CHUNK_SAMPLES
        );

        if (s_stream.loop_count != loop_before && s_stream.loop_count != s_last_logged_loop) {
            int64_t now_us = esp_timer_get_time();
            int64_t loop_duration_ms = (now_us - s_last_loop_time_us) / 1000;
            s_last_loop_time_us = now_us;
            s_last_logged_loop = s_stream.loop_count;
            ESP_LOGI(TAG, "BGM Loop #%u completed (cycle duration: %lld ms, seamless rewind)",
                     (unsigned)s_last_logged_loop, (long long)loop_duration_ms);
        }

        if (samples == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        esp_err_t err = bsp_audio_write(s_pcm_buf, samples * sizeof(int16_t));
        if (err != ESP_OK) {
            s_underrun_count++;
            ESP_LOGW(TAG, "I2S DMA write underrun / submit fail (total: %u)", (unsigned)s_underrun_count);
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

    // 1. Ensure NVS flash is initialized for persistent configuration
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

    // 4. Load persisted volume (or fallback to CONFIG_BAYE_MUSIC_VOLUME)
    s_volume = passport_audio_load_persisted_volume();
    bsp_audio_set_volume(s_volume);
    s_vol_dirty = true;
    ESP_LOGI(TAG, "ES8311 initialized: %uHz 16-bit mono, initial volume=%u%%",
             AUDIO_SAMPLE_RATE, (unsigned)s_volume);

    // 5. Initialize stream from Flash asset
    size_t bgm_bytes = (size_t)(_binary_baye_bgm_16k_adpcm_end - _binary_baye_bgm_16k_adpcm_start);
    if (bgm_bytes == 0) {
        ESP_LOGE(TAG, "BGM asset empty or missing");
        return ESP_ERR_INVALID_SIZE;
    }

    passport_adpcm_state_reset(&s_state);
    passport_adpcm_stream_init(&s_stream, _binary_baye_bgm_16k_adpcm_start, bgm_bytes, true);

    ESP_LOGI(TAG, "BGM asset loaded from Flash: %u bytes (~%u samples, %.2f seconds)",
             (unsigned)bgm_bytes, (unsigned)(bgm_bytes * 2), (float)(bgm_bytes * 2) / AUDIO_SAMPLE_RATE);

    // 6. Spawn background audio worker task
    s_audio_running = true;
    BaseType_t ret = xTaskCreate(
        passport_audio_worker,
        "baye_audio",
        AUDIO_TASK_STACK_BYTES,
        NULL,
        5, // Priority 5 matches game task, natural cooperative I2S DMA blocking
        &s_audio_task
    );

    if (ret != pdPASS) {
        s_audio_running = false;
        ESP_LOGE(TAG, "Failed to create baye_audio task");
        return ESP_FAIL;
    }

    return ESP_OK;
#endif
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

void passport_audio_stop(void) {
    s_audio_running = false;
}

uint32_t passport_audio_get_loop_count(void) {
    return s_stream.loop_count;
}

uint32_t passport_audio_get_task_hwm(void) {
    return s_audio_task ? uxTaskGetStackHighWaterMark(s_audio_task) : 0;
}

uint32_t passport_audio_get_underrun_count(void) {
    return s_underrun_count;
}

void passport_audio_log_telemetry(void) {
    UBaseType_t hwm = passport_audio_get_task_hwm();
    ESP_LOGI(TAG, "=== AUDIO TELEMETRY ===");
    ESP_LOGI(TAG, "  Worker Status:   %s", s_audio_running ? "RUNNING" : "STOPPED");
    ESP_LOGI(TAG, "  Current Volume:  %u%%", (unsigned)s_volume);
    ESP_LOGI(TAG, "  Loop Count:      %u", (unsigned)s_stream.loop_count);
    ESP_LOGI(TAG, "  Task Stack HWM:  %u bytes free", (unsigned)hwm);
    ESP_LOGI(TAG, "  Underruns:       %u", (unsigned)s_underrun_count);
    ESP_LOGI(TAG, "=======================");
}

#else
// Host stubs for volume state testing
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
