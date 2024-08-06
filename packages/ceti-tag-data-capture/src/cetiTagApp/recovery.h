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
#define GPS_LOCATION_LENGTH (1024)
#define RCVRY_RP_nEN 0x01       //Recovery board controls
#define nRCVRY_SWARM_nEN 0x02
#define nRCVRY_VHF_nEN 0x04

typedef enum recovery_power_level_e {
	RECOVERY_POWER_LOW,
	RECOVERY_POWER_HIGH,
}RecoveryPowerLevel;

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_recovery_thread_is_running;

//-----------------------------------------------------------------------------
// Helper Methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Hardware Methods
//-----------------------------------------------------------------------------
// initialize recovery board hardware
int recovery_init(void);
int recovery_restart(void);
int recovery_get_aprs_call_sign(char buffer[static 7]);
int recovery_get_aprs_callsign(APRSCallsign *callsign);
int recovery_get_aprs_freq_mhz(float *p_freq_MHz);
int recovery_get_gps_data(char gpsLocation[static GPS_LOCATION_LENGTH], time_t timeout_us);
int recovery_set_aprs_callsign(const APRSCallsign *callsign);
int recovery_set_aprs_freq_mhz(float f_MHz);
int recovery_set_aprs_message_recipient(const APRSCallsign *callsign);
int recovery_set_comment(const char *message);
int recovery_set_critical_voltage(float voltage);
int recovery_message(const char *message);
int recovery_on(void);
int recovery_off(void);
int recovery_shutdown(void);
void recovery_kill(void);

//-----------------------------------------------------------------------------
// Thread Methods
//-----------------------------------------------------------------------------
int recovery_thread_init();
void* recovery_thread(void* paramPtr);
void *recovery_gps_test_thread(void *paramPtr);

#endif // RECOVERY_H







