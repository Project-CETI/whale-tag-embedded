//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "utils/logging.h"
#include "launcher.h" // for g_exit, sampling rate, data filepath, and CPU affinity
#include "sys/types.h"
#include "sys/sysinfo.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include <pthread.h> // to set CPU affinity

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define NUM_CPU_ENTRIES 5 // overall, then 4 cores
#define TID_PRINT_PERIOD_US 60000000 // How often to print thread IDs; -1 to never print

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_systemMonitor();
long long get_virtual_memory_total();
long long get_virtual_memory_used();
long long get_swap_total();
long long get_swap_free();
long long get_ram_total();
long long get_ram_free();
long long get_ram_used();
int update_cpu_usage();
int get_cpu_id_for_tid(int tid);
float get_cpu_temperature_c();
float get_gpu_temperature_c();
int system_call_with_output(char* cmd, char* result);
void* systemMonitor_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_systemMonitor_thread_is_running;
extern int g_audio_thread_spi_tid;
extern int g_audio_thread_writeData_tid;
extern int g_ecg_thread_getData_tid;
extern int g_ecg_thread_writeData_tid;
extern int g_imu_thread_tid;
extern int g_light_thread_tid;
extern int g_pressureTemperature_thread_tid;
extern int g_boardTemperature_thread_tid;
extern int g_battery_thread_tid;
extern int g_recovery_thread_tid;
extern int g_stateMachine_thread_tid;
extern int g_command_thread_tid;
extern int g_rtc_thread_tid;
extern int g_systemMonitor_thread_tid;
extern int g_goPros_thread_tid;

#endif // SYSTEMMONITOR_H







