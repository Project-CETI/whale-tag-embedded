
#ifndef BOARDTEMPERATURE_H
#define BOARDTEMPERATURE_H

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
#define BOARDTEMPERATURE_SAMPLING_PERIOD_US 1000000
#define BOARDTEMPERATURE_DATA_FILEPATH "/data/data_boardTemperature.csv"

#define ADDR_BOARDTEMPERATURE 0x4C

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_boardTemperature_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_boardTemperature();
int getBoardTemperature(int *pBoardTemp);
void* boardTemperature_thread(void* paramPtr);

#endif // BOARDTEMPERATURE_H







