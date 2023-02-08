//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagWrapper.h
// Description: The Ceti Tag application top level header file
//-----------------------------------------------------------------------------

#ifndef LAUNCHER_H
#define LAUNCHER_H

//-----------------------------------------------------------------------------
// Select which components to enable.
//-----------------------------------------------------------------------------
#define ENABLE_FPGA 1 // for audio acquisition AND for safe power-down sequences
#define ENABLE_BATTERY_GAUGE 0
#define ENABLE_AUDIO 1
#define ENABLE_ECG 0
#define ENABLE_IMU 0
#define ENABLE_LIGHT_SENSOR 0
#define ENABLE_BOARDTEMPERATURE_SENSOR 0
#define ENABLE_PRESSURETEMPERATURE_SENSOR 0
#define ENABLE_RECOVERY 0
#define ENABLE_SYSTEMMONITOR 1
#define ENABLE_BURNWIRE 0

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <pigpio.h>
#include "utils/logging.h"
#include "utils/timing.h"
#include "commands.h"
#include "state_machine.h"

#if ENABLE_BATTERY_GAUGE
#include "battery.h"
#endif

#if ENABLE_BURNWIRE
#include "burnwire.h"
#endif

#if ENABLE_FPGA
#include "fpga.h"
#endif

#if ENABLE_RECOVERY
#include "recovery.h"
#endif

#if ENABLE_AUDIO
#include "sensors/audio.h"
#endif

#if ENABLE_IMU
#include "sensors/imu.h"
#endif

#if ENABLE_LIGHT_SENSOR
#include "sensors/light.h"
#endif

#if ENABLE_PRESSURETEMPERATURE_SENSOR
#include "sensors/pressure_temperature.h"
#endif

#if ENABLE_BOARDTEMPERATURE_SENSOR
#include "sensors/boardTemperature.h"
#endif

#if ENABLE_ECG
#include "sensors/ecg.h"
#endif

#if ENABLE_SYSTEMMONITOR
#include "systemMonitor.h"
#endif

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_exit;

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------
int init_tag();

#endif // LAUNCHER_H



