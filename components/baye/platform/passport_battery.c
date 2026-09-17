#include "passport_battery.h"
#include "passport_display.h"
#include <string.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp_battery.h"

static const char *TAG = "baye_batt";
#define COLOR_BG 0x0000
#define COLOR_FG 0xFFFF
#else
#define COLOR_BG 0x0000
#define COLOR_FG 0xFFFF
#endif

// 5x7 bitmap font for characters: '0'-'9', '%', '-', ' '
// Format: 5 bytes per glyph (column-major, bits 0..6 represent rows 0..6)
static const uint8_t FONT_5X7[13][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x23, 0x13, 0x08, 0x64, 0x62}, // '%'
    {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
};

static const uint8_t *get_glyph(char c) {
    if (c >= '0' && c <= '9') return FONT_5X7[c - '0'];
    if (c == '%') return FONT_5X7[10];
    if (c == '-') return FONT_5X7[11];
    return FONT_5X7[12]; // space or unknown
}

int passport_battery_format_text(int soc, char *out_str, size_t max_len) {
    if (!out_str || max_len < 4) return -1;
    if (soc >= 0 && soc <= 100) {
        return snprintf(out_str, max_len, "%d%%", soc);
    }
    return snprintf(out_str, max_len, "--%%");
}

void passport_battery_render_bitmap(int soc, uint16_t *buf, int width, int height) {
    if (!buf || width < BATTERY_W || height < BATTERY_H) return;

    // 1. Clear entire buffer to background (black)
    for (int i = 0; i < width * height; i++) {
        buf[i] = COLOR_BG;
    }

    // 2. Draw Battery Outline
    // Body: x=0..11, y=1..8 (12 wide, 8 high)
    for (int x = 0; x <= 11; x++) {
        buf[1 * width + x] = COLOR_FG; // Top edge
        buf[8 * width + x] = COLOR_FG; // Bottom edge
    }
    for (int y = 1; y <= 8; y++) {
        buf[y * width + 0]  = COLOR_FG; // Left edge
        buf[y * width + 11] = COLOR_FG; // Right edge
    }
    // Terminal nub: x=12..13, y=3..6
    for (int x = 12; x <= 13; x++) {
        for (int y = 3; y <= 6; y++) {
            buf[y * width + x] = COLOR_FG;
        }
    }

    // 3. Draw Battery Fill Level
    if (soc > 0) {
        int fill_soc = soc > 100 ? 100 : soc;
        int fill_width = (fill_soc * 8 + 50) / 100; // Map 0..100 to 0..8 columns
        if (fill_width < 1) fill_width = 1;
        if (fill_width > 8) fill_width = 8;

        for (int x = 0; x < fill_width; x++) {
            for (int y = 3; y <= 6; y++) {
                buf[y * width + (2 + x)] = COLOR_FG;
            }
        }
    }

    // 4. Format and Render Text
    char text[8];
    passport_battery_format_text(soc, text, sizeof(text));

    int cursor_x = 16;
    int cursor_y = 1; // Font is 7 pixels high, centered in 10-pixel height

    for (int ci = 0; text[ci] != '\0' && cursor_x + 5 <= width; ci++) {
        const uint8_t *glyph = get_glyph(text[ci]);
        for (int col = 0; col < 5; col++) {
            uint8_t col_bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (col_bits & (1 << row)) {
                    int px = cursor_x + col;
                    int py = cursor_y + row;
                    if (px < width && py < height) {
                        buf[py * width + px] = COLOR_FG;
                    }
                }
            }
        }
        cursor_x += 6; // 5 pixels glyph + 1 pixel gap
    }
}

#ifdef ESP_PLATFORM
static uint16_t s_widget_buf[BATTERY_W * BATTERY_H];
static int s_last_soc = -999;
static int64_t s_last_check_us = 0;
#define BATTERY_POLL_INTERVAL_US (30LL * 1000LL * 1000LL) // 30 seconds

esp_err_t passport_battery_init(void) {
    esp_err_t err = bsp_battery_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CW2017 battery gauge not found or failed init: %s", esp_err_to_name(err));
        // Still proceed gracefully: widget will show --%
    }
    s_last_soc = -999;
    s_last_check_us = 0;
    return err;
}

static void update_widget_on_lcd(int soc) {
    passport_battery_render_bitmap(soc, s_widget_buf, BATTERY_W, BATTERY_H);
    passport_display_draw_bitmap_sync(
        BATTERY_POS_X,
        BATTERY_POS_Y,
        BATTERY_POS_X + BATTERY_W,
        BATTERY_POS_Y + BATTERY_H,
        s_widget_buf
    );
}

void passport_battery_force_refresh(void) {
    int soc = bsp_battery_soc();
    s_last_soc = soc;
    s_last_check_us = esp_timer_get_time();
    update_widget_on_lcd(soc);
    ESP_LOGI(TAG, "Battery force refresh: SOC=%d%%", soc);
}

void passport_battery_tick(void) {
    int64_t now = esp_timer_get_time();
    if (s_last_soc != -999 && (now - s_last_check_us) < BATTERY_POLL_INTERVAL_US) {
        return;
    }

    s_last_check_us = now;
    int soc = bsp_battery_soc();

    if (soc != s_last_soc) {
        ESP_LOGI(TAG, "Battery level changed: %d%% -> %d%%", s_last_soc, soc);
        s_last_soc = soc;
        update_widget_on_lcd(soc);
    }
}
#endif
