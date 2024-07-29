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


#include "launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/logging.h"
#include "max17320.h"
#include "cetiTag.h" //for CetiBatterySample
//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
// Set to 0 if DS2778 is used
#define MAX17320 1

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

#define BATT_CTL_VAL 0X8E //SETS UV CUTOFF TO 2.6V
#define BATT_OV_VAL 0x5A  //SETS OV CUTOFF TO 4.2V

#define MIN_CHARGE_TEMP (10)
#define MAX_CHARGE_TEMP (40)
#define MIN_DISCHARGE_TEMP (0)
#define MAX_DISCHARGE_TEMP (50)

#define DE 0x01    //BIT 0 set  discharge enable
#define CE 0x02    //BIT 1 set  charge enable

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_battery();
int getBatteryData(double* battery_v1_v, double* battery_v2_v, double* battery_i_mA);
int enableCharging(void);
int enableDischarging(void);
int disableCharging(void);
int disableDischarging(void);
void* battery_thread(void* paramPtr);
int resetBattTempFlags(void);


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_battery_thread_is_running;
// Store global versions of the latest readings since the state machine will use them.
extern CetiBatterySample *g_battery;
#endif // BATTERY_H
