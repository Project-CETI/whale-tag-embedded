//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE // change how sched.h will be included

#include "battery.h"
#include "burnwire.h"
#include "launcher.h" // for g_exit, g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "recovery.h"
#include "sensors/pressure_temperature.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/logging.h"
#include "utils/power.h"
#include "utils/timing.h"

#include <pthread.h> // to set CPU affinity
#include <stdio.h>   // for FILE
#include <stdlib.h>  // for atof, atol, etc
#include <unistd.h>  // gethostname

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define FORCE_NETWORKS_OFF_ON_START 0 // turn off networks regardless of dive status
#define CETI_CONFIG_FILE "../config/ceti-config.txt"

typedef enum {       // Tag operational states for deployment sequencing
  ST_CONFIG = 0,     // get the deployment parameters from config file
  ST_START,          // turn on the audio recorder, illuminate ready LED
  ST_DEPLOY,         // wait for whale to dive
  ST_RECORD_DIVING,  // recording while underwater
  ST_RECORD_SURFACE, // recording while surfaced - trying for a GPS fix
  ST_BRN_ON,         // burnwire is on, may or may not be at the surface when in this state
  ST_RETRIEVE,       // burnwire timed out, likely at surface, monitor GPS and transmit coord if enough battery
  ST_SHUTDOWN,       // battery critical, put system in minimum power mode
  ST_UNKNOWN
} wt_state_t;

#define MAX_STATE_STRING_LEN (32)
static const char state_str[][MAX_STATE_STRING_LEN] = {
    "CONFIG", "START", "DEPLOY", "RECORD_DIVING", "RECORD_SURFACE",
    "BRN_ON", "RETRIEVE", "SHUTDOWN", "UNKNOWN"};

#define WIFI_GRACE_PERIOD_MIN 10

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_stateMachine_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_stateMachine();
int updateStateMachine();
const char *get_state_str(wt_state_t state);
void *stateMachine_thread(void *paramPtr);

#endif // STATE_MACHINE_H
