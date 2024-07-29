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
// Initialize and connect the GPIO expander via I2C.
WTResult wt_ecg_iox_init(void){
    // Initialize I2C/GPIO functionality.
    WT_TRY(iox_init());

    WT_TRY(iox_set_mode(IOX_GPIO_ECG_LOD_N, IOX_MODE_INPUT));
    WT_TRY(iox_set_mode(IOX_GPIO_ECG_LOD_P, IOX_MODE_INPUT));
    
    return WT_OK;
}

//-----------------------------------------------------------------------------
// Read/parse data
//-----------------------------------------------------------------------------

// Read the ECG leads-off detection (positive electrode) output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
WTResult wt_ecg_iox_read_leadsOff_p(int *value) {
  return iox_read(IOX_GPIO_ECG_LOD_P, value);
}

// Read the ECG leads-off detection (negative electrode) output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
WTResult wt_ecg_iox_read_leadsOff_n(int *value) {
  return iox_read(IOX_GPIO_ECG_LOD_N, value);
}

// Given a byte of all GPIO expander inputs, extract the ECG leads-off detection bit (positive electrode).
int wt_ecg_iox_parse_leadsOff_p(uint8_t data) {
  return (data >> IOX_GPIO_ECG_LOD_P) & 0b00000001;
}

// Given a byte of all GPIO expander inputs, extract the ECG leads-off detection bit (negative electrode).
int wt_ecg_iox_parse_leadsOff_n(uint8_t data) {
  return (data >> IOX_GPIO_ECG_LOD_N) & 0b00000001;
}
