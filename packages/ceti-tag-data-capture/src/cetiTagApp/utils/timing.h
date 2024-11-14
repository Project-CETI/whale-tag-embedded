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
#include <time.h>   // for struct tm

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_timing();
void updateRtcCount();
int getRtcCount();
void *rtc_thread(void *paramPtr);
int64_t get_global_time_us();
int64_t get_global_time_ms();
int64_t get_global_time_s(void);
int sync_global_time_init(void);
int64_t get_next_time_of_day_occurance_s(const struct tm *time_of_day);
//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_rtc_thread_is_running;

#endif // TIMING_H
