//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "burnwire.h"
#include "iox.h"

//==== Function Definitions ===================================================

WTResult wt_burnwire_init(void) {
    // Initialize I2C/GPIO functionality.
    WT_TRY(wt_iox_init());

    WT_TRY(wt_iox_set_mode(WT_IOX_GPIO_BURNWIRE_ON, WT_IOX_MODE_OUTPUT));
    WT_TRY(wt_burnwire_off());
    return WT_OK;
}

WTResult wt_burnwire_on(void) {
    return wt_iox_write(WT_IOX_GPIO_BURNWIRE_ON, 1);
}

WTResult wt_burnwire_off(void) {
    return wt_iox_write(WT_IOX_GPIO_BURNWIRE_ON, 0);
}
