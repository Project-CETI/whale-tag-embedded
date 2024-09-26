//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef BATTERY_H
#define BATTERY_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "cetiTag.h" //for CetiBatterySample

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define MIN_CHARGE_TEMP (10)
#define MAX_CHARGE_TEMP (40)
#define MIN_DISCHARGE_TEMP (0)
#define MAX_DISCHARGE_TEMP (50)

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_battery();
void* battery_thread(void* paramPtr);
int resetBattTempFlags(void);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_battery_thread_is_running;
// Store global versions of the latest readings since the state machine will use them.
extern CetiBatterySample *shm_battery;

#endif // BATTERY_H
