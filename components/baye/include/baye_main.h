#pragma once

#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Launch the Baye game core task
esp_err_t baye_game_start(void);

// Telemetry helper to monitor RAM and stack
void baye_log_telemetry(const char *phase_label);

#ifdef __cplusplus
}
#endif
