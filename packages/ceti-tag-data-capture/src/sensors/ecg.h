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
#include <unistd.h>  // for access() to check file existence
#include <pthread.h> // to set CPU affinity
#include <sched.h>   // to set process priority

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ECG_MAX_FILE_SIZE_MB 1024 // Seems to log about 1GB every 6.5 hours.
                                  // Note that 2GB is the file size maximum for 32-bit systems
#define ECG_I2C_BUS 0x01

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_ecg_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_ecg();
int init_ecg_data_file(int restarted_program);
void* ecg_thread(void* paramPtr);

#endif // ECG_H





