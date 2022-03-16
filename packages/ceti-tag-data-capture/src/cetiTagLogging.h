//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    Description
//  0.00    10/08/21   Begin work, establish framework
//  1.00    03/05/22   Switch to using linux syslog
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Description: Logging system for the project
//-----------------------------------------------------------------------------

#ifndef CETI_LOG_H
#define CETI_LOG_H

#include <syslog.h>

extern void CETI_initializeLog();
#define CETI_LOG(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define CETI_logMsg(...) syslog(LOG_DEBUG, __VA_ARGS__)

#endif
