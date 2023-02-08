//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef TIMING_H
#define TIMING_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "logging.h"
#include "../launcher.h" // for the state machine data filepath, to get an initial RTC timestamp if needed
#include <pigpio.h>
#include <sys/time.h>

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------
#define ADDR_RTC 0x68

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_timing();
int getRtcCount();
int resetRtcCount();
unsigned int getTimeDeploy(void);
long long get_global_time_us();
long long get_global_time_ms();

#endif // TIMING_H