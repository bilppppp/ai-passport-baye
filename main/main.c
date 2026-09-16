#include "esp_log.h"
#include "esp_err.h"
#include "bsp_display.h"
#include "baye_main.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  AI Passport: 三国霸业 (Baye Port)");
    ESP_LOGI(TAG, "  ESP32-C3 | ST7789P3 320x240 | 1bpp -> 2x RGB565");
    ESP_LOGI(TAG, "==================================================");

    // Initialize display hardware (ST7789 panel + backlight)
    esp_err_t err = bsp_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_init failed: %s", esp_err_to_name(err));
    }
    // Turn on display backlight
    bsp_display_backlight(85);

    // Launch Baye Game Task
    err = baye_game_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "baye_game_start failed: %s", esp_err_to_name(err));
    }
}
