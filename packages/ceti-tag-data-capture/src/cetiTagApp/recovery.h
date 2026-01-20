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
#include "cetiRecovery.h"
#include "utils/config.h" //for TagConfig
#include "utils/error.h"

#include <time.h> //for time_t

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define RECOVERY_BOARD_TYPE_APRS 0
#define RECOVERY_BOARD_TYPE_ARGOS 1

#define RECOVERY_BOARD_TYPE RECOVERY_BOARD_TYPE_ARGOS

typedef enum recovery_power_level_e {
    RECOVERY_POWER_LOW,
    RECOVERY_POWER_HIGH,
} RecoveryPowerLevel;

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
WTResult wt_recovery_init(void);

//-----------------------------------------------------------------------------
// Control Methods
//-----------------------------------------------------------------------------
// initialize recovery board hardware
// int recovery_restart(void);
int recovery_ping(void);
#if RECOVERY_BOARD_TYPE_APRS == RECOVERY_BOARD_TYPE
int recovery_get_aprs_callsign(APRSCallsign *callsign);
int recovery_get_aprs_freq_mhz(float *p_freq_MHz);
int recovery_set_aprs_callsign(const APRSCallsign *callsign);
int recovery_set_aprs_freq_mhz(float f_MHz);
int recovery_set_aprs_message_recipient(const APRSCallsign *callsign);
int recovery_set_aprs_comment(const char *message);
#elif RECOVERY_BOARD_TYPE_ARGOS == RECOVERY_BOARD_TYPE
int recovery_get_argos_address(char address[static 9]);
int recovery_get_argos_id(char address[static 7]);
int recovery_get_argos_modulation(RecoveryArgoModulation *mod_scheme);
int recovery_get_argos_secret_key(char secret_key[static 32]);

int recovery_set_argos_address(const char *address, size_t address_len);
int recovery_set_argos_id(const char *id, size_t id_len);
int recovery_set_argos_modulation(RecoveryArgoModulation mod_scheme);
int recovery_set_argos_secret_key(const char *secret_key, size_t secret_key_len);
#endif
int recovery_gps_only(void);
int recovery_message(const char *message);
int recovery_off(void);
int recovery_on(void);
int recovery_set_critical_voltage(float voltage);
int recovery_sleep(void);
WTResult recovery_sync_time(void);
int recovery_wake(void);

//-----------------------------------------------------------------------------
// Thread Methods
//-----------------------------------------------------------------------------
int recovery_thread_init(TagConfig *pConfig);
void *recovery_rx_thread(void *paramPtr);

#endif // RECOVERY_H
