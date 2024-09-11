//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "pressure_temperature.h"

// === Private Local Libraries ===
#include "../utils/memory.h"
#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"

// === Private System Libraries ===
#include <fcntl.h>
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <stdint.h>
#include <unistd.h> // for usleep()

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
static const int num_pressureTemperature_data_file_headers = sizeof(pressureTemperature_data_file_headers)/sizeof(*pressureTemperature_data_file_headers);
// Store global versions of the latest readings since the state machine will use
// them.
CetiPressureSample *g_pressure = NULL;
static sem_t *s_pressure_data_ready;

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Keller sensor interface
//-----------------------------------------------------------------------------

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

void pressure_update_sample(void){
  // Acquire timing and sensor information as close together as possible.
  g_pressure->sys_time_us = get_global_time_us();
  g_pressure->rtc_time_s = getRtcCount();
  g_pressure->error = getPressureTemperature(&g_pressure->pressure_bar, &g_pressure->temperature_c);
  
  // push semaphore to indicate to user applications that new data is available
  sem_post(s_pressure_data_ready);
}

/**
 * @brief convert pressure semsor sample to human readable csv
 */
void pressure_sample_to_csv(FILE *fp, CetiPressureSample *pSample){
      // Write timing information.
      fprintf(fp, "%ld", g_pressure->sys_time_us);
      fprintf(fp, ",%d", g_pressure->rtc_time_s);
      // Write any notes, then clear them so they are only written once.
      fprintf(fp, ",%s", pressureTemperature_data_file_notes);
      if (g_pressure->error != 0)
        fprintf(fp, "ERROR | ");
        
      // it seems to return -228.63 for pressure and -117.10 for temperature when no sensor is connected
      if (g_pressure->pressure_bar < -100 || g_pressure->temperature_c < -100) {
        CETI_WARN("Readings are likely invalid");
        fprintf(fp, "INVALID? | ");
      }

      pressureTemperature_data_file_notes[0] = '\0';
      // Write the sensor data.
      fprintf(fp, ",%.3f", g_pressure->pressure_bar);
      fprintf(fp, ",%.3f", g_pressure->temperature_c);
      // Finish the row of data and close the file.
      fprintf(fp, "\n");
}


//-----------------------------------------------------------------------------
// CetiTagApp - Main thread
//-----------------------------------------------------------------------------
int init_pressureTemperature() {
    CETI_LOG("Successfully initialized the pressure/temperature sensor.");

    // Open an output file to write data.
    if (init_data_file(pressureTemperature_data_file,
                        PRESSURETEMPERATURE_DATA_FILEPATH,
                        pressureTemperature_data_file_headers,
                        num_pressureTemperature_data_file_headers,
                        pressureTemperature_data_file_notes,
                        "init_pressureTemperature()") < 0)
        return -1;
    //setup shared memory
    g_pressure = create_shared_memory_region(PRESSURE_SHM_NAME, sizeof(CetiPressureSample));

    //setup semaphore
    s_pressure_data_ready = sem_open(PRESSURE_SEM_NAME, O_CREAT, 0644, 0);
    if(s_pressure_data_ready == SEM_FAILED){
        perror("sem_open");
        CETI_ERR("Failed to create semaphore");
        return -1;
    }
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
    int64_t polling_sleep_duration_us;
    g_pressureTemperature_thread_is_running = 1;
    while (!g_stopAcquisition) {
        //update sample for system
        pressure_update_sample();

        // log sample
        if (!g_stopLogging) {
          pressureTemperature_data_file = fopen(PRESSURETEMPERATURE_DATA_FILEPATH, "at");
          if (pressureTemperature_data_file == NULL) {
            CETI_LOG("failed to open data output file: %s", PRESSURETEMPERATURE_DATA_FILEPATH);
          } else {
            pressure_sample_to_csv(pressureTemperature_data_file, g_pressure);
            fclose(pressureTemperature_data_file);
          }
        }

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = PRESSURE_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - g_pressure->sys_time_us;
        if (polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
    }
    g_pressureTemperature_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

