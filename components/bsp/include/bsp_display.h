// components/bsp/include/bsp_display.h
// ST7789P3 240x320 显示:SPI 面板初始化 + 厂商专属寄存器 + LEDC 背光调光。
#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include <stdbool.h>
#include <stdint.h>

#define BSP_DISPLAY_PORTRAIT_W  240
#define BSP_DISPLAY_PORTRAIT_H  320
#define BSP_DISPLAY_LANDSCAPE_W 320
#define BSP_DISPLAY_LANDSCAPE_H 240

// 初始化 SPI 总线、面板、厂商寄存器、背光 LEDC。
esp_err_t bsp_display_init(void);

// 设置面板旋转 (swap_xy, mirror_x, mirror_y)
esp_err_t bsp_display_set_rotation(bool swap_xy, bool mirror_x, bool mirror_y);

// 取底层面板句柄，用于 esp_lcd_panel_draw_bitmap()。
esp_lcd_panel_handle_t bsp_display_panel(void);

// 取底层 panel io 句柄。
esp_lcd_panel_io_handle_t bsp_display_io(void);

// 等待前次 DMA 传输完成（以保证写入源 buffer 安全复用）
// timeout_ms: 超时毫秒数 (传入 UINT32_MAX 表示无限等待)
esp_err_t bsp_display_wait_trans_done(uint32_t timeout_ms);

// 背光亮度 0..100(%)。LEDC PWM,0=全灭。
void bsp_display_backlight(uint8_t percent);
