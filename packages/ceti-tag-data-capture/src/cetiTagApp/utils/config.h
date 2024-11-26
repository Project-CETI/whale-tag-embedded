//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg
//-----------------------------------------------------------------------------
#ifndef CETI_CONFIG_H
#define CETI_CONFIG_H

#include "../aprs.h"
#include "../sensors/audio.h"
#include <stdint.h>
#include <time.h>

#define CONFIG_DEFAULT_AUDIO_SAMPLE_RATE AUDIO_SAMPLE_RATE_96KHZ
#define CONFIG_DEFAULT_AUDIO_BIT_DEPTH AUDIO_BIT_DEPTH_16
#define CONFIG_DEFAULT_AUDIO_FILTER_TYPE AUDIO_FILTER_WIDEBAND
#define CONFIG_DEFAULT_SURFACE_PRESSURE_BAR (0.3) // depth_m is roughly 10*pressure_bar
#define CONFIG_DEFAULT_DIVE_PRESSURE_BAR (0.5)    // depth_m is roughly 10*pressure_bar
#define CONFIG_DEFAULT_RELEASE_VOLTAGE_V (6.4)
#define CONFIG_DEFAULT_CRITICAL_VOLTAGE_V (6.2)
#define CONFIG_DEFAULT_TIMEOUT_S (4 * 24 * 60 * 60)
#define CONFIG_DEFAULT_BURN_INTERVAL_S (5 * 60)
#define CONFIG_DEFAULT_RECOVERY_ENABLED 0
#define CONFIG_DEFAULT_RECOVERY_FREQUENCY_MHZ 145.050
#define CONFIG_DEFAULT_RECOVERY_CALLSIGN "J75Y"
#define CONFIG_DEFAULT_RECOVERY_SSID 1
#define CONFIG_DEFAULT_RECOVERY_RECIPIENT_CALLSIGN "J75Y"
#define CONFIG_DEFAULT_RECOVERY_RECIPIENT_SSID 2

typedef enum config_error_e {
    CONFIG_OK = 0,
    CONFIG_ERR_UNKNOWN_KEY = 1,
    CONFIG_ERR_INVALID_VALUE = 2,
    CONFIG_ERR_MISSING_ASSIGN_OP = 3,
} ConfigError;

typedef struct tag_configuration {
    AudioConfig audio;
    float surface_pressure;
    float dive_pressure;
    float release_voltage_v;
    float critical_voltage_v;
    time_t timeout_s;
    struct {
        int valid;
        struct tm value;
    } tod_release;
    time_t burn_interval_s;
    struct {
        int enabled;
        APRSCallsign callsign;
        APRSCallsign recipient;
        float freq_MHz;
    } recovery;
} TagConfig;

extern TagConfig g_config;

int strtobool_s(const char *_String, const char **_EndPtr);
time_t strtotime_s(const char *_String, char **_EndPtr);
int config_read(const char *filename);
int config_parse_line(const char *_String);
void config_log(void);

#endif // CETI_CONFIG_H
