#include "passport_timer.h"
#include <stdbool.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_log.h"
static const char *TAG = "baye_timer";
#else
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
static const char *TAG = "baye_timer";
#endif

typedef struct {
    void (*cb)(void);
    int interval;
    int tick;
    bool enabled;
} baye_timer_t;

static baye_timer_t s_timer1 = {0};
static baye_timer_t s_timer2 = {0};

#ifdef ESP_PLATFORM
static esp_timer_handle_t s_esp_timer = NULL;

static void esp_timer_cb(void *arg) {
    (void)arg;
    passport_timer_tick();
}
#endif

void passport_timer_tick(void) {
    if (s_timer1.enabled) {
        if (--s_timer1.tick <= 0) {
            s_timer1.tick = s_timer1.interval;
            if (s_timer1.cb) {
                s_timer1.cb();
            }
        }
    }

    if (s_timer2.enabled) {
        if (--s_timer2.tick <= 0) {
            s_timer2.tick = s_timer2.interval;
            if (s_timer2.cb) {
                s_timer2.cb();
            }
        }
    }
}

void gam_timer_init(void) {
#ifdef ESP_PLATFORM
    if (!s_esp_timer) {
        const esp_timer_create_args_t args = {
            .callback = &esp_timer_cb,
            .arg = NULL,
            .name = "baye_tick",
            .dispatch_method = ESP_TIMER_TASK,
        };
        esp_err_t err = esp_timer_create(&args, &s_esp_timer);
        if (err == ESP_OK) {
            // Periodic 10ms (10,000 us = 100 Hz = 1% sec)
            esp_timer_start_periodic(s_esp_timer, 10000);
            ESP_LOGI(TAG, "Timer started (10ms tick)");
        } else {
            ESP_LOGE(TAG, "Failed to create esp_timer: %s", esp_err_to_name(err));
        }
    }
#endif
}

void gam_timer_set_callback(void (*cb)(void)) {
    s_timer1.cb = cb;
}

void gam_timer_open(int interval) {
    if (interval <= 0) interval = 1;
    s_timer1.interval = interval;
    s_timer1.tick = interval;
    s_timer1.enabled = true;
}

void gam_timer_close(void) {
    s_timer1.enabled = false;
}

int gam_timer_interval(void) {
    return s_timer1.interval;
}

void gam_timer2_open(int interval, void (*callback)(void)) {
    if (interval <= 0) interval = 1;
    s_timer2.cb = callback;
    s_timer2.interval = interval;
    s_timer2.tick = interval;
    s_timer2.enabled = true;
}
