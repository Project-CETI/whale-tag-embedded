//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto, Michael Salino-Hugg,
//     [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#ifndef __CETI_WHALE_TAG_HAL_ECG_H__
#define __CETI_WHALE_TAG_HAL_ECG_H__

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "../../utils/error.h"  // for WTResult
#include "../../iox.h"

#include "../../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../../systemMonitor.h" // for the global CPU assignment variable to update
#include "../../utils/timing.h"  // for get_global_time_us

#include <stdint.h>             // for uint8_t
#include <pthread.h>
#include <unistd.h> // for usleep()

// ------------------------------------------
// Definitions/Configuration
// ------------------------------------------
#define ECG_LOD_READ_POLLING_PERIOD_US 1000

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_ecg_lod_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
void ecg_read_leadsOff(int* leadsOff_p, int* leadsOff_n);

// Threading methods
int ecg_lod_init(void);
void* ecg_lod_thread(void* paramPtr);

#endif // __CETI_WHALE_TAG_HAL_ECG_H__




