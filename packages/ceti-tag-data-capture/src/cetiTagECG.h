//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG.h
// Description: Control ECG-related acquisition and logging
//-----------------------------------------------------------------------------

#ifndef CETI_ECG_H
#define CETI_ECG_H

#include <pigpio.h>   // for I2C functions
#include <sys/time.h> // for gettimeofday
#define _GNU_SOURCE   // change how sched.h is included
#include <sched.h>    // to set process priority and affinity
#include <sys/mman.h> // for locking memory (after setting priority/affinity)

#include "cetiTagLogging.h"
#include "cetiTagWrapper.h"

#include "cetiTagECG_helpers/cetiTagECG_gpio_expander.h"
#include "cetiTagECG_helpers/cetiTagECG_adc.h"
#include "cetiTagECG_helpers/cetiTagECG_logging.h"

//--------------------------------------------
// Configuration constants
//--------------------------------------------
#define ECG_I2C_BUS 0x01


#endif // CETI_ECG_H





