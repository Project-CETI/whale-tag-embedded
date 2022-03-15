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

#include "cetiTagLogging.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <syslog.h>
#include <time.h>

#include "cetiTagWrapper.h"

//***********************************************************************************
//                            static function declarations
//***********************************************************************************
static void createNewLog();
static void writeToFile(const char *msg);
static void getTimestampString(char *timestampLogStr);

//***********************************************************************************
//                               Global variables
//***********************************************************************************
#define LOG_FILENAME_LEN (100)
static FILE *logFilePtr;
static char logFileName[LOG_FILENAME_LEN];
// enum logLevelEnum currentLogLevel = DEBUG_MEDIUM;
static enum logLevelEnum currentLogLevel = ERROR;
// length of the currently active log file
static unsigned long logFileLength;
static pthread_mutex_t logMutex;

/***********************************************************************************

  Name:     initializeLog

  Notes:    Opens the log file and initializes any required parameters.

  Parameters:
        _logFileName (i): Name of the log file

  Returns:  N/A

  Revision History:

        PTM     05/07/12    Created

 ***********************************************************************************/
void CETI_initializeLog(const char *_logFileName) {
  // printf("CETI_initializeLog: Starting the Log\n");
  time_t timeStamp;  //  struct timeval timeStamp;
  char bannerStr[1000];

  //  Initialize mutex
  pthread_mutex_init(&logMutex, NULL);

  // Save the Log file name
  strcpy(logFileName, _logFileName);

  // Open log file
  // printf("Attempt to open the log\n");
  logFilePtr = fopen(logFileName, "a");

  if (logFilePtr == NULL) {
    printf("CETI_initializeLog: Failed to start the Log\n");
    //   syslog(LOG_CRIT, "Unable to open CETI log file: %s\n",
    //          logFileName);
  }

  // --------------------------------
  // Get file length
  // --------------------------------

  // file pointer is positioned at the end of the file
  // because we opened it in append mode
  // printf("check length\n");
  logFileLength = ftell(logFilePtr);
  // printf("write banner\n");
  //
  //   Put Banner header at the beginning of the file
  //
  int len = sprintf(bannerStr,
                    "\n*************************************************\n");
  len += sprintf(bannerStr + len,
                 "            CETI Tag Electronics                 \n");
  len += sprintf(bannerStr + len,
                 "                                                 \n");
  len += sprintf(bannerStr + len,
                 " -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- \n");
  len += sprintf(bannerStr + len, "    Version %s\n", CETI_VERSION);
  len +=
      sprintf(bannerStr + len, "    Build date: %s, %s\n", __DATE__, __TIME__);
  len += sprintf(bannerStr + len,
                 "*************************************************\n");

  time(&timeStamp);
  len += sprintf(bannerStr + len, "%s", ctime(&timeStamp));

  writeToFile(bannerStr);

  fclose(logFilePtr);
}

/***********************************************************************************

  Name:     logMsg

  Notes:    This function will selectively write to the log based on the input
            logLevel.

  Parameters:
      location (i):   A string populated by compiler macros which specifies
                      the calling file and function
      logLevel (i):   the log level at which this message should be printed.
                      ERROR ... DEBUG_HIGH
      timeOption (i): specifies whether or not to print a timestamp before the
                      message
      msg (i):        format specifier for the message.  This is the same as
                      used by the printf() function and will be passed directly
                      to the vsprintf() function to do the actual message
                      printing.
      ... (i):        variable list of arguments to be printed in the message.
                      This will be passed into the vsprintf() function.

  Returns:  N/A

  Revision History:

        PTM     05/07/12    Created

 ***********************************************************************************/
