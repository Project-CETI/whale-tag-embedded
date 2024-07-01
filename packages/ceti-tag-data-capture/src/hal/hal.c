//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "hal.h"
#include <pigpio.h>
#include <stdlib.h>

WTResult wt_initialize(void){
    int init_result = gpioInitialise();
    if (init_result < 0) {
        return WT_RESULT(WT_DEV_NONE, init_result);
    }
    
    return WT_OK;
    atexit(wt_terminate);
}

void wt_terminate(void){
    gpioTerminate();
}