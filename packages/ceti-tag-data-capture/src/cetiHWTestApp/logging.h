//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef LOGGING_H
#define LOGGING_H

#include <syslog.h>

#define CETI_LOG(FMT_STR, ...) syslog(LOG_DEBUG, ("%s(): " FMT_STR), __FUNCTION__ __VA_OPT__(,) __VA_ARGS__)
#define CETI_WARN(FMT_STR, ...) CETI_LOG("[WARN]: " FMT_STR __VA_OPT__(,) __VA_ARGS__)
#define CETI_ERR(FMT_STR, ...) CETI_LOG("[ERROR]: " FMT_STR __VA_OPT__(,) __VA_ARGS__)
#ifdef DEBUG
    #define CETI_DEBUG(...) CETI_LOG(__VA_ARGS__)
#else
    #define CETI_DEBUG(...) {}
#endif
#define CETI_logMsg(...) syslog(LOG_DEBUG, __VA_ARGS__)

#endif // LOGGING_H
