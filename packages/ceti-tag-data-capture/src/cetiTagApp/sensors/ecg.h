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

#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"
#include "../utils/timing.h" // for timestamps
#include "ecg_helpers/ecg_adc.h"
#if ENABLE_ECG_LOD
#include "ecg_helpers/ecg_lod.h"
#endif
#include "../cetiTag.h"

#include <pigpio.h>  // for I2C functions
#include <pthread.h> // to set CPU affinity
#include <sched.h>   // to set process priority
#include <unistd.h>  // for access() to check file existence

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ECG_MAX_FILE_SIZE_MB 1024 // Seems to log about 1GiB every 6.5 hours. Note that 2GB is the file size maximum for 32-bit systems

#define ECG_SAMPLE_TIMEOUT_US 100000               // Max time to wait for ADC or GPIO expander data to be ready before reconnecting the ECG electronics
#define ECG_ZEROCOUNT_THRESHOLD 100                // Max number of samples to tolerate consecutive 0s before reconnecting the ECG electronics
#define ECG_INVALID_PLACEHOLDER ((long)(-6666666)) // Do not expect large negative voltages (ADC readings are out of ~8e6)

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
int init_ecg_data_file();
void *ecg_thread_getData(void *paramPtr);
void *ecg_thread_writeData(void *paramPtr);

#endif // ECG_H
