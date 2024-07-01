//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_H__
#define __CETI_WHALE_TAG_HAL_H__

//==== Public Headers =========================================================
#include "hal/error.h"

#include "hal/audio.h"
#include "hal/burnwire.h"
#include "hal/ecg.h"
#include "hal/fpga.h"
#include "hal/gpio.h"
#include "hal/iox.h"
#include "hal/recovery.h"

//==== Function Declarations ===================================================

WTResult wt_initialize(void);

void wt_terminate(void);

#endif // CETI_WHALE_TAG_HAL_H
