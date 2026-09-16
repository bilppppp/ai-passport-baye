#include <stdio.h>
#include <string.h>
#include "baye/compa.h"
#include "baye/comm.h"
#include "baye/script.h"
#include "passport_timer.h"
#include "passport_gui.h"
#include "passport_display.h"

static void _timercb(void) {
    MsgType msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VM_TIMER;
    GuiPushMsg(&msg);
}

FAR void SysMemInit(U16 start, U16 len) {
    (void)start;
    (void)len;
    gam_timer_init();
    gam_timer_set_callback(_timercb);
    gam_timer2_open(3, passport_display_flush);
}

FAR U8 SysGetKey(void) {
    MsgType msg;
    GuiGetMsg(&msg);
    return msg.param;
}

#include <time.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#endif

FAR U8 SysGetKeySound(void) {
    return 0;
}

FAR U8 SysGetSecond(void) {
#ifdef ESP_PLATFORM
    return (U8)((esp_timer_get_time() / 1000000ULL) % 60);
#else
    return (U8)(time(NULL) % 60);
#endif
}

FAR U8 SysGetMinute(void) {
#ifdef ESP_PLATFORM
    return (U8)((esp_timer_get_time() / 60000000ULL) % 60);
#else
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    return tm_info ? (U8)tm_info->tm_min : 0;
#endif
}

FAR U8 SysGetHour(void) {
#ifdef ESP_PLATFORM
    return (U8)((esp_timer_get_time() / 3600000000ULL) % 24);
#else
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    return tm_info ? (U8)tm_info->tm_hour : 0;
#endif
}

FAR U8 SysGetTimer1Number(void) {
    return (U8)gam_timer_interval();
}

FAR void SysIconAllClear(void) {
}

FAR void SysIconBattery(U8 data) {
    (void)data;
}

FAR void SysLCDVoltage(U8 voltage) {
    (void)voltage;
}

FAR void SysSetKeySound(U8 keySoundFlag) {
    (void)keySoundFlag;
}

FAR void SysTimer1Close(void) {
    gam_timer_close();
}

FAR void SysTimer1Open(U8 times) {
    gam_timer_open(times);
}

// Dummy hook script implementation for native platform
void script_init(void) {}

int has_hook(const char* name) {
    (void)name;
    return 0;
}

int call_hook_a(const char* name, Value* context) {
    (void)name;
    (void)context;
    return -1;
}

FAR void logPicture(U8 wid, U8 hgt, U8* buffer) {
    (void)wid;
    (void)hgt;
    (void)buffer;
}
