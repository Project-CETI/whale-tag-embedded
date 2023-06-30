//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef LAUNCHER_H
#define LAUNCHER_H

//-----------------------------------------------------------------------------
// Select which components to enable.
//-----------------------------------------------------------------------------
#define ENABLE_FPGA 1 // for audio acquisition AND for safe power-down sequences
#define ENABLE_RTC 1
#define ENABLE_BATTERY_GAUGE 1
#define ENABLE_AUDIO 1
#define ENABLE_AUDIO_FLAC 1
#define ENABLE_ECG 1
#define ENABLE_IMU 1
#define ENABLE_LIGHT_SENSOR 1
#define ENABLE_BOARDTEMPERATURE_SENSOR 1
#define ENABLE_PRESSURETEMPERATURE_SENSOR 1
#define ENABLE_RECOVERY 1
#define ENABLE_SYSTEMMONITOR 1
#define ENABLE_BURNWIRE 1
#define ENABLE_GOPROS 0

//-----------------------------------------------------------------------------
// Sampling and logging configuration.
//-----------------------------------------------------------------------------
#define BATTERY_SAMPLING_PERIOD_US 1000000
#define IMU_SAMPLING_PERIOD_QUAT_US  50000 // rate for the computed orientation
#define IMU_SAMPLING_PERIOD_9DOF_US  20000 // rate for the accelerometer/gyroscope/magnetometer
#define LIGHT_SAMPLING_PERIOD_US 1000000
#define PRESSURETEMPERATURE_SAMPLING_PERIOD_US 1000000
#define BOARDTEMPERATURE_SAMPLING_PERIOD_US 1000000
#define RECOVERY_SAMPLING_PERIOD_US 5000000
#define RTC_UPDATE_PERIOD_LONG_US 950000 // A coarse delay, to get close to when the RTC is expected to change
#define RTC_UPDATE_PERIOD_SHORT_US 10000 // A finer delay, to find the new RTC value close to its update time
#define COMMAND_POLLING_PERIOD_US 1000
#define STATEMACHINE_UPDATE_PERIOD_US 1000000
#define SYSTEMMONITOR_SAMPLING_PERIOD_US 5000000

// Setting a CPU to -1 will allow the system to decide (the thread will not set an affinity)
#define AUDIO_SPI_CPU 3
#define AUDIO_WRITEDATA_CPU 0
#define ECG_GETDATA_CPU 2
#define ECG_WRITEDATA_CPU 1
#define BATTERY_CPU 1
#define IMU_CPU 1
#define LIGHT_CPU 1
#define PRESSURETEMPERATURE_CPU 1
#define BOARDTEMPERATURE_CPU 1
#define RECOVERY_CPU 1
#define RTC_CPU 1
#define COMMAND_CPU 0
#define STATEMACHINE_CPU 0
#define SYSTEMMONITOR_CPU 0
#define GOPROS_CPU ECG_GETDATA_CPU // Note that the wearable tag will not use ECG

#define ECG_DATA_FILEPATH_BASE "/data/data_ecg" // will append a counter and create new files according to a maximum size
#define BATTERY_DATA_FILEPATH "/data/data_battery.csv"
#define IMU_DATA_FILEPATH_BASE "/data/data_imu" // will append a counter and create new files according to a maximum size
#define LIGHT_DATA_FILEPATH "/data/data_light.csv"
#define PRESSURETEMPERATURE_DATA_FILEPATH "/data/data_pressure_temperature.csv"
#define BOARDTEMPERATURE_DATA_FILEPATH "/data/data_boardTemperature.csv"
#define RECOVERY_DATA_FILEPATH "/data/data_gps.csv"
#define STATEMACHINE_DATA_FILEPATH "/data/data_state.csv"
#define SYSTEMMONITOR_DATA_FILEPATH "/data/data_systemMonitor.csv"
#define GOPROS_DATA_FILEPATH "/data/data_goPros.csv"

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

#if ENABLE_GOPROS
#include "sensors/goPros.h"
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



