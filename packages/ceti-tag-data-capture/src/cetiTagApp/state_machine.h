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

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define FORCE_NETWORKS_OFF_ON_START 0 // turn off networks regardless of dive status

typedef enum {         // Tag operational states for deployment sequencing
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
#define MISSION_BMS_CONSECUTIVE_ERROR_THRESHOLD 5

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_stateMachine_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_stateMachine();
int updateStateMachine();
wt_state_t stateMachine_get_state(void);
void stateMachine_pause(void);
void stateMachine_resume(void);
int stateMachine_set_state(wt_state_t new_state);
const char *get_state_str(wt_state_t state);
wt_state_t strtomissionstate(const char *_String, const char **_EndPtr);
void *stateMachine_thread(void *paramPtr);

#endif // STATE_MACHINE_H
