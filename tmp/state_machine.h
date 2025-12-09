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
typedef enum {          // Tag operational states for deployment sequencing
    ST_START = 0,       // turn on the audio recorder, illuminate ready LED
    ST_RECORD_DIVING,   // recording while underwater
    ST_RECORD_FLOATING, // recording but likely that the tag has detatched for the whale
    ST_RECORD_SURFACE,  // recording while surfaced - trying for a GPS fix
    ST_BRN_ON,          // burnwire is on, may or may not be at the surface when in this state
    ST_LOW_POWER_BURN,  // burnwire with sonsors disabled, but recovery on, transitions directly into shutdown on completion
    ST_RETRIEVE,        // burnwire timed out, monitor GPS and transmit coord if enough battery
    ST_SHUTDOWN,        // battery critical, put system in minimum power mode
    ST_UNKNOWN
} wt_state_t;

#define WIFI_GRACE_PERIOD_MIN 5
#define FLOAT_DETECTION 1
#define MISSION_BMS_CONSECUTIVE_ERROR_THRESHOLD 5
#define BATTERY_LOW_VOLTAGE_CONSECUTIVE_THRESHOLD 10
#define BATTERY_CRITICAL_VOLTAGE_CONSECUTIVE_THRESHOLD 10

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

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
