//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_RTC__
#define __CETI_WHALE_TAG_HAL_RTC__

#include "../utils/error.h"

#include <stdint.h>

WTResult rtc_get_count(uint32_t *pCount);
WTResult rtc_set_count(uint32_t count);

#endif