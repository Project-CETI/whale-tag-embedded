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
#define _GNU_SOURCE // change how sched.h will be included

#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"

#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <stdint.h>
#include <unistd.h> // for usleep()

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ADDR_PRESSURETEMPERATURE 0x40
// Keller 4LD Pressure Sensor 200 bar
// Reference pressure is a 1 bar abs
#define PRESSURE_MIN 0   // bar
#define PRESSURE_MAX 200 // bar
#define PRESSURE_ZERO_AT_STARTUP 0 // whether to take a reading at startup and subtract that from all future readings

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_pressureTemperature();
int getPressureTemperature(double *pressure_bar, double *temperature_c);
int pressure_get_calibrated(double *pressure_bar, double * temperature_c);
void *pressureTemperature_thread(void *paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_pressureTemperature_thread_is_running;
// Store global versions of the latest readings since the state machine will use them.
extern double g_latest_pressureTemperature_pressure_bar;
extern double g_latest_pressureTemperature_temperature_c;

#endif // PRESSURETEMPERATURE_H
