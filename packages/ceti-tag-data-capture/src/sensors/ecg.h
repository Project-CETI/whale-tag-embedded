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
#define ECG_MAX_FILE_SIZE_MB 1024 // Seems to log about 1GiB every 6.5 hours.
                                  // Note that 2GB is the file size maximum for 32-bit systems
#define ECG_NUM_BUFFERS 2      // One for logging, one for writing.
#define ECG_BUFFER_LENGTH 5000 // Once a buffer fills, it will be flushed to a file
#define ECG_SAMPLE_TIMEOUT_US 100000 // Max time to wait for ADC or GPIO expander data to be ready before reconnecting the ECG electronics
#define ECG_ZEROCOUNT_THRESHOLD 100 // Max number of samples to tolerate consecutive 0s before reconnecting the ECG electronics
#define ECG_INVALID_PLACEHOLDER ((long)(-6666666)) // Do not expect large negative voltages (ADC readings are out of ~8e6)
#define ECG_LEADSOFF_INVALID_PLACEHOLDER ((int)(-1)) // Only expect 0 or 1

#define ECG_I2C_BUS 0x00

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_ecg_thread_getData_is_running;
extern int g_ecg_thread_writeData_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_ecg();
int init_ecg_electronics();
int init_ecg_data_file(int restarted_program);
void* ecg_thread_getData(void* paramPtr);
void* ecg_thread_writeData(void* paramPtr);

#endif // ECG_H





