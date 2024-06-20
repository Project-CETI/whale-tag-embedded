//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "pressure_temperature.h"

//-----------------------------------------------------------------------------
// Global/static variables
//-----------------------------------------------------------------------------
int g_pressureTemperature_thread_is_running = 0;
static FILE *pressureTemperature_data_file = NULL;
static char pressureTemperature_data_file_notes[256] = "";
static const char *pressureTemperature_data_file_headers[] = {
    "Pressure [bar]",
    "Water Temperature [C]",
};
static const int num_pressureTemperature_data_file_headers = 2;
// Store global versions of the latest readings since the state machine will use
// them.
double g_latest_pressureTemperature_pressure_bar = 0.0;
double g_latest_pressureTemperature_temperature_c = 0.0;

static double s_pressure_zero_bar = 0.0;

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Keller sensor interface
//-----------------------------------------------------------------------------

int pressure_zero(void){
  int result = getPressureTemperature(&s_pressure_zero_bar, NULL);
  if (result == 0)
    CETI_LOG("Pressure sensor zeroed at %5.3f bar", s_pressure_zero_bar);
  return result;
}

int getPressureTemperature(double *pressure_bar, double *temperature_c) {

  int fd;
  int16_t temperature_data, pressure_data;
  char presSensData_byte[5];

  if ((fd = i2cOpen(1, ADDR_PRESSURETEMPERATURE, 0)) < 0) {
    CETI_ERR("Failed to connect to the pressure/temperature sensor");
    return (-1);
  }

  i2cWriteByte(fd, 0xAC); // measurement request from the device
  usleep(10000);          // wait 10 ms for the measurement to finish

  i2cReadDevice(fd, presSensData_byte, 5); // read the measurement

  if (temperature_c != NULL) { 
    temperature_data = presSensData_byte[3] << 8;
    temperature_data = temperature_data + presSensData_byte[4];
    // convert to deg C
    *temperature_c = ((temperature_data >> 4) - 24) * .05 - 50;
  }

  if (pressure_bar != NULL) {
    pressure_data = presSensData_byte[1] << 8;
    pressure_data = pressure_data + presSensData_byte[2];
    // convert to bar - see Keller data sheet for the particular sensor in use
    *pressure_bar =
        ((PRESSURE_MAX - PRESSURE_MIN) / 32768.0) * (pressure_data - 16384.0);
  }
  i2cClose(fd);
  return (0);
}

int pressure_get_calibrated(double *pressure_bar, double * temperature_c) {
  double raw_pressure_bar;
  if(getPressureTemperature(&raw_pressure_bar, temperature_c) != 0){
    return -1;
  }
  *pressure_bar = raw_pressure_bar - s_pressure_zero_bar;
  return 0;
}



//-----------------------------------------------------------------------------
// CetiTagApp - Main thread
//-----------------------------------------------------------------------------
int init_pressureTemperature() {
  #if PRESSURE_ZERO_AT_STARTUP
  if(pressure_zero() != 0) {
    CETI_ERR("Could not read an initial pressure value for calibration");
    return -2;
  }
  #else
  CETI_LOG("Note: not using an initial pressure value for calibration");
  #endif
  CETI_LOG("Successfully initialized the pressure/temperature sensor.");

  // Open an output file to write data.
  if (init_data_file(pressureTemperature_data_file,
                     PRESSURETEMPERATURE_DATA_FILEPATH,
                     pressureTemperature_data_file_headers,
                     num_pressureTemperature_data_file_headers,
                     pressureTemperature_data_file_notes,
                     "init_pressureTemperature()") < 0)
    return -1;

  return 0;
}

void *pressureTemperature_thread(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_pressureTemperature_thread_tid = gettid();

  // Set the thread CPU affinity.
  if (PRESSURETEMPERATURE_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(PRESSURETEMPERATURE_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", PRESSURETEMPERATURE_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", PRESSURETEMPERATURE_CPU);
  }

  // Main loop while application is running.
  CETI_LOG("Starting loop to periodically acquire data");
  long long global_time_us;
  int rtc_count;
  int64_t polling_sleep_duration_us;
  g_pressureTemperature_thread_is_running = 1;
  while (!g_stopAcquisition) {
    pressureTemperature_data_file = fopen(PRESSURETEMPERATURE_DATA_FILEPATH, "at");
    if (pressureTemperature_data_file == NULL) {
      CETI_LOG("failed to open data output file: %s", PRESSURETEMPERATURE_DATA_FILEPATH);
      // Sleep a bit before retrying.
      for (int i = 0; i < 10 && !g_stopAcquisition; i++)
        usleep(100000);
    } else {
      bool pressureTemperature_data_error = false;
      bool pressureTemperature_data_valid = true;
      // Acquire timing and sensor information as close together as possible.
      global_time_us = get_global_time_us();
      rtc_count = getRtcCount();
      pressureTemperature_data_error = (pressure_get_calibrated(&g_latest_pressureTemperature_pressure_bar, &g_latest_pressureTemperature_temperature_c) < 0);

      // it seems to return -228.63 for pressure and -117.10 for temperature when no sensor is connected
      pressureTemperature_data_valid = !(g_latest_pressureTemperature_pressure_bar < -100 || g_latest_pressureTemperature_temperature_c < -100);
      if (!pressureTemperature_data_valid) {
        CETI_WARN("Readings are likely invalid");
      }

      // Write timing information.
      fprintf(pressureTemperature_data_file, "%lld", global_time_us);
      fprintf(pressureTemperature_data_file, ",%d", rtc_count);
      // Write any notes, then clear them so they are only written once.
      fprintf(pressureTemperature_data_file, ",%s", pressureTemperature_data_file_notes);
      if (pressureTemperature_data_error)
        fprintf(pressureTemperature_data_file, "ERROR | ");
      if (!pressureTemperature_data_valid)
        fprintf(pressureTemperature_data_file, "INVALID? | ");

      pressureTemperature_data_file_notes[0] = '\0';
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
      if (polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);
    }
  }
  g_pressureTemperature_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}

