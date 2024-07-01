//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_AUDIO__
#define __CETI_WHALE_TAG_HAL_AUDIO__

#include "error.h"

WTResult wt_audio_init(void);
int wt_audio_read_data_ready(void);
int wt_audio_read_overflow(void);

#endif // __CETI_WHALE_TAG_HAL_AUDIO__