void CETI_logMsg(const char *file, const char *function,
                 enum logLevelEnum logLevel, enum logTimestampEnum timeOption,
                 const char *msg, ...) {
  char timeStampString[100] = "";
  char formattedMessage[LOG_MSG_SIZE];
  va_list args;
  unsigned count = 0;

  // if the message should be logged because it's beneath the current
  // threshold...
  if (logLevel <= currentLogLevel) {
    va_start(args, msg);

    // if this message is supposed to have a timestamp...
    if (timeOption == TIMESTAMP) {
      // get the timestamp
      getTimestampString(timeStampString);
    }

    // if the message is too big...
    if (strlen(msg) > (LOG_MSG_SIZE - 100)) {
      // Too big.  Don't print a message
      sprintf(formattedMessage, "%s   %s:%s:%s, %d characters", timeStampString,
              file, function, "Message too Large to Log", strlen(msg));
    } else {
      if (timeOption == TIMESTAMP) {
        // print timestamp and thread name
        count = sprintf(formattedMessage, "\n%s   %s : %s()\n", timeStampString,
                        file, function);
      }

      // print the log string
      vsprintf(&formattedMessage[count], msg, args);
    }

    // Open log file
    fopen(logFileName, "a");
    writeToFile(formattedMessage);
    fclose(logFilePtr);

    va_end(args);
  }
}

/***********************************************************************************

  Name:     writeToFile

  Notes:    This function will write the formatted message into the log file.

  Parameters:
        msg (i): string to be written to the log file

  Returns:  N/A

  Revision History:

        PTM     05/07/12    Created

 ***********************************************************************************/
static void writeToFile(const char *msg) {
  unsigned msgLen;

  pthread_mutex_lock(&logMutex);

  // calculate new log file length and write message to log file
  msgLen = strlen(msg);
  logFileLength += msgLen;
  fwrite(msg, 1, msgLen, logFilePtr);
  fwrite("\n", 1, 1, logFilePtr);

  // if file length is larger than the max...
  if (logFileLength > MAX_LOG_FILE_SIZE) {
    createNewLog();
  }

  pthread_mutex_unlock(&logMutex);
}

/***********************************************************************************

  Name:     createNewLog

  Notes:    This function will start a new log file and copy old files into
 backups limiting the total number of old logs to a maximum

  Parameters: N/A

  Returns:  N/A

  Revision History:

        PTM     05/07/12    Created

 ***********************************************************************************/
#define BUF_SIZE (LOG_FILENAME_LEN + 8)
static void createNewLog() {
  char newFile[50];
  char oldFile[50];
  unsigned int fileNum;

  // close the current log file
  fclose(logFilePtr);

  // remove the oldest log file
  fileNum = MAX_LOG_FILES - 1;
  snprintf(oldFile, BUF_SIZE, "%s.%u", logFileName, fileNum);
  remove(oldFile);

  // advance the file numbers on all of the existing log files
  --fileNum;
  while (fileNum > 0) {
    snprintf(oldFile, BUF_SIZE, "%s.%u", logFileName, fileNum);
    snprintf(newFile, BUF_SIZE, "%s.%u", logFileName, fileNum + 1);
    rename(oldFile, newFile);
    --fileNum;
  }

  // rename the primary log file to .1
  snprintf(newFile, BUF_SIZE, "%s.1", logFileName);
  rename(logFileName, newFile);

  // open the new log file
  logFilePtr = fopen(logFileName, "a");

  logFileLength = 0;
}

/***********************************************************************************

  Name:     getTimestampString

  Notes:    This function will return a timestamp string, in the format
            MM/DD/YYYY HH:MM:SS:UUU, to be used for logging.

  Parameters:
      timestampLogStr (o): pointer to the string that will contain the timestamp
 on exit.

  Returns:  N/A

  Revision History:

        PTM     05/07/12    Created

 ***********************************************************************************/
static void getTimestampString(char *timestampLogStr) {
  // Get time data
  struct tm *tp;
  struct timeb tsb;

  ftime(&tsb);
  tp = localtime(&tsb.time);

  // Create time log string:  MM/DD/YYYY HH:MM:SS:UUU
  sprintf(timestampLogStr, "%02d/%02d/%d %02d:%02d:%02d:%03d", (tp->tm_mon + 1),
          tp->tm_mday, (tp->tm_year + 1900), tp->tm_hour, tp->tm_min,
          tp->tm_sec, tsb.millitm);

}  // end getTimestampString
