//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>
#include <unistd.h>

typedef enum {
    LED_STATE_FPGA,
    LED_STATE_BURN,
    LED_STATE_SHUTDOWN,
    LED_STATE_REPORT_ERROR,
    LED_STATE_DIVE,
    LED_STATE_EXIT_REPORT_ERROR,
} LEDState;

void LEDCtrl_set_state(LEDState state);
void LEDCtrl_flash_err(size_t bit_len, uint32_t warn, uint32_t err);
void *LEDCtrl_thread(void *pParam);

#endif