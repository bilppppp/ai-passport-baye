#include "passport_audio.h"
#include "passport_adpcm.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp_audio.h"

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

    // 1. Initialize Codec & I2S hardware
    esp_err_t err = bsp_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Configure 16kHz 16-bit Mono format
    err = bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_set_format failed: %s", esp_err_to_name(err));
        return err;
    }

    // 3. Set Master Volume
    bsp_audio_set_volume(CONFIG_BAYE_MUSIC_VOLUME);
    ESP_LOGI(TAG, "ES8311 initialized: %uHz 16-bit mono, volume=%u%%",
             AUDIO_SAMPLE_RATE, (unsigned)CONFIG_BAYE_MUSIC_VOLUME);

    // 4. Initialize stream from Flash asset
    size_t bgm_bytes = (size_t)(_binary_baye_bgm_16k_adpcm_end - _binary_baye_bgm_16k_adpcm_start);
    if (bgm_bytes == 0) {
        ESP_LOGE(TAG, "BGM asset empty or missing");
        return ESP_ERR_INVALID_SIZE;
    }

    passport_adpcm_state_reset(&s_state);
    passport_adpcm_stream_init(&s_stream, _binary_baye_bgm_16k_adpcm_start, bgm_bytes, true);

    ESP_LOGI(TAG, "BGM asset loaded from Flash: %u bytes (~%u samples, %.2f seconds)",
             (unsigned)bgm_bytes, (unsigned)(bgm_bytes * 2), (float)(bgm_bytes * 2) / AUDIO_SAMPLE_RATE);

    // 5. Spawn background audio worker task
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
    bsp_audio_set_volume(volume);
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
    ESP_LOGI(TAG, "  Loop Count:      %u", (unsigned)s_stream.loop_count);
    ESP_LOGI(TAG, "  Task Stack HWM:  %u bytes free", (unsigned)hwm);
    ESP_LOGI(TAG, "  Underruns:       %u", (unsigned)s_underrun_count);
    ESP_LOGI(TAG, "=======================");
}

#endif
