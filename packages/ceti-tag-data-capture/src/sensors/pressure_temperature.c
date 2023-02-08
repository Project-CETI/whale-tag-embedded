//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "pressure_temperature.h"

//-----------------------------------------------------------------------------
// Global/static variables
//-----------------------------------------------------------------------------
int g_pressureTemperature_thread_is_running = 0;
static FILE* pressureTemperature_data_file = NULL;
static char pressureTemperature_data_file_notes[256] = "";
static const char* pressureTemperature_data_file_headers[] = {
  "Pressure [bar]",
  "Water Temperature [C]",
  };
static const int num_pressureTemperature_data_file_headers = 2;
// Store global versions of the latest readings since the state machine will use them.
double g_latest_pressureTemperature_pressure_bar = 0;
double g_latest_pressureTemperature_temperature_c = 0;

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

int init_pressureTemperature() {
  CETI_LOG("init_pressureTemperature(): Successfully initialized the pressure/temperature sensor [did nothing]");

  // Open an output file to write data.
  if(init_data_file(pressureTemperature_data_file, PRESSURETEMPERATURE_DATA_FILEPATH,
                     pressureTemperature_data_file_headers,  num_pressureTemperature_data_file_headers,
                     pressureTemperature_data_file_notes, "init_pressureTemperature()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* pressureTemperature_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_pressureTemperature_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(PRESSURETEMPERATURE_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(PRESSURETEMPERATURE_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("pressureTemperature_thread(): Successfully set affinity to CPU %d", PRESSURETEMPERATURE_CPU);
      else
        CETI_LOG("pressureTemperature_thread(): XXX Failed to set affinity to CPU %d", PRESSURETEMPERATURE_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("pressureTemperature_thread(): Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_pressureTemperature_thread_is_running = 1;
    while (!g_exit) {
      pressureTemperature_data_file = fopen(PRESSURETEMPERATURE_DATA_FILEPATH, "at");
      if(pressureTemperature_data_file == NULL)
      {
        CETI_LOG("pressureTemperature_thread(): failed to open data output file: %s", PRESSURETEMPERATURE_DATA_FILEPATH);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
      }
      else
      {
        // Acquire timing and sensor information as close together as possible.
        global_time_us = get_global_time_us();
        rtc_count = getRtcCount();
        if(getPressureTemperature(&g_latest_pressureTemperature_pressure_bar,
                                  &g_latest_pressureTemperature_temperature_c)
                                  < 0)
          strcat(pressureTemperature_data_file_notes, "ERROR | ");
        if(g_latest_pressureTemperature_pressure_bar < -100 || g_latest_pressureTemperature_temperature_c < -100) // it seems to return -228.63 for pressure and -117.10 for temperature when no sensor is connected
        {
          CETI_LOG("pressureTemperature_thread(): XXX readings are likely invalid");
          strcat(pressureTemperature_data_file_notes, "INVALID? | ");
        }

        // Write timing information.
        fprintf(pressureTemperature_data_file, "%lld", global_time_us);
        fprintf(pressureTemperature_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(pressureTemperature_data_file, ",%s", pressureTemperature_data_file_notes);
        strcpy(pressureTemperature_data_file_notes, "");
        // Write the sensor data.
        fprintf(pressureTemperature_data_file, ",%.3f", g_latest_pressureTemperature_pressure_bar);
        fprintf(pressureTemperature_data_file, ",%.3f", g_latest_pressureTemperature_temperature_c);
        // Finish the row of data and close the file.
        fprintf(pressureTemperature_data_file, "\n");
        fclose(pressureTemperature_data_file);

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = PRESSURETEMPERATURE_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - global_time_us;
        if(polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
      }
    }
    g_pressureTemperature_thread_is_running = 0;
    CETI_LOG("pressureTemperature_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Keller sensor interface
//-----------------------------------------------------------------------------

int getPressureTemperature(double* pressure_bar, double* temperature_c) {

    int fd;
    short int temperature_data, pressure_data;
    char presSensData_byte[5];

    if ((fd = i2cOpen(1, ADDR_PRESSURETEMPERATURE, 0)) < 0) {
        CETI_LOG("updatePressureTemperature(): XXXX Failed to connect to the pressure/temperature sensor XXXX");
        return (-1);
    }

    i2cWriteByte(fd, 0xAC); // measurement request from the device
    usleep(10000);          // wait 10 ms for the measurement to finish

    i2cReadDevice(fd, presSensData_byte, 5); // read the measurement

    temperature_data = presSensData_byte[3] << 8;
    temperature_data = temperature_data + presSensData_byte[4];
    // convert to deg C
    *temperature_c = ((temperature_data >> 4) - 24) * .05 - 50;

    pressure_data = presSensData_byte[1] << 8;
    pressure_data = pressure_data + presSensData_byte[2];
    // convert to bar - see Keller data sheet for the particular sensor in use
    *pressure_bar = ((PRESSURE_MAX - PRESSURE_MIN) / 32768.0) * (pressure_data - 16384.0);

    i2cClose(fd);
    return (0);
}