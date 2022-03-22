//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
//  Version Date       Description
//  0.00    10/08/21   Begin work, establish framework
//  1.00    03/05/22   Switch to using syslog
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagLogging.c
// Description: Logging system for the project
//-----------------------------------------------------------------------------

#include "cetiTagLogging.h"

#include "cetiTagWrapper.h"

void CETI_initializeLog() {
  openlog("CETI data capture", LOG_PERROR | LOG_CONS, LOG_USER);
  syslog(LOG_DEBUG, "************************************************");
  syslog(LOG_DEBUG, "            CETI Tag Electronics                ");
  syslog(LOG_DEBUG, " -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  syslog(LOG_DEBUG, "    Version %s\n", CETI_VERSION);
  syslog(LOG_DEBUG, "    Build date: %s, %s\n", __DATE__, __TIME__);
  syslog(LOG_DEBUG, "*************************************************");
}
