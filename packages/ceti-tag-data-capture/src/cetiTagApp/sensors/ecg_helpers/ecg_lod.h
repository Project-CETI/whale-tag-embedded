//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#ifndef __CETI_WHALE_TAG_HAL_ECG_H__
#define __CETI_WHALE_TAG_HAL_ECG_H__

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "../../utils/error.h"  // for WTResult
#include <stdint.h>             // for uint8_t

// ------------------------------------------
// Definitions/Configuration
// ------------------------------------------

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------

WTResult wt_ecg_iox_init(void);

WTResult wt_ecg_iox_read_leadsOff_p(int *value);
WTResult wt_ecg_iox_read_leadsOff_n(int *value);
int wt_ecg_iox_parse_leadsOff_p(uint8_t data);
int wt_ecg_iox_parse_leadsOff_n(uint8_t data);

#endif // __CETI_WHALE_TAG_HAL_ECG_H__




