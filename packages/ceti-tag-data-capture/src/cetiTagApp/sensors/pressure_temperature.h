//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef PRESSURETEMPERATURE_H
#define PRESSURETEMPERATURE_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "../cetiTag.h" //for cetiPressureSample

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_pressureTemperature(void);
void *pressureTemperature_thread(void *paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_pressureTemperature_thread_is_running;
// Store global versions of the latest readings since the state machine will use them.
extern CetiPressureSample *g_pressure;
#endif // PRESSURETEMPERATURE_H
