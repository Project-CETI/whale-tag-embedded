//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "light.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_light_thread_is_running = 0;
static FILE* light_data_file = NULL;
static char light_data_file_notes[256] = "";
static const char* light_data_file_headers[] = {
  "Ambient Light: Visible",
  "Ambient Light: IR",
  };
static const int num_light_data_file_headers = 2;
static int light_reading_visible;
static int light_reading_ir;

int init_light() {
  int fd;
  if((fd=i2cOpen(1,ADDR_LIGHT,0)) < 0) {
    CETI_LOG("init_light(): XXXX Failed to connect to the light sensor XXXX");
    return (-1);
  }
  else {
    i2cWriteByteData(fd,0x80,0x1); // wake the light sensor up
  }
  CETI_LOG("init_light(): Successfully initialized the light sensor");

  // Open an output file to write data.
  if(init_data_file(light_data_file, LIGHT_DATA_FILEPATH,
                     light_data_file_headers,  num_light_data_file_headers,
                     light_data_file_notes, "init_light()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* light_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_light_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(LIGHT_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(LIGHT_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("light_thread(): Successfully set affinity to CPU %d", LIGHT_CPU);
      else
        CETI_LOG("light_thread(): XXX Failed to set affinity to CPU %d", LIGHT_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("light_thread(): Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_light_thread_is_running = 1;
    while (!g_exit) {
      light_data_file = fopen(LIGHT_DATA_FILEPATH, "at");
      if(light_data_file == NULL)
      {
        CETI_LOG("light_thread(): failed to open data output file: %s", LIGHT_DATA_FILEPATH);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
      }
      else
      {
        // Acquire timing and sensor information as close together as possible.
        global_time_us = get_global_time_us();
        rtc_count = getRtcCount();
        if(getAmbientLight(&light_reading_visible, &light_reading_ir) < 0)
          strcat(light_data_file_notes, "ERROR | ");
        if(light_reading_visible < -80 || light_reading_ir < -80) // it seems to return -83 when no sensor is connected
        {
          CETI_LOG("light_thread(): XXX readings are likely invalid");
          strcat(light_data_file_notes, "INVALID? | ");
        }

        // Write timing information.
        fprintf(light_data_file, "%lld", global_time_us);
        fprintf(light_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(light_data_file, ",%s", light_data_file_notes);
        strcpy(light_data_file_notes, "");
        // Write the sensor data.
        fprintf(light_data_file, ",%d", light_reading_visible);
        fprintf(light_data_file, ",%d", light_reading_ir);
        // Finish the row of data and close the file.
        fprintf(light_data_file, "\n");
        fclose(light_data_file);

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = LIGHT_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - global_time_us;
        if(polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
      }
    }
    g_light_thread_is_running = 0;
    CETI_LOG("light_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Ambient light sensor LiteON LTR-329ALS-01 Interface
//-----------------------------------------------------------------------------

int getAmbientLight(int* pAmbientLightVisible, int* pAmbientLightIR) {

    int fd;

    if((fd=i2cOpen(1,ADDR_LIGHT,0)) < 0) {
        CETI_LOG("getAmbientLight(): XXXX Failed to connect to the light sensor XXXX");
        return (-1);
    }

    // Read both wavelengths.
    *pAmbientLightVisible = i2cReadWordData(fd,0x88); // visible
    *pAmbientLightIR = i2cReadWordData(fd,0x8A); // infrared
    i2cClose(fd);
    return (0);
}
