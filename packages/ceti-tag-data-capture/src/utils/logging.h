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

#ifndef LOGGING_H
#define LOGGING_H

#include <syslog.h>
#include <stdio.h> // for FILE
#include <unistd.h> // for access() to check file existence
#include <string.h>

extern void init_logging();
#define CETI_LOG(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define CETI_logMsg(...) syslog(LOG_DEBUG, __VA_ARGS__)

extern int init_data_file(FILE* data_file, const char* data_filepath,
                          const char** data_file_headers, int num_data_file_headers,
                          char* data_file_notes, const char* log_tag);

#endif // LOGGING_H
