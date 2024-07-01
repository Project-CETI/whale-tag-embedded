//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef __CETI_WHALE_TAG_HAL_BURNWIRE_H__
#define __CETI_WHALE_TAG_HAL_BURNWIRE_H__

#include "error.h"

WTResult wt_burnwire_init(void);
WTResult wt_burnwire_on(void);
WTResult wt_burnwire_off(void);

#endif // __CETI_WHALE_TAG_HAL_BURNWIRE_H__
