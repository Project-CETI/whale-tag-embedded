//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#include "../../iox.h"
#include "../../utils/error.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

WTResult init_ecg_leadsOff() {
    // Initialize I2C/GPIO functionality for the IO expander.
    WT_TRY(iox_set_mode(IOX_GPIO_ECG_LOD_N, IOX_MODE_INPUT));
    WT_TRY(iox_set_mode(IOX_GPIO_ECG_LOD_P, IOX_MODE_INPUT));
    
    return WT_OK;
}

//-----------------------------------------------------------------------------
// Read/parse data
//-----------------------------------------------------------------------------

// Read both ECG leads-off detections (positive and negative electrodes).
// Will first read all inputs of the GPIO expander, then extract the desired bit.
// Will use a single IO expander reading, so
//   both detections are effectively sampled simultaneously
//   and the IO expander only needs to be queried once.
WTResult ecg_read_leadsOff(int* leadsOff_p, int* leadsOff_n) {
  // Read the latest result, and request an asynchronous reading for the next iteration.
  uint8_t register_value = 0;
  iox_read_register(&register_value, 0);
  // Extract the desired pins.
  *leadsOff_p = ((register_value >> IOX_GPIO_ECG_LOD_P) & 1);
  *leadsOff_n = ((register_value >> IOX_GPIO_ECG_LOD_N) & 1);
  return WT_OK;
}

// Read the ECG leads-off detection (positive electrode) output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
WTResult ecg_read_leadsOff_p(int* leadsOff_p) {
  // Read the latest result, and request an asynchronous reading for the next iteration.
  return iox_read_pin(IOX_GPIO_ECG_LOD_P, leadsOff_p, 0);
}

// Read the ECG leads-off detection (negative electrode) output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
WTResult ecg_read_leadsOff_n(int* leadsOff_n) {
  // Read the latest result, and request an asynchronous reading for the next iteration.
  return iox_read_pin(IOX_GPIO_ECG_LOD_N, leadsOff_n, 0);
}

