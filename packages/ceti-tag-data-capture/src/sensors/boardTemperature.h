//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef BOARDTEMPERATURE_H
#define BOARDTEMPERATURE_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE // change how sched.h will be included

#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"

#include <pigpio.h>
#include <pthread.h> // to set CPU affinity

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ADDR_BOARDTEMPERATURE 0x4C

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_boardTemperature_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_boardTemperature();
int getBoardTemperature(int *pBoardTemp);
int getBatteryTemperature(int *pBattTemp);
int getTemperatures(int *pBoardTemp, int *pBattTemp);
int resetBattTempFlags();
void* boardTemperature_thread(void* paramPtr);

#endif // BOARDTEMPERATURE_H
