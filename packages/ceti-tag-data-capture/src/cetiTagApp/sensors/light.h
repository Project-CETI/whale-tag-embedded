//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef LIGHT_H
#define LIGHT_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"

#include <pigpio.h>
#include <pthread.h> // to set CPU affinity

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ADDR_LIGHT 0x29

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_light();
int getAmbientLight(int *pAmbientLightVisible, int *pAmbientLightIR);
void *light_thread(void *paramPtr);
int light_verify(void);
int light_wake(void);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_light_thread_is_running;

#endif // LIGHT_H
