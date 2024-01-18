//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "timing.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_rtc_thread_is_running = 0;
static int latest_rtc_count = -1;
static int64_t last_rtc_update_time_us = -1;

int init_timing() {
#if ENABLE_RTC
  // Test whether the RTC is available.
  updateRtcCount();
  if (latest_rtc_count == -1) {
    CETI_ERR("Failed to fetch a valid RTC count");
    latest_rtc_count = -1;
    last_rtc_update_time_us = -1;
    return (-1);
  }
#endif

  CETI_LOG("Successfully initialized timing");
  return 0;
}

unsigned int getTimeDeploy(void) {
  // Open the state machine data file and get the first RTC timestamp.
  // This will be used by the state machine to determine timeouts.

  FILE *stateMachineCsvFile = NULL;
  char *pTemp;

  char line[512];

  stateMachineCsvFile = fopen(STATEMACHINE_DATA_FILEPATH, "r");
  if (stateMachineCsvFile == NULL) {
    CETI_LOG("cannot open state machine csv output file: %s",
             STATEMACHINE_DATA_FILEPATH);
    return (-1);
  }

  // first line is always the header
  fgets(line, 512, stateMachineCsvFile);

  // first line of the actual data
  fgets(line, 512, stateMachineCsvFile);

  fclose(stateMachineCsvFile);
  // parse out the RTC count, which is in the 2nd column of the CSV
  for (pTemp = line; *pTemp != ','; pTemp++) {
  }

  // find first comma
  pTemp++;
  // append terminator
  pTemp[10] = '\0';

  // convert to uint
  return strtoul(pTemp, NULL, 0);
}

//-----------------------------------------------------------------------------
// RTC second counter
//-----------------------------------------------------------------------------
int getRtcCount() { return latest_rtc_count; }

void updateRtcCount() {
  int fd;
  int rtcCount, rtcShift = 0;
  char rtcCountByte[4];

  if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
    CETI_LOG("Failed to connect to the RTC");
    rtcCount = -1;
  } else {
    // read the time of day counter and assemble the bytes in 32 bit int
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

  latest_rtc_count = rtcCount;
  last_rtc_update_time_us = get_global_time_us();
}

int resetRtcCount() {
  int fd;

  CETI_LOG("Executing");

  if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
    CETI_LOG("Failed to connect to the RTC");
    return (-1);
  } else {
    i2cWriteByteData(fd, 0x00, 0x00);
    i2cWriteByteData(fd, 0x01, 0x00);
    i2cWriteByteData(fd, 0x02, 0x00);
    i2cWriteByteData(fd, 0x03, 0x00);
  }
  i2cClose(fd);
  return (0);
}

static int rtc_set_count(time_t seconds) {
  int fd;

  CETI_LOG("Executing");

  if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
    CETI_LOG("Failed to connect to the RTC");
    return (-1);
  } else {
    i2cWriteByteData(fd, 0x00, (uint8_t)((seconds >> 0) & 0xFF));
    i2cWriteByteData(fd, 0x01, (uint8_t)((seconds >> 8) & 0xFF));
    i2cWriteByteData(fd, 0x02, (uint8_t)((seconds >> 16) & 0xFF));
    i2cWriteByteData(fd, 0x03, (uint8_t)((seconds >> 24) & 0xFF));
  }
  i2cClose(fd);
  return (0);
}

// Thread to update the latest RTC time, to use the I2C bus more sparingly
//  instead of having all other threads that request RTC use the bus.
void *rtc_thread(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_rtc_thread_tid = gettid();

  // Set the thread CPU affinity.
  if (RTC_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(RTC_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", RTC_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", RTC_CPU);
  }

  // Do an initial RTC update.
  updateRtcCount();

  // Main loop while application is running.
  CETI_LOG("Starting loop to periodically acquire data");
  int old_rtc_count = -1;
  int delay_duration_us = 0;
  int prev_num_updates_required = 0;
  g_rtc_thread_is_running = 1;
  while (!g_exit) {
    // Wait the long polling period since the last update.
    // Unless the last time only required a single update to find a new RTC
    // value, in which case we might be out of step with the RTC clock and we
    // should use fast polling again to get near the RTC update boundary.
    //    Note that this is hopefully only the case on the first loop.
    if (prev_num_updates_required > 1) {
      delay_duration_us =
          (last_rtc_update_time_us + RTC_UPDATE_PERIOD_LONG_US) -
          get_global_time_us();
      if (delay_duration_us > RTC_UPDATE_PERIOD_SHORT_US)
        usleep(delay_duration_us);
    }
    // Update the RTC until its value changes, using the faster polling
    // period.
    old_rtc_count = latest_rtc_count;
    prev_num_updates_required = 0;
    while (latest_rtc_count == old_rtc_count) {
      usleep(RTC_UPDATE_PERIOD_SHORT_US);
      updateRtcCount();
      prev_num_updates_required++;
    }
  }
  g_rtc_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}

//-----------------------------------------------------------------------------
// Global time
//-----------------------------------------------------------------------------
int64_t get_global_time_us() {
  struct timeval current_timeval;
  int64_t current_time_us;

  gettimeofday(&current_timeval, NULL);
  current_time_us = (int64_t)(current_timeval.tv_sec * 1000000LL) +
                    (int64_t)(current_timeval.tv_usec);
  return current_time_us;
}

int64_t get_global_time_ms() {
  int64_t current_time_ms;
  struct timeval current_timeval;
  gettimeofday(&current_timeval, NULL);
  current_time_ms = (int64_t)(current_timeval.tv_sec * 1000LL) +
                    (int64_t)(current_timeval.tv_usec / 1000);
  return current_time_ms;
}

void sync_global_time_init(void) {
  CETI_LOG("Executing");
  struct timex timex_info = {.modes = 0};
  int ntp_result = ntp_adjtime(&timex_info);
  int ntp_synchronized = (ntp_result >= 0) && (ntp_result != TIME_ERROR);

  if (ntp_synchronized) {
    struct timeval current_timeval;
    gettimeofday(&current_timeval, NULL);
    rtc_set_count(current_timeval.tv_sec);
    CETI_LOG("RTC synchronized to system clock: %ld)", current_timeval.tv_sec);
  } else {
    CETI_LOG("Synchronizing system clock to RTC)");
    struct timeval current_timeval = {.tv_sec = getRtcCount()};
    settimeofday(&current_timeval, NULL);
  }
}
