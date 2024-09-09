//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef TIMING_H
#define TIMING_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include <stdint.h> // for int64_t

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------
#define ADDR_RTC 0x68

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_timing();
void updateRtcCount();
int getRtcCount();
int resetRtcCount();
unsigned int getTimeDeploy(void);
int64_t get_global_time_us();
int64_t get_global_time_ms();
int sync_global_time_init(void);
void *rtc_thread(void *paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_rtc_thread_is_running;

#endif // TIMING_H
