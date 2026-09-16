#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "passport_display.h"

int main(void) {
    printf("--- Running test_framebuffer ---\n");

    passport_display_init();
    uint8_t *fb = passport_display_get_fb();
    assert(fb != NULL);

    // 1. Check all pixels initially 0
    for (int i = 0; i < BAYE_FB_BYTES; i++) {
        assert(fb[i] == 0);
    }

    // 2. PutPixel test
    SysPutPixel(0, 0, 1);
    assert(fb[0] == 0x80); // bit 7
    SysPutPixel(1, 0, 1);
    assert(fb[0] == 0xC0); // bits 7 and 6
    SysPutPixel(7, 0, 1);
    assert(fb[0] == 0xC1); // bits 7, 6, 0

    // Clear pixel
    SysPutPixel(1, 0, 0);
    assert(fb[0] == 0x81);

    // Far corner pixel (159, 95)
    // byte offset: 95 * 20 + 159/8 = 1900 + 19 = 1919
    // bit: 7 - (159 % 8) = 0
    SysPutPixel(159, 95, 1);
    assert((fb[1919] & 0x01) != 0);

    // Out of bounds check (should not crash)
    SysPutPixel(160, 0, 1);
    SysPutPixel(0, 96, 1);

    // 3. SysLcdPartClear
    SysLcdPartClear(0, 0, 159, 95);
    for (int i = 0; i < BAYE_FB_BYTES; i++) {
        assert(fb[i] == 0);
    }

    // 4. SysRect test: 10x10 rect at (5, 5) -> (14, 14)
    SysRect(5, 5, 14, 14);
    // Point (5, 5) should be 1
    assert((fb[5 * 20 + (5 >> 3)] & (128 >> (5 & 7))) != 0);
    // Point (14, 5) should be 1
    assert((fb[5 * 20 + (14 >> 3)] & (128 >> (14 & 7))) != 0);
    // Point (6, 6) inside should be 0 (hollow)
    assert((fb[6 * 20 + (6 >> 3)] & (128 >> (6 & 7))) == 0);

    // 5. SysLcdReverse test
    SysLcdPartClear(0, 0, 159, 95);
    SysLcdReverse(0, 0, 7, 0); // first 8 pixels
    assert(fb[0] == 0xFF);
    SysLcdReverse(0, 0, 7, 0);
    assert(fb[0] == 0x00);

    // 6. SysPicture test: blit an 8x8 checkerboard bitmap
    uint8_t bmp[8] = {
        0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
    };
    SysPicture(0, 0, 7, 7, bmp, 0); // copy
    for (int y = 0; y < 8; y++) {
        assert(fb[y * 20] == bmp[y]);
    }

    // Test clear flag (flag = 4)
    SysPicture(0, 0, 7, 7, bmp, 4);
    for (int y = 0; y < 8; y++) {
        assert(fb[y * 20] == 0);
    }

    // 7. Screen Save and Restore
    SysPicture(0, 0, 7, 7, bmp, 0);
    SysSaveScreen();
    SysLcdPartClear(0, 0, 159, 95);
    assert(fb[0] == 0);
    SysRestoreScreen();
    assert(fb[0] == 0xAA);

    printf("test_framebuffer: PASS\n");
    return 0;
}
