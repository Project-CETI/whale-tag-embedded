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
#define _GNU_SOURCE   // change how sched.h will be included

#include "launcher.h" // for g_exit, sampling rate, data filepath, and CPU affinity
#include "utils/logging.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update

#include <pigpio.h>
#include <unistd.h> // for usleep()
#include <string.h> // for memset() and other string functions
#include <pthread.h> // to set CPU affinity

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ADDR_MAINTAG_IOX 0x38 // NOTE also defined in burnwire.h
#define GPS_LOCATION_LENGTH (1024)
#define RCVRY_RP_nEN 0x01       //Recovery board controls
#define nRCVRY_SWARM_nEN 0x02
#define nRCVRY_VHF_nEN 0x04

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_recovery_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_recovery();
int testRecoverySerial(void);
int getGpsLocation(char* gpsLocation);
int recoveryOn(void);
int recoveryOff(void);
void* recovery_thread(void* paramPtr);

#endif // RECOVERY_H







