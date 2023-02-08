//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
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
