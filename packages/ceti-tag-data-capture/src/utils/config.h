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

#include <stdint.h>
#include <time.h>
#include "../aprs.h"

#define CONFIG_DEFAULT_SURFACE_PRESSURE_BAR        (0.04)
#define CONFIG_DEFAULT_DIVE_PRESSURE_BAR           (0.10)
#define CONFIG_DEFAULT_RELEASE_VOLTAGE_V           (6.4)
#define CONFIG_DEFAULT_CRITICAL_VOLTAGE_V          (6.2)
#define CONFIG_DEFAULT_TIMEOUT_S                   (4*24*60*60)
#define CONFIG_DEFAULT_BURN_INTERVAL_S             (5*60)
#define CONFIG_DEFAULT_RECOVERY_ENABLED            0
#define CONFIG_DEFAULT_RECOVERY_FREQUENCY_MHZ      144.390
#define CONFIG_DEFAULT_RECOVERY_CALLSIGN           "J75Y"
#define CONFIG_DEFAULT_RECOVERY_SSID               1
#define CONFIG_DEFAULT_RECOVERY_RECIPIENT_CALLSIGN "J75Y"
#define CONFIG_DEFAULT_RECOVERY_RECIPIENT_SSID     2



typedef struct tag_configuration {
    float   surface_pressure;
    float   dive_pressure;
    float   release_voltage_v;
    float   critical_voltage_v;
    time_t  timeout_s;
    time_t  burn_interval_s;
    struct  {
        int enabled;
        APRSCallsign callsign;
        APRSCallsign recipient;
        float freq_MHz;
    } recovery;
} TagConfig;

extern TagConfig g_config;

time_t strtotime_s(const char *_String, char **_EndPtr);
int config_read(const char * filename);
int config_parse_line(const char *_String);
#endif //CETI_CONFIG_H
