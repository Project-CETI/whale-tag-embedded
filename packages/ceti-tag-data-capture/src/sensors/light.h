
#ifndef LIGHT_H
#define LIGHT_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "../utils/logging.h"
#include "../launcher.h" // for g_exit
#include "../systemMonitor.h" // for the global CPU assignment variable to update

#include <pigpio.h>
#include <sched.h> // to get the current CPU assigned to the thread

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define LIGHT_SAMPLING_PERIOD_US 1000000
#define LIGHT_DATA_FILEPATH "/data/data_light.csv"
#define ADDR_LIGHT 0x29

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_light();
int getAmbientLight(int* pAmbientLightVisible, int* pAmbientLightIR);
void* light_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_light_thread_is_running;

#endif // LIGHT_H







