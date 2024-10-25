//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: PDevice driver for Keller 4LD pressure transmitter
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_KELLER_4LD__
#define __CETI_WHALE_TAG_HAL_KELLER_4LD__

#include "../utils/error.h" //for WTResult

#include <stdint.h>

WTResult pressure_get_measurement(double *pPressureBar, double *pTempC);

#endif // __CETI_WHALE_TAG_HAL_KELLER_4LD__
