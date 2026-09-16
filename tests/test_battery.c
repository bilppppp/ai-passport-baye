#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "passport_battery.h"

static void test_format_text(void) {
    char buf[16];
    int len;

    len = passport_battery_format_text(100, buf, sizeof(buf));
    assert(len == 4);
    assert(strcmp(buf, "100%") == 0);

    len = passport_battery_format_text(82, buf, sizeof(buf));
    assert(len == 3);
    assert(strcmp(buf, "82%") == 0);

    len = passport_battery_format_text(9, buf, sizeof(buf));
    assert(len == 2);
    assert(strcmp(buf, "9%") == 0);

    len = passport_battery_format_text(0, buf, sizeof(buf));
    assert(len == 2);
    assert(strcmp(buf, "0%") == 0);

    // Negative / error case
    len = passport_battery_format_text(-1, buf, sizeof(buf));
    assert(len == 3);
    assert(strcmp(buf, "--%") == 0);

    // Overflow / invalid percentage
    len = passport_battery_format_text(150, buf, sizeof(buf));
    assert(len == 3);
    assert(strcmp(buf, "--%") == 0);

    // Small buffer guard
    char small_buf[3];
    len = passport_battery_format_text(50, small_buf, sizeof(small_buf));
    assert(len == -1);

    printf("test_format_text: PASS\n");
}

static void test_render_bitmap(void) {
    #define CANARY 0xDEAD
    #define PADDING 16
    const int total_pixels = BATTERY_W * BATTERY_H + PADDING * 2;
    uint16_t buffer[total_pixels];

    // Set canaries
    for (int i = 0; i < total_pixels; i++) {
        buffer[i] = CANARY;
    }

    uint16_t *widget = buffer + PADDING;

    // Render 82%
    passport_battery_render_bitmap(82, widget, BATTERY_W, BATTERY_H);

    // Check canary boundaries are undisturbed (no underflow/overflow)
    for (int i = 0; i < PADDING; i++) {
        assert(buffer[i] == CANARY);
        assert(widget[BATTERY_W * BATTERY_H + i] == CANARY);
    }

    // Check that we have non-zero foreground pixels and non-zero background pixels
    int fg_count = 0;
    int bg_count = 0;
    for (int i = 0; i < BATTERY_W * BATTERY_H; i++) {
        if (widget[i] == 0xFFFF) fg_count++;
        else if (widget[i] == 0x0000) bg_count++;
        else assert(0 && "Unexpected pixel color in widget buffer");
    }

    assert(fg_count > 40 && "Expected reasonable foreground pixel count for battery + digits");
    assert(bg_count > 100 && "Expected background pixels");

    // Render 0%
    passport_battery_render_bitmap(0, widget, BATTERY_W, BATTERY_H);
    int fg_count_0 = 0;
    for (int i = 0; i < BATTERY_W * BATTERY_H; i++) {
        if (widget[i] == 0xFFFF) fg_count_0++;
    }
    // 0% should have fewer foreground pixels than 82% because bars are empty
    assert(fg_count_0 < fg_count);

    // Render 100%
    passport_battery_render_bitmap(100, widget, BATTERY_W, BATTERY_H);
    int fg_count_100 = 0;
    for (int i = 0; i < BATTERY_W * BATTERY_H; i++) {
        if (widget[i] == 0xFFFF) fg_count_100++;
    }
    // 100% should have more foreground pixels than 82% (4 bars + 4 chars "100%")
    assert(fg_count_100 > fg_count);

    // Render invalid/unknown (-1)
    passport_battery_render_bitmap(-1, widget, BATTERY_W, BATTERY_H);
    for (int i = 0; i < PADDING; i++) {
        assert(buffer[i] == CANARY);
        assert(widget[BATTERY_W * BATTERY_H + i] == CANARY);
    }

    printf("test_render_bitmap: PASS\n");
}

int main(void) {
    printf("--- Running test_battery ---\n");
    test_format_text();
    test_render_bitmap();
    return 0;
}
