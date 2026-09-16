#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Dimensions of the tiny top-right battery status widget
#define BATTERY_W 48
#define BATTERY_H 10
#define BATTERY_POS_X 266
#define BATTERY_POS_Y 7

// Host-testable pure formatting function
// Returns length written or -1 on buffer overflow
int passport_battery_format_text(int soc, char *out_str, size_t max_len);

// Host-testable pure bitmap rendering function
// Renders battery icon + percentage string into a 16-bit RGB565 buffer (width x height)
// Colors are pre-formatted for ST7789 display transmission
void passport_battery_render_bitmap(int soc, uint16_t *buf, int width, int height);

#ifdef ESP_PLATFORM
#include "esp_err.h"
// Hardware initialization: attaches CW2017 battery fuel gauge
esp_err_t passport_battery_init(void);

// Periodic tick called from display flush pipeline (safe single-threaded context)
// Samples CW2017 every 30~60 seconds and updates the widget only on change
void passport_battery_tick(void);

// Force refresh the battery widget on LCD (e.g. during boot/theme reset)
void passport_battery_force_refresh(void);
#endif

#ifdef __cplusplus
}
#endif
