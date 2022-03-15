//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    Description
//  0.00    10/08/21   Begin work, establish framework
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagLogging.c
// Description: Logging system for the project
//-----------------------------------------------------------------------------

#ifndef CETI_LOG_H
#define CETI_LOG_H

//***********************************************************************************
//                                      defines
//***********************************************************************************
#define LOG_MSG_SIZE 25000
#define MAX_LOG_FILE_SIZE (1024 * 1024 * 10)
#define MAX_LOG_FILES 100

#define CETI_LOG_NO_TIME(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, ERROR, NO_TIMESTAMP, __VA_ARGS__)
#define CETI_LOG(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, ERROR, TIMESTAMP, __VA_ARGS__)
#define CETI_ERR(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, ERROR, TIMESTAMP, __VA_ARGS__)
#define ERR_NO_TIME(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, ERROR, NO_TIMESTAMP, __VA_ARGS__)
#define DEBUG_LOW(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, DEBUG_LOW, TIMESTAMP, __VA_ARGS__)
#define DEBUG_LOW_NO_TIME(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, DEBUG_LOW, NO_TIMESTAMP, __VA_ARGS__)
#define DEBUG_MEDIUM(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, DEBUG_MEDIUM, TIMESTAMP, __VA_ARGS__)
#define DEBUG_MEDIUM_NO_TIME(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, DEBUG_MEDIUM, NO_TIMESTAMP, __VA_ARGS__)
#define DEBUG_HIGH(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, DEBUG_HIGH, TIMESTAMP, __VA_ARGS__)
#define DEBUG_HIGH_NO_TIME(...) \
  CETI_logMsg(__FILE__, __FUNCTION__, DEBUG_HIGH, NO_TIMESTAMP, __VA_ARGS__)

enum logLevelEnum { ERROR, DEBUG_LOW, DEBUG_MEDIUM, DEBUG_HIGH };

enum logTimestampEnum { NO_TIMESTAMP, TIMESTAMP };

//***********************************************************************************
//                               function declarations
//***********************************************************************************

extern void CETI_initializeLog(const char *_logFileName);
extern void CETI_logMsg(const char *file, const char *function,
                        enum logLevelEnum logLevel,
                        enum logTimestampEnum timeOption, const char *msg, ...);

#endif
