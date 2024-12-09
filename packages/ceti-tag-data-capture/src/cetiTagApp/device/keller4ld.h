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

#define KELLER_4LD_RAW_TO_PRESSURE_BAR(raw) (((PRESSURE_MAX - PRESSURE_MIN) / 32768.0) * ((double)(raw)-16384.0))
#define KELLER_4LD_RAW_TO_TEMPERATURE_C(raw) ((double)(((raw) >> 4) - 24) * .05 - 50.0)

WTResult pressure_get_measurement_raw(int16_t *pPressure, int16_t *pTemp);
WTResult pressure_get_measurement(double *pPressureBar, double *pTempC);

#endif // __CETI_WHALE_TAG_HAL_KELLER_4LD__
