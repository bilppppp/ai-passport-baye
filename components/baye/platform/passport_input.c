#include "passport_input.h"
#include "passport_gui.h"
#include "inc/keytable.h"
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
static const char *TAG = "baye_input";
#else
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
static const char *TAG = "baye_input";
#endif

void passport_input_on_button(bsp_btn_t btn, bsp_btn_ev_t ev) {
    int key = 0;

    switch (btn) {
        case BSP_BTN_UP:
            if (ev == BSP_BTN_CLICK) {
                key = CHAR_UP;
            } else if (ev == BSP_BTN_LONG) {
                key = CHAR_LEFT;
            }
            break;

        case BSP_BTN_DOWN:
            if (ev == BSP_BTN_CLICK) {
                key = CHAR_DOWN;
            } else if (ev == BSP_BTN_LONG) {
                key = CHAR_RIGHT;
            }
            break;

        case BSP_BTN_OK:
            if (ev == BSP_BTN_CLICK) {
                key = CHAR_ENTER;
            } else if (ev == BSP_BTN_LONG) {
                key = CHAR_EXIT;
            } else if (ev == BSP_BTN_DOUBLE) {
                key = CHAR_HELP;
            }
            break;

        default:
            break;
    }

    if (key != 0) {
        ESP_LOGI(TAG, "Btn %d, Ev %d -> Key 0x%02X", (int)btn, (int)ev, key);
        bayeSendKey(key);
    }
}

#ifdef ESP_PLATFORM
static void bsp_button_event_callback(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    passport_input_on_button(btn, ev);
}
#endif

void passport_input_init(void) {
#ifdef ESP_PLATFORM
    esp_err_t err = bsp_button_init(bsp_button_event_callback, NULL);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Passport buttons registered successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize buttons: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGI(TAG, "Host input initialized");
#endif
}
