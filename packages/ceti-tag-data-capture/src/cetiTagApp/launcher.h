//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
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
#define ENABLE_ECG_LOD 1 // will be implicitly disabled if ENABLE_ECG is 0
#define ENABLE_IMU 1
#define ENABLE_LIGHT_SENSOR 1
#define ENABLE_PRESSURETEMPERATURE_SENSOR 1
#define ENABLE_RECOVERY 1
#define ENABLE_SYSTEMMONITOR 1
#define ENABLE_BURNWIRE 1
#define ENABLE_RUNTIME_AUDIO 0
#define ENABLE_VIDEO 1

//-----------------------------------------------------------------------------
// State Change Configurations
//-----------------------------------------------------------------------------
#define MIN_DATA_PARTITION_FREE_KB (1 * 1024 * 1024) // 1 GiB

//-----------------------------------------------------------------------------
// Sampling and logging configuration.
//-----------------------------------------------------------------------------
#define RTC_UPDATE_PERIOD_LONG_US 950000 // A coarse delay, to get close to when the RTC is expected to change
#define RTC_UPDATE_PERIOD_SHORT_US 10000 // A finer delay, to find the new RTC value close to its update time
#define COMMAND_POLLING_PERIOD_US 100000
#define STATEMACHINE_UPDATE_PERIOD_US 1000000
#define SYSTEMMONITOR_SAMPLING_PERIOD_US 10000000

// Setting a CPU to -1 will allow the system to decide (the thread will not set an affinity)
#define AUDIO_SPI_CPU 3
#define AUDIO_WRITEDATA_CPU 0
#define ECG_GETDATA_CPU 2
#define ECG_WRITEDATA_CPU 1
#define ECG_LOD_CPU 1
#define BATTERY_CPU 1
#define IMU_CPU 1
#define LIGHT_CPU 1
#define PRESSURETEMPERATURE_CPU 1
#define RECOVERY_RX_CPU 1
#define RTC_CPU 1
#define COMMAND_CPU 0
#define STATEMACHINE_CPU 0
#define SYSTEMMONITOR_CPU 0

#define ECG_DATA_FILEPATH_BASE "/data/data_ecg" // will append a counter and create new files according to a maximum size
#define BATTERY_DATA_FILEPATH "/data/data_battery.csv"
#define IMU_DATA_FILEPATH_BASE "/data/data_imu" // will append a counter and create new files according to a maximum size
#define LIGHT_DATA_FILEPATH "/data/data_light.csv"
#define PRESSURETEMPERATURE_DATA_FILEPATH "/data/data_pressure_temperature.csv"
#define AUDIO_STATUS_FILEPATH "/data/data_audio_status.csv"
#define RECOVERY_DATA_FILEPATH "/data/data_gps.csv"
#define STATEMACHINE_DATA_FILEPATH "/data/data_state.csv"
#define STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH "/data/burnwire_timeout_start_time_s.csv"
#define SYSTEMMONITOR_DATA_FILEPATH "/data/data_systemMonitor.csv"

#define CETI_CONFIG_FILE "../config/ceti-config.txt"              // This is non-volatile config file's relative path
#define CETI_CONFIG_OVERWRITE_FILE "/data/config/ceti-config.txt" // This is the config file in /data/config

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_exit;
extern int g_stopAcquisition;
extern int g_stopLogging;
extern char g_process_path[256];

//-----------------------------------------------------------------------------
// Helper methods
//-----------------------------------------------------------------------------
int init_tag();

#endif // LAUNCHER_H
