#include "passport_display.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_display.h"
static const char *TAG = "baye_disp";
static uint32_t s_full_flush_count = 0;
static int64_t  s_total_full_flush_us = 0;
static int64_t  s_last_full_flush_us = 0;
static int64_t  s_min_full_flush_us = 0;
static int64_t  s_max_full_flush_us = 0;
static uint32_t s_dma_timeout_count = 0;

static esp_err_t wait_trans_done_fail_closed(int strip_id) {
    esp_err_t err = bsp_display_wait_trans_done(100);
    if (err != ESP_OK) {
        s_dma_timeout_count++;
        ESP_LOGE(TAG, "Strip %d wait DMA timeout (total timeouts: %u): %s; fail-closed waiting indefinitely...",
                 strip_id, (unsigned)s_dma_timeout_count, esp_err_to_name(err));
        err = bsp_display_wait_trans_done(UINT32_MAX);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Strip %d fatal DMA wait failure: %s", strip_id, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}
#else
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[%s] WARN: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[%s] ERROR: " fmt "\n", tag, ##__VA_ARGS__)
static const char *TAG = "baye_disp";
#endif

// 1 bit per pixel packed framebuffer (160x96)
static uint8_t s_framebuffer[BAYE_FB_BYTES];
static uint8_t s_backup_buffer[BAYE_FB_BYTES];

// Dirty tracking
static bool s_dirty = true;
static int s_dirty_min_y = 0;
static int s_dirty_max_y = BAYE_LOGICAL_H - 1;

// Strip buffer: 8 logical rows = 16 physical rows
#define STRIP_LOGICAL_ROWS 8
#define STRIP_PHYS_ROWS    (STRIP_LOGICAL_ROWS * BAYE_SCALE) // 16
#define STRIP_PIXELS       (PASSPORT_PHYS_W * STRIP_PHYS_ROWS) // 320 * 16 = 5120
#define STRIP_BYTES        (STRIP_PIXELS * sizeof(uint16_t))  // 10240 bytes

static uint16_t *s_strip_buf = NULL;
#ifndef ESP_PLATFORM
static uint16_t s_strip_buf_static[STRIP_PIXELS];
#endif

// Color definitions (RGB565)
// Macro to byte-swap for little-endian SPI DMA transmission to ST7789
#ifdef ESP_PLATFORM
#define TO_LCD_COLOR(c) (uint16_t)((((uint16_t)(c) >> 8) & 0xFF) | (((uint16_t)(c) & 0xFF) << 8))
#else
#define TO_LCD_COLOR(c) ((uint16_t)(c))
#endif

// RGB565 colors
// Retro LCD theme: soft greenish-grey background (#C8D6A4 = 0xC6B4), dark charcoal foreground (#182418 = 0x1923)
#define COLOR_RETRO_BG 0xC6B4
#define COLOR_RETRO_FG 0x1923

// Classic B&W: pure white (#FFFFFF = 0xFFFF), black (#000000 = 0x0000)
#define COLOR_BW_BG    0xFFFF
#define COLOR_BW_FG    0x0000

static uint16_t s_raw_bg = COLOR_RETRO_BG;
static uint16_t s_raw_fg = COLOR_RETRO_FG;
static uint16_t s_lcd_bg = 0;
static uint16_t s_lcd_fg = 0;

void passport_display_set_theme(bool retro) {
    if (retro) {
        s_raw_bg = COLOR_RETRO_BG;
        s_raw_fg = COLOR_RETRO_FG;
    } else {
        s_raw_bg = COLOR_BW_BG;
        s_raw_fg = COLOR_BW_FG;
    }
    s_lcd_bg = TO_LCD_COLOR(s_raw_bg);
    s_lcd_fg = TO_LCD_COLOR(s_raw_fg);
    passport_display_mark_dirty(0, BAYE_LOGICAL_H - 1);
}

void passport_display_mark_dirty(int y1, int y2) {
    if (y1 < 0) y1 = 0;
    if (y2 >= BAYE_LOGICAL_H) y2 = BAYE_LOGICAL_H - 1;
    if (y1 > y2) return;

    if (!s_dirty) {
        s_dirty_min_y = y1;
        s_dirty_max_y = y2;
        s_dirty = true;
    } else {
        if (y1 < s_dirty_min_y) s_dirty_min_y = y1;
        if (y2 > s_dirty_max_y) s_dirty_max_y = y2;
    }
}

uint8_t *passport_display_get_fb(void) {
    return s_framebuffer;
}

uint8_t *passport_display_get_backup_fb(void) {
    return s_backup_buffer;
}

void passport_display_init(void) {
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    memset(s_backup_buffer, 0, sizeof(s_backup_buffer));
    passport_display_set_theme(true);

#ifdef ESP_PLATFORM
    if (!s_strip_buf) {
        s_strip_buf = (uint16_t *)heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!s_strip_buf) {
            ESP_LOGE(TAG, "Failed to allocate DMA strip buffer (%u bytes)", (unsigned)STRIP_BYTES);
            s_strip_buf = (uint16_t *)malloc(STRIP_BYTES);
        }
    }

    esp_lcd_panel_handle_t panel = bsp_display_panel();
    if (panel && s_strip_buf) {
        // Clear top and bottom letterbox bars (black)
        uint16_t black_color = TO_LCD_COLOR(0x0000);
        for (size_t i = 0; i < STRIP_PIXELS; i++) {
            s_strip_buf[i] = black_color;
        }
        // Top border: y = 0..23 (24 rows = 16 rows + 8 rows)
        esp_lcd_panel_draw_bitmap(panel, 0, 0, PASSPORT_PHYS_W, 16, s_strip_buf);
        wait_trans_done_fail_closed(-1);

        esp_lcd_panel_draw_bitmap(panel, 0, 16, PASSPORT_PHYS_W, 24, s_strip_buf);
        wait_trans_done_fail_closed(-2);

        // Bottom border: y = 216..239 (24 rows)
        esp_lcd_panel_draw_bitmap(panel, 0, 216, PASSPORT_PHYS_W, 232, s_strip_buf);
        wait_trans_done_fail_closed(-3);

        esp_lcd_panel_draw_bitmap(panel, 0, 232, PASSPORT_PHYS_W, 240, s_strip_buf);
        wait_trans_done_fail_closed(-4);
    }
#else
    s_strip_buf = s_strip_buf_static;
#endif

    passport_display_mark_dirty(0, BAYE_LOGICAL_H - 1);
    ESP_LOGI(TAG, "1bpp display initialized: %dx%d (scaled 2x -> %dx%d centered at y=%d)",
             BAYE_LOGICAL_W, BAYE_LOGICAL_H, BAYE_RENDER_W, BAYE_RENDER_H, BAYE_OFFSET_Y);
}

void passport_display_flush(void) {
    if (!s_dirty) return;

#ifdef ESP_PLATFORM
    int64_t t_start = esp_timer_get_time();
    esp_lcd_panel_handle_t panel = bsp_display_panel();
    if (!panel || !s_strip_buf) {
        s_dirty = false;
        return;
    }

    int start_strip = s_dirty_min_y / STRIP_LOGICAL_ROWS;
    int end_strip   = s_dirty_max_y / STRIP_LOGICAL_ROWS;

    for (int strip = start_strip; strip <= end_strip; strip++) {
        int ly_start = strip * STRIP_LOGICAL_ROWS;
        int ly_end = ly_start + STRIP_LOGICAL_ROWS;
        if (ly_end > BAYE_LOGICAL_H) ly_end = BAYE_LOGICAL_H;
        int logical_rows_in_strip = ly_end - ly_start;

        uint16_t *dst = s_strip_buf;

        for (int r = 0; r < logical_rows_in_strip; r++) {
            int ly = ly_start + r;
            const uint8_t *src_row = &s_framebuffer[ly * BAYE_LINE_BYTES];
            uint16_t *phys_row0 = dst;
            uint16_t *phys_row1 = dst + PASSPORT_PHYS_W;

            int px = 0;
            for (int col = 0; col < BAYE_LINE_BYTES; col++) {
                uint8_t byte_val = src_row[col];
                for (int b = 7; b >= 0; b--) {
                    uint16_t color = (byte_val & (1 << b)) ? s_lcd_fg : s_lcd_bg;
                    phys_row0[px]     = color;
                    phys_row0[px + 1] = color;
                    phys_row1[px]     = color;
                    phys_row1[px + 1] = color;
                    px += 2;
                }
            }
            dst += PASSPORT_PHYS_W * 2;
        }

        int py_start = BAYE_OFFSET_Y + ly_start * BAYE_SCALE;
        int py_end   = py_start + logical_rows_in_strip * BAYE_SCALE;
        esp_lcd_panel_draw_bitmap(panel, 0, py_start, PASSPORT_PHYS_W, py_end, s_strip_buf);

        // Fail-closed: block until DMA transaction completes before modifying s_strip_buf for next strip
        esp_err_t wait_err = wait_trans_done_fail_closed(strip);
        if (wait_err != ESP_OK) {
            // Unrecoverable DMA failure; abort flush to preserve buffer integrity
            s_dirty = false;
            return;
        }
    }

    if (start_strip == 0 && end_strip == ((BAYE_LOGICAL_H / STRIP_LOGICAL_ROWS) - 1)) {
        int64_t dur_us = esp_timer_get_time() - t_start;
        s_full_flush_count++;
        s_total_full_flush_us += dur_us;
        s_last_full_flush_us = dur_us;
        if (dur_us < s_min_full_flush_us || s_min_full_flush_us == 0) s_min_full_flush_us = dur_us;
        if (dur_us > s_max_full_flush_us) s_max_full_flush_us = dur_us;
    }
#endif

    s_dirty = false;
    s_dirty_min_y = BAYE_LOGICAL_H - 1;
    s_dirty_max_y = 0;
}

void passport_display_log_perf(void) {
#ifdef ESP_PLATFORM
    uint32_t avg_us = s_full_flush_count ? (uint32_t)(s_total_full_flush_us / s_full_flush_count) : 0;
    float avg_ms = avg_us / 1000.0f;
    float last_ms = s_last_full_flush_us / 1000.0f;
    float min_ms = s_min_full_flush_us / 1000.0f;
    float max_ms = s_max_full_flush_us / 1000.0f;
    float max_fps = avg_us ? (1000000.0f / (float)avg_us) : 0.0f;
    ESP_LOGI(TAG, "=== DISPLAY FLUSH PERF ===");
    ESP_LOGI(TAG, "  Full Flushes:   %u", (unsigned)s_full_flush_count);
    ESP_LOGI(TAG, "  Last Full Time: %.2f ms", last_ms);
    ESP_LOGI(TAG, "  Avg Full Time:  %.2f ms (Min: %.2f ms, Max: %.2f ms)", avg_ms, min_ms, max_ms);
    ESP_LOGI(TAG, "  Theoretical FPS:%.1f fps", max_fps);
    ESP_LOGI(TAG, "  DMA Timeouts:   %u", (unsigned)s_dma_timeout_count);
    ESP_LOGI(TAG, "==========================");
#else
    printf("[baye_disp] Display perf not tracked on host\n");
#endif
}

uint32_t passport_display_get_dma_timeout_count(void) {
#ifdef ESP_PLATFORM
    return s_dma_timeout_count;
#else
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// Drawing Primitives Called by Game Core
// ---------------------------------------------------------------------------

void SysPicture(uint8_t sX, uint8_t sY, uint8_t eX, uint8_t eY, uint8_t *pic, uint8_t flag) {
    int wid = (int)eX - (int)sX + 1;
    int hgt = (int)eY - (int)sY + 1;
    if (wid <= 0 || hgt <= 0) return;

    int picPerLine = (wid + 7) / 8;

    for (int y = 0; y < hgt; y++) {
        int Y = (int)sY + y;
        if (Y < 0 || Y >= BAYE_LOGICAL_H) continue;

        const uint8_t *pic_row = pic ? (pic + y * picPerLine) : NULL;
        uint8_t *dst_row = s_framebuffer + Y * BAYE_LINE_BYTES;

        for (int x = 0; x < wid; x++) {
            int X = (int)sX + x;
            if (X < 0 || X >= BAYE_LOGICAL_W) continue;

            uint8_t pixel0 = 0;
            if (pic_row) {
                pixel0 = (pic_row[x >> 3] & (128 >> (x & 7))) ? 1 : 0;
            }

            int byte_idx = X >> 3;
            uint8_t mask = 128 >> (X & 7);
            uint8_t pixel1 = (dst_row[byte_idx] & mask) ? 1 : 0;

            switch (flag) {
                case 0: // Normal
                    pixel1 = pixel0;
                    break;
                case 1: // AND
                    pixel1 = pixel0 && pixel1;
                    break;
                case 2: // OR
                    pixel1 = pixel0 || pixel1;
                    break;
                case 3: // XOR
                    pixel1 = pixel0 ^ pixel1;
                    break;
                case 4: // Clear
                    pixel1 = 0;
                    break;
                default:
                    pixel1 = pixel0;
                    break;
            }

            if (pixel1) {
                dst_row[byte_idx] |= mask;
            } else {
                dst_row[byte_idx] &= ~mask;
            }
        }
    }

    passport_display_mark_dirty(sY, eY);
}

void SysLcdPartClear(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    if (x1 > x2) { uint8_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint8_t t = y1; y1 = y2; y2 = t; }

    for (int y = y1; y <= y2; y++) {
        if (y >= BAYE_LOGICAL_H) break;
        uint8_t *row = s_framebuffer + y * BAYE_LINE_BYTES;
        for (int x = x1; x <= x2; x++) {
            if (x >= BAYE_LOGICAL_W) break;
            row[x >> 3] &= ~(128 >> (x & 7));
        }
    }

    passport_display_mark_dirty(y1, y2);
}

void SysLcdReverse(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    if (x1 > x2) { uint8_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint8_t t = y1; y1 = y2; y2 = t; }

    for (int y = y1; y <= y2; y++) {
        if (y >= BAYE_LOGICAL_H) break;
        uint8_t *row = s_framebuffer + y * BAYE_LINE_BYTES;
        for (int x = x1; x <= x2; x++) {
            if (x >= BAYE_LOGICAL_W) break;
            row[x >> 3] ^= (128 >> (x & 7));
        }
    }

    passport_display_mark_dirty(y1, y2);
}

void SysRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    if (x1 > x2) { uint8_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint8_t t = y1; y1 = y2; y2 = t; }

    // Top and bottom horizontal lines
    for (int x = x1; x <= x2; x++) {
        if (x >= BAYE_LOGICAL_W) break;
        if (y1 < BAYE_LOGICAL_H) s_framebuffer[y1 * BAYE_LINE_BYTES + (x >> 3)] |= (128 >> (x & 7));
        if (y2 < BAYE_LOGICAL_H) s_framebuffer[y2 * BAYE_LINE_BYTES + (x >> 3)] |= (128 >> (x & 7));
    }
    // Left and right vertical lines
    for (int y = y1; y <= y2; y++) {
        if (y >= BAYE_LOGICAL_H) break;
        if (x1 < BAYE_LOGICAL_W) s_framebuffer[y * BAYE_LINE_BYTES + (x1 >> 3)] |= (128 >> (x1 & 7));
        if (x2 < BAYE_LOGICAL_W) s_framebuffer[y * BAYE_LINE_BYTES + (x2 >> 3)] |= (128 >> (x2 & 7));
    }

    passport_display_mark_dirty(y1, y2);
}

void SysRectClear(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    SysLcdPartClear(x1, y1, x2, y2);
}

void SysPutPixel(uint8_t x, uint8_t y, uint8_t data) {
    if (x >= BAYE_LOGICAL_W || y >= BAYE_LOGICAL_H) return;

    uint8_t *row = s_framebuffer + y * BAYE_LINE_BYTES;
    uint8_t mask = 128 >> (x & 7);
    if (data) {
        row[x >> 3] |= mask;
    } else {
        row[x >> 3] &= ~mask;
    }

    passport_display_mark_dirty(y, y);
}

void SysLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    // Bresenham's line algorithm
    int dx = abs((int)x2 - (int)x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs((int)y2 - (int)y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    int x = x1, y = y1;

    while (1) {
        SysPutPixel(x, y, 1);
        if (x == x2 && y == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

void SysAscii(uint8_t x, uint8_t y, uint8_t asc) {
    (void)x; (void)y; (void)asc;
}

void SysSaveScreen(void) {
    memcpy(s_backup_buffer, s_framebuffer, sizeof(s_framebuffer));
}

void SysRestoreScreen(void) {
    memcpy(s_framebuffer, s_backup_buffer, sizeof(s_framebuffer));
    passport_display_mark_dirty(0, BAYE_LOGICAL_H - 1);
}
