#include "baye_main.h"
#include "passport_display.h"
#include "passport_fsys.h"
#include "passport_input.h"
#include "passport_timer.h"
#include "passport_gui.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#define BAYE_GAME_TASK_STACK_BYTES 16384

static const char *TAG = "baye_main";
static TaskHandle_t s_game_task_handle = NULL;

extern void GamBaYeEng(void);
extern uint8_t *g_FightMapData;

void baye_log_telemetry(const char *phase_label) {
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_heap  = esp_get_minimum_free_heap_size();
    uint32_t max_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    UBaseType_t stack_hwm_bytes = s_game_task_handle ? uxTaskGetStackHighWaterMark(s_game_task_handle) : 0;

    ESP_LOGI(TAG, "=== RAM TELEMETRY [%s] ===", phase_label ? phase_label : "CHECK");
    ESP_LOGI(TAG, "  Free Heap:       %u bytes (%u KB)", (unsigned)free_heap, (unsigned)(free_heap / 1024));
    ESP_LOGI(TAG, "  Min Free Heap:   %u bytes (%u KB)", (unsigned)min_heap, (unsigned)(min_heap / 1024));
    ESP_LOGI(TAG, "  Largest Block:   %u bytes (%u KB)", (unsigned)max_block, (unsigned)(max_block / 1024));
    ESP_LOGI(TAG, "  Task Config Stk: %u bytes (%u KB)", (unsigned)BAYE_GAME_TASK_STACK_BYTES, (unsigned)(BAYE_GAME_TASK_STACK_BYTES / 1024));
    ESP_LOGI(TAG, "  Task Stack HWM:  %u bytes free", (unsigned)stack_hwm_bytes);
    ESP_LOGI(TAG, "  Battle RAM Gate: %p (65536 bytes / 64 KB contiguous SRAM)", (void *)g_FightMapData);
    ESP_LOGI(TAG, "================================");
    passport_display_log_perf();
}

static void baye_game_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Starting Baye Game Task on ESP32-C3");

    baye_log_telemetry("PRE-INIT");

    passport_display_init();
    passport_fsys_init();
    passport_input_init();

    baye_log_telemetry("POST-INIT");

    while (1) {
        ESP_LOGI(TAG, "Entering GamBaYeEng()");
        GamBaYeEng();
        ESP_LOGI(TAG, "GamBaYeEng() returned, restarting in 2 seconds...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

esp_err_t baye_game_start(void) {
    // 16 KiB stack for game core (ESP-IDF xTaskCreate takes bytes)
    BaseType_t ret = xTaskCreate(
        baye_game_task,
        "baye_game",
        BAYE_GAME_TASK_STACK_BYTES,
        NULL,
        5,
        &s_game_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create baye_game task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "baye_game task created successfully (stack: %u bytes)", (unsigned)BAYE_GAME_TASK_STACK_BYTES);
    return ESP_OK;
}
#else
void baye_log_telemetry(const char *phase_label) {
    printf("[baye_telemetry] %s\n", phase_label);
}

esp_err_t baye_game_start(void) {
    return 0;
}
#endif
