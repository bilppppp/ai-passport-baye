#ifndef PASSPORT_INPUT_H
#define PASSPORT_INPUT_H

#include "bsp_button.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize button handling and register with BSP
void passport_input_init(void);

// Core button event translator (also used for host testing)
void passport_input_on_button(bsp_btn_t btn, bsp_btn_ev_t ev);

#ifdef __cplusplus
}
#endif

#endif // PASSPORT_INPUT_H
