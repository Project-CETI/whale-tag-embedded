//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG.h
// Description: Control ECG-related acquisition and logging
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
#include "../launcher.h"     // for g_exit
#include "../systemMonitor.h" // for the global CPU assignment variable to update

#include <pigpio.h>   // for I2C functions
#include <sched.h>    // to set process priority and affinity
#include <sys/mman.h> // for locking memory (after setting priority/affinity)

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ECG_DATA_FILEPATH "/data/data_ecg.csv"
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





