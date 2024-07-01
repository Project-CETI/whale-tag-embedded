//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "audio.h"

#include "fpga.h"
#include "gpio.h"
#include "iox.h"

#include <pigpio.h>

WTResult wt_audio_init(void) {
    //initialize 5v enable as ouput and drive high
    WT_TRY(wt_iox_init());
    WT_TRY(wt_iox_set_mode(WT_IOX_GPIO_5V_EN, WT_IOX_MODE_OUTPUT));
    WT_TRY(wt_iox_write(WT_IOX_GPIO_5V_EN, 1));

    //initialize audio overflow pin
    gpioSetMode(AUDIO_OVERFLOW_GPIO, PI_INPUT);

    //initialize audio data ready pin
    gpioSetMode(AUDIO_DATA_AVAILABLE, PI_INPUT);

    return WT_OK;
}

int wt_audio_read_data_ready(void) {
    return gpioRead(AUDIO_DATA_AVAILABLE);
}

int wt_audio_read_overflow(void) {
    return gpioRead(AUDIO_OVERFLOW_GPIO);
}
