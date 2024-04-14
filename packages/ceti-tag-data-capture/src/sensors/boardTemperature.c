//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "boardTemperature.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_boardTemperature_thread_is_running = 0;
static FILE* boardTemperature_data_file = NULL;
static char boardTemperature_data_file_notes[256] = "";
static const char* boardTemperature_data_file_headers[] = {
  "Board Temperature [C]",
  "Battery Temperature [C]",
  };
static const int num_boardTemperature_data_file_headers = 2;
static int boardTemperature_c;
static int batteryTemperature_c;
static int charging_disabled, discharging_disabled;

int init_boardTemperature() {
  CETI_LOG("Successfully initialized the board temperature sensor [did nothing]");

  // Open an output file to write data.
  if(init_data_file(boardTemperature_data_file, BOARDTEMPERATURE_DATA_FILEPATH,
                     boardTemperature_data_file_headers,  num_boardTemperature_data_file_headers,
                     boardTemperature_data_file_notes, "init_boardTemperature()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* boardTemperature_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_boardTemperature_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(BOARDTEMPERATURE_CPU >= 0) {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(BOARDTEMPERATURE_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("Successfully set affinity to CPU %d", BOARDTEMPERATURE_CPU);
      else
        CETI_ERR("Failed to set affinity to CPU %d", BOARDTEMPERATURE_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_boardTemperature_thread_is_running = 1;
    while (!g_stopAcquisition) {
      boardTemperature_data_file = fopen(BOARDTEMPERATURE_DATA_FILEPATH, "at");
      if(boardTemperature_data_file == NULL) {
        CETI_LOG("failed to open data output file: %s", BOARDTEMPERATURE_DATA_FILEPATH);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_stopAcquisition; i++)
          usleep(100000);
        continue;
      }


      // Acquire timing and sensor information as close together as possible.
      global_time_us = get_global_time_us();
      rtc_count = getRtcCount();

      if(getTemperatures(&boardTemperature_c,&batteryTemperature_c) < 0)
        strcat(boardTemperature_data_file_notes, "ERROR | ");

      if(boardTemperature_c < -80) // it seems to return -83 when no sensor is connected
      {
        CETI_ERR("readings are likely invalid");
        strcat(boardTemperature_data_file_notes, "INVALID? | ");
      }
      
      // ******************   Battery Temperature Checks *************************

      if( (batteryTemperature_c > MAX_CHARGE_TEMP) ||  (batteryTemperature_c < MIN_CHARGE_TEMP) ) {          
        if (!charging_disabled){  
          disableCharging();
          charging_disabled = 1;
          CETI_WARN("Battery charging disabled, outside thermal limits: %d C", batteryTemperature_c);
        }
      }

      if( (batteryTemperature_c > MAX_DISCHARGE_TEMP) ) {
        if (!discharging_disabled){  
          disableDischarging();
          discharging_disabled = 1;
          CETI_WARN("Battery discharging disabled, outside thermal limit: %d C", batteryTemperature_c);
        }     
      }
    
      // ******************   End Battery Temperature Checks *********************
      
      // Write timing information.
      fprintf(boardTemperature_data_file, "%lld", global_time_us);
      fprintf(boardTemperature_data_file, ",%d", rtc_count);
      // Write any notes, then clear them so they are only written once.
      fprintf(boardTemperature_data_file, ",%s", boardTemperature_data_file_notes);
      strcpy(boardTemperature_data_file_notes, "");
      // Write the sensor data.
      fprintf(boardTemperature_data_file, ",%d", boardTemperature_c);
      fprintf(boardTemperature_data_file, ",%d", batteryTemperature_c);
      // Finish the row of data and close the file.
      fprintf(boardTemperature_data_file, "\n");
      fclose(boardTemperature_data_file);

      // Delay to implement a desired sampling rate.
      // Take into account the time it took to acquire/save data.
      polling_sleep_duration_us = BOARDTEMPERATURE_SAMPLING_PERIOD_US;
      polling_sleep_duration_us -= get_global_time_us() - global_time_us;
      if(polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);
    }
    g_boardTemperature_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Board mounted temperature sensor SA56004 Interface
//  * This device provides two measurement channels, one of which is a remote 
//    temperature sesnsing diode attached to the battery cells.
//-----------------------------------------------------------------------------

int getBoardTemperature(int *pBoardTemp) {
  if (pBoardTemp == NULL) {
    CETI_ERR("Process failed: Null pointer passed in to store data.");
    return (-1);
  }
  #if MAX17320 == 1
    int ret = max17320_get_temperature(&dev);
    *pBoardTemp = dev.die_temperature;
    return ret;
  #else
    int fd = i2cOpen(1,ADDR_BOARDTEMPERATURE,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the temperature sensor");
        return(-1);
    }

    *pBoardTemp = i2cReadByteData(fd,0x00);
    i2cClose(fd);
    return(0);
  #endif
}

int getBatteryTemperature(int *pBattTemp) {
  if (pBattTemp == NULL) {
    CETI_ERR("Process failed: Null pointer passed in to store data.");
    return (-1);
  }
  #if MAX17320 == 1
    int ret = max17320_get_temperature(&dev);
    *pBattTemp = dev.temperature;
    return ret;
  #else
    int fd = i2cOpen(1,ADDR_BOARDTEMPERATURE,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the temperature sensor");
        return(-1);
    }

    *pBattTemp = i2cReadByteData(fd,0x01);
    i2cClose(fd);
    return(0);
  #endif
}

// This function does both at once, experimental for chasing the bus crash behavior
// What we observe is that the bus hangs, not seeing this bug with the single
// temperature reading. Hang in this case is SCL stuck low. Restarting the program
// releases it. 

int getTemperatures(int *pBoardTemp, int *pBattTemp) {
  if (pBattTemp == NULL || pBoardTemp == NULL) {
    CETI_ERR("Process failed: Null pointers passed in to store data.");
    return (-1);
  }
  #if MAX17320 == 1
    int ret = max17320_get_temperature(&dev);
    *pBoardTemp = dev.die_temperature;
    *pBattTemp = dev.temperature;
    return ret;
  #else
    int fd = i2cOpen(1,ADDR_BOARDTEMPERATURE,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the temperature sensor");
        return(-1);
    }
    *pBoardTemp = i2cReadByteData(fd,0x0);
    usleep(1000*10);  //experimental
    *pBattTemp  = i2cReadByteData(fd,0x01);
    i2cClose(fd);
    return(0);
  #endif
}

int resetBattTempFlags() {
    discharging_disabled = 0;
    charging_disabled = 0;
    enableDischarging();
    enableCharging();
    return(0);
}