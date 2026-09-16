#ifndef PASSPORT_TIMER_H
#define PASSPORT_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

void gam_timer_init(void);
void gam_timer_set_callback(void (*cb)(void));
void gam_timer_open(int interval);
void gam_timer_close(void);
int gam_timer_interval(void);
void gam_timer2_open(int interval, void (*callback)(void));

// Tick function that can be called manually or from esp_timer / host test
void passport_timer_tick(void);

#ifdef __cplusplus
}
#endif

#endif // PASSPORT_TIMER_H
