//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: Ambient Light Sensor device driver for LiteON LTR-329ALS-01
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_LTR329ALS__
#define __CETI_WHALE_TAG_HAL_LTR329ALS__

#include "../utils/error.h" //for WTResult

#include <stdint.h>

WTResult als_wake(void);
WTResult als_get_measurement(int *pVisible, int *pInfrared);
WTResult als_get_manufacturer_id(uint8_t *pManuId);
WTResult als_get_part_id(uint8_t *pPartId, uint8_t *pRevisionId);

#endif // __CETI_WHALE_TAG_HAL_LTR329ALS__
