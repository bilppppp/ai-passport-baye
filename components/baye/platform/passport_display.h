#ifndef PASSPORT_DISPLAY_H
#define PASSPORT_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Baye logical screen dimensions (matches classic BBK 160x96)
#define BAYE_LOGICAL_W    160
#define BAYE_LOGICAL_H    96
#define BAYE_LINE_BYTES   ((BAYE_LOGICAL_W + 7) / 8) // 20 bytes
#define BAYE_FB_BYTES     (BAYE_LINE_BYTES * BAYE_LOGICAL_H) // 1920 bytes

// Physical display dimensions (ST7789 landscape)
#define PASSPORT_PHYS_W   320
#define PASSPORT_PHYS_H   240

// 2x scale placement
#define BAYE_SCALE        2
#define BAYE_RENDER_W     (BAYE_LOGICAL_W * BAYE_SCALE) // 320
#define BAYE_RENDER_H     (BAYE_LOGICAL_H * BAYE_SCALE) // 192
#define BAYE_OFFSET_X     ((PASSPORT_PHYS_W - BAYE_RENDER_W) / 2) // 0
#define BAYE_OFFSET_Y     ((PASSPORT_PHYS_H - BAYE_RENDER_H) / 2) // 24

// Initialize the display subsystem and panel
void passport_display_init(void);

// Color theme
void passport_display_set_theme(bool retro);

// Mark screen dirty and request flush
void passport_display_mark_dirty(int y1, int y2);
void passport_display_flush(void);

// Get direct access to the 1bpp framebuffer (for unit tests / inspection)
uint8_t *passport_display_get_fb(void);
uint8_t *passport_display_get_backup_fb(void);

// Drawing primitives called by Baye game core
void SysPicture(uint8_t sX, uint8_t sY, uint8_t eX, uint8_t eY, uint8_t *pic, uint8_t flag);
void SysLcdPartClear(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SysLcdReverse(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SysRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SysRectClear(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SysPutPixel(uint8_t x, uint8_t y, uint8_t data);
void SysLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SysAscii(uint8_t x, uint8_t y, uint8_t asc);
void SysSaveScreen(void);
void SysRestoreScreen(void);

#ifdef __cplusplus
}
#endif

#endif // PASSPORT_DISPLAY_H
