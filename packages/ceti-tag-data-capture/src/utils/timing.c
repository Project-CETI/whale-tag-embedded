//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "timing.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_timing() {
  CETI_LOG("init_timing(): Successfully initialized timing [did nothing]");
  return 0;
}

unsigned int getTimeDeploy(void) {
    // Open the state machine data file and get the first RTC timestamp.
    // This will be used by the state machine to determine timeouts.

    FILE *stateMachineCsvFile = NULL;
    char *pTemp;

    char line[512];

    char strTimeDeploy[16];

    unsigned int timeDeploy;

    stateMachineCsvFile = fopen(STATEMACHINE_DATA_FILEPATH, "r");
    if (stateMachineCsvFile == NULL) {
        CETI_LOG("getTimeDeploy():cannot open state machine csv output file: %s", STATEMACHINE_DATA_FILEPATH);
        return (-1);
    }

    fgets(line, 512, stateMachineCsvFile); // first line is always the header
    fgets(line, 512, stateMachineCsvFile); // first line of the actual data

    // parse out the RTC count, which is in the 2nd column of the CSV
    for(pTemp = line; *pTemp != ',' ; pTemp++);  //find first comma
    strncpy(strTimeDeploy, pTemp+1, 10);         //copy the string
    strTimeDeploy[10] = '\0';                    //append terminator
    timeDeploy = strtoul(strTimeDeploy,NULL,0);  //convert to uint

    fclose(stateMachineCsvFile);
    return (timeDeploy);
}

//-----------------------------------------------------------------------------
// RTC second counter
//-----------------------------------------------------------------------------
int getRtcCount() {

    int fd;
    int rtcCount, rtcShift = 0;
    char rtcCountByte[4];

    if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
        CETI_LOG("getRtcCount(): Failed to connect to the RTC");
        return (-1);
    }

    else { // read the time of day counter and assemble the bytes in 32 bit int

        rtcCountByte[0] = i2cReadByteData(fd, 0x00);
        rtcCountByte[1] = i2cReadByteData(fd, 0x01);
        rtcCountByte[2] = i2cReadByteData(fd, 0x02);
        rtcCountByte[3] = i2cReadByteData(fd, 0x03);

        rtcCount = (rtcCountByte[0]);
        rtcShift = (rtcCountByte[1] << 8);
        rtcCount = rtcCount | rtcShift;
        rtcShift = (rtcCountByte[2] << 16);
        rtcCount = rtcCount | rtcShift;
        rtcShift = (rtcCountByte[3] << 24);
        rtcCount = rtcCount | rtcShift;
    }

    i2cClose(fd);
    return rtcCount;
}

int resetRtcCount() {
    int fd;

    CETI_LOG("resetRtcCount(): Exectuting");

    if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
        CETI_LOG("getRtcCount(): Failed to connect to the RTC");
        return (-1);
    }

    else {
        i2cWriteByteData(fd, 0x00, 0x00);
        i2cWriteByteData(fd, 0x01, 0x00);
        i2cWriteByteData(fd, 0x02, 0x00);
        i2cWriteByteData(fd, 0x03, 0x00);
    }
    i2cClose(fd);
    return (0);
}

//-----------------------------------------------------------------------------
// Global time
//-----------------------------------------------------------------------------
long long get_global_time_us()
{
  struct timeval current_timeval;
  long long current_time_us;

  gettimeofday(&current_timeval, NULL);
  current_time_us = (long long)(current_timeval.tv_sec * 1000000LL)
                     + (long long)(current_timeval.tv_usec);
  return current_time_us;
}

long long get_global_time_ms()
{
  long long current_time_ms;
  struct timeval current_timeval;
  gettimeofday(&current_timeval, NULL);
  current_time_ms = (long long)(current_timeval.tv_sec * 1000LL) + (long long)(current_timeval.tv_usec / 1000);
  return current_time_ms;
}


