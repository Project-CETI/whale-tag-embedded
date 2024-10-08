//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//
// Description: device driver for LiteON LTR-329ALS-01 ambient light sensor
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_LIGHT__
#define __CETI_WHALE_TAG_HAL_LIGHT__

#include "../utils/error.h"

#include <stdint.h>

WTResult light_wake(void);
WTResult light_get_value(int *pVisible, int *pInfrared);
WTResult light_get_manufacturer(uint8_t *pManuId);
WTResult light_get_part(uint8_t *pPartId, uint8_t *pRevision);

#endif // __CETI_WHALE_TAG_HAL_LIGHT__