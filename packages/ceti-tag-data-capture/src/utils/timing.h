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
#define _GNU_SOURCE   // change how sched.h will be included

#include "logging.h"
#include "../launcher.h" // for g_exit, the state machine data filepath, to get an initial RTC timestamp if needed
#include <pigpio.h>
#include <sys/time.h>
#include <pthread.h> // to set CPU affinity

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
long long get_global_time_us();
long long get_global_time_ms();
void* rtc_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_rtc_thread_is_running;

#endif // TIMING_H