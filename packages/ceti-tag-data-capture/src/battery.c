//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "battery.h"

//-----------------------------------------------------------------------------
// Global/static variables
//-----------------------------------------------------------------------------
int g_battery_thread_is_running = 0;
static FILE* battery_data_file = NULL;
static char battery_data_file_notes[256] = "";
static const char* battery_data_file_headers[] = {
  "Battery V1 [V]",
  "Battery V2 [V]",
  "Battery I [mA]",
  };
static const int num_battery_data_file_headers = 3;
// Store global versions of the latest readings since the state machine will use them.
double g_latest_battery_v1_v;
double g_latest_battery_v2_v;
double g_latest_battery_i_mA;

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_battery() {
  int fd;
  if((fd=i2cOpen(1,ADDR_BATT_GAUGE,0)) < 0) {
    CETI_LOG("init_battery(): XXXX Failed to connect to the battery gauge XXXX");
    return (-1);
  }
  else {
    i2cWriteByteData(fd,BATT_CTL,BATT_CTL_VAL); //establish undervoltage cutoff
    i2cWriteByteData(fd,BATT_OVER_VOLTAGE,BATT_OV_VAL); //establish undervoltage cutoff
  }
  CETI_LOG("init_battery(): Successfully initialized the battery gauge");

  // Open an output file to write data.
  if(init_data_file(battery_data_file, BATTERY_DATA_FILEPATH,
                     battery_data_file_headers,  num_battery_data_file_headers,
                     battery_data_file_notes, "init_battery()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* battery_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_battery_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(BATTERY_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(BATTERY_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("battery_thread(): Successfully set affinity to CPU %d", BATTERY_CPU);
      else
        CETI_LOG("battery_thread(): XXX Failed to set affinity to CPU %d", BATTERY_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("battery_thread(): Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_battery_thread_is_running = 1;
    while (!g_exit) {
      battery_data_file = fopen(BATTERY_DATA_FILEPATH, "at");
      if(battery_data_file == NULL)
      {
        CETI_LOG("battery_thread(): failed to open data output file: %s", BATTERY_DATA_FILEPATH);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
      }
      else
      {
        // Acquire timing and sensor information as close together as possible.
        global_time_us = get_global_time_us();
        rtc_count = getRtcCount();
        if(getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v,
                          &g_latest_battery_i_mA) < 0)
          strcat(battery_data_file_notes, "ERROR | ");
        if(g_latest_battery_v1_v < 0 || g_latest_battery_v2_v < 0) // it seems to return -0.01 for voltages and -5.19 for current when no sensor is connected
        {
          CETI_LOG("battery_thread(): XXX readings are likely invalid");
          strcat(battery_data_file_notes, "INVALID? | ");
        }

        // Write timing information.
        fprintf(battery_data_file, "%lld", global_time_us);
        fprintf(battery_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(battery_data_file, ",%s", battery_data_file_notes);
        strcpy(battery_data_file_notes, "");
        // Write the sensor data.
        fprintf(battery_data_file, ",%.3f", g_latest_battery_v1_v);
        fprintf(battery_data_file, ",%.3f", g_latest_battery_v2_v);
        fprintf(battery_data_file, ",%.3f", g_latest_battery_i_mA);
        // Finish the row of data and close the file.
        fprintf(battery_data_file, "\n");
        fclose(battery_data_file);

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = BATTERY_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - global_time_us;
        if(polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
      }
    }
    g_battery_thread_is_running = 0;
    CETI_LOG("battery_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Battery gauge interface
//-----------------------------------------------------------------------------
int getBatteryData(double* battery_v1_v, double* battery_v2_v,
                   double* battery_i_mA) {

    int fd, result;
    signed short voltage, current;

    if ((fd = i2cOpen(1, ADDR_BATT_GAUGE, 0)) < 0) {
        CETI_LOG("getBattStatus(): XXX Failed to connect to the fuel gauge");
        return (-1);
    }

    result = i2cReadByteData(fd, BATT_CELL_1_V_MS);
    voltage = result << 3;
    result = i2cReadByteData(fd, BATT_CELL_1_V_LS);
    voltage = (voltage | (result >> 5));
    voltage = (voltage | (result >> 5));
    *battery_v1_v = 4.883e-3 * voltage;

    result = i2cReadByteData(fd, BATT_CELL_2_V_MS);
    voltage = result << 3;
    result = i2cReadByteData(fd, BATT_CELL_2_V_LS);
    voltage = (voltage | (result >> 5));
    voltage = (voltage | (result >> 5));
    *battery_v2_v = 4.883e-3 * voltage;

    result = i2cReadByteData(fd, BATT_I_MS);
    current = result << 8;
    result = i2cReadByteData(fd, BATT_I_LS);
    current = current | result;
    *battery_i_mA = 1000 * current * (1.5625e-6 / BATT_R_SENSE);

    i2cClose(fd);
    return (0);
}

