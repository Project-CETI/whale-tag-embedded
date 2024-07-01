//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "burnwire.h"
#include "hal.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_burnwire() {
  WTResult hal_result = wt_burnwire_init();
  if (hal_result != WT_OK){
    CETI_ERR("Failed to initialize the burnwire : %s", wt_strerror(hal_result));
    return -1;
  }

  hal_result = wt_burnwire_off();
  if (hal_result < 0) {
    CETI_ERR("Failed to turn off the burnwire: %s", wt_strerror(hal_result));
    return (-1);
  }
  CETI_LOG("Successfully initialized the burnwire");
  return 0;
}

//-----------------------------------------------------------------------------
// Burnwire interface
//-----------------------------------------------------------------------------

int burnwireOn(void) {
  WTResult hal_result = wt_burnwire_on();
  if (hal_result < 0) {
    CETI_ERR("Failed to turn on the burnwire: %s", wt_strerror(hal_result));
    return (-1);
  }
  return 0;
}

int burnwireOff(void) {
  WTResult hal_result = wt_burnwire_off();
  if (hal_result < 0) {
    CETI_ERR("Failed to turn off the burnwire: %s", wt_strerror(hal_result));
    return (-1);
  }
  return 0;
}
