//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef ECG_H
#define ECG_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "ecg_helpers/ecg_gpioExpander.h"
#include "ecg_helpers/ecg_adc.h"
#include "../utils/logging.h"
#include "../utils/timing.h" // for timestamps
#include "../launcher.h"     // for g_exit, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update

#include <pigpio.h>  // for I2C functions
#include <pthread.h> // to set CPU affinity
#include <sched.h>   // to set process priority

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ECG_I2C_BUS 0x01

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_ecg_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_ecg();
void* ecg_thread(void* paramPtr);

#endif // ECG_H





