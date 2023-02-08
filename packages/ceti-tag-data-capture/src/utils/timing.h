
#ifndef TIMING_H
#define TIMING_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "logging.h"
#include <pigpio.h>
#include <sys/time.h>

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------
#define ADDR_RTC 0x68

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_timing();
int getRtcCount();
int resetRtcCount();
unsigned int getTimeDeploy(void);
long long get_global_time_us();
long long get_global_time_ms();

#endif // TIMING_H