//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef BATTERY_H
#define BATTERY_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "launcher.h" // for g_exit, sampling rate, data filepath, and CPU affinity
#include "utils/logging.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update

#include <pigpio.h>
#include <pthread.h> // to set CPU affinity

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ADDR_BATT_GAUGE 0x59

// DS2778 Gas Gauge Registers and Settings
#define BATT_PROTECT 0x00
#define BATT_CTL 0x60
#define BATT_OVER_VOLTAGE 0x7F
#define BATT_CELL_1_V_MS 0x0C
#define BATT_CELL_1_V_LS 0x0D
#define BATT_CELL_2_V_MS 0x1C
#define BATT_CELL_2_V_LS 0x1D
#define BATT_I_MS 0x0E
#define BATT_I_LS 0x0F
#define BATT_R_SENSE 0.025

#define BATT_CTL_VAL 0X8E  //SETS UV CUTOFF TO 2.6V
#define BATT_OV_VAL 0x5A //SETS OV CUTOFF TO 4.2V

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_battery();
int getBatteryData(double* battery_v1_v, double* battery_v2_v, double* battery_i_mA);
void* battery_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_battery_thread_is_running;
// Store global versions of the latest readings since the state machine will use them.
extern double g_latest_battery_v1_v;
extern double g_latest_battery_v2_v;
extern double g_latest_battery_i_mA;

#endif // BATTERY_H






