//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef RECOVERY_H
#define RECOVERY_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "aprs.h"

#include <time.h> //for time_t

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
typedef enum recovery_power_level_e {
	RECOVERY_POWER_LOW,
	RECOVERY_POWER_HIGH,
}RecoveryPowerLevel;

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_recovery_rx_thread_is_running;

//-----------------------------------------------------------------------------
// Helper Methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Hardware Methods
//-----------------------------------------------------------------------------
// initialize recovery board hardware
// int recovery_restart(void);
int recovery_ping(void);
int recovery_get_aprs_callsign(APRSCallsign *callsign);
int recovery_get_aprs_freq_mhz(float *p_freq_MHz);
int recovery_set_aprs_callsign(const APRSCallsign *callsign);
int recovery_set_aprs_freq_mhz(float f_MHz);
int recovery_set_aprs_message_recipient(const APRSCallsign *callsign);
int recovery_set_comment(const char *message);
int recovery_set_critical_voltage(float voltage);
int recovery_message(const char *message);
// int recovery_on(void);
// int recovery_off(void);
// int recovery_shutdown(void);
// void recovery_kill(void);
int recovery_ping(void);

//-----------------------------------------------------------------------------
// Thread Methods
//-----------------------------------------------------------------------------
int recovery_thread_init();
void* recovery_rx_thread(void* paramPtr);

#endif // RECOVERY_H







