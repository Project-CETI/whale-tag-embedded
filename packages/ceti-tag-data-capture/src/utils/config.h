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

typedef struct tag_configuration {
    float dive_pressure;
    float surface_pressure;
    float release_voltage_v;
    float critical_voltage_v;
    int64_t timeout_s;
    uint32_t burn_interval_s;
    //recovery enable ???
} TagConfig;

#extern TagConfig g_tag_config;

#endif //CETI_CONFIG_H
