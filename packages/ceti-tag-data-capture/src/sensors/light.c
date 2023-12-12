//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "light.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

#define LIGHT_TRY_OPEN(fd)   if ((fd=i2cOpen(1, ADDR_LIGHT, 0)) < 0) { \
    CETI_LOG("XXXX Failed to connect to the light sensor XXXX"); \
    return -1; \
  }

// Global/static variables
int g_light_thread_is_running = 0;
static FILE *light_data_file = NULL;
static char light_data_file_notes[256] = "";
static const char *light_data_file_headers[] = {
    "Ambient Light: Visible",
    "Ambient Light: IR",
};
static const int num_light_data_file_headers = 2;
static int light_reading_visible;
static int light_reading_ir;

int init_light() {
  if(light_wake() != 0){
    CETI_LOG("XXXX Failed to wake light sensor XXXX");
    return -1;
  }
  CETI_LOG("Successfully initialized the light sensor");

  // Open an output file to write data.
  if (init_data_file(light_data_file, LIGHT_DATA_FILEPATH,
                     light_data_file_headers, num_light_data_file_headers,
                     light_data_file_notes, "init_light()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *light_thread(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_light_thread_tid = gettid();

  // Set the thread CPU affinity.
  if (LIGHT_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LIGHT_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", LIGHT_CPU);
    else
      CETI_LOG("XXX Failed to set affinity to CPU %d", LIGHT_CPU);
  }

  // Main loop while application is running.
  CETI_LOG("Starting loop to periodically acquire data");
  int64_t global_time_us;
  int rtc_count;
  int64_t polling_sleep_duration_us;
  g_light_thread_is_running = 1;
  while (!g_stopAcquisition) {
    light_data_file = fopen(LIGHT_DATA_FILEPATH, "at");
    if (light_data_file == NULL) {
      CETI_LOG("failed to open data output file: %s", LIGHT_DATA_FILEPATH);
      // Sleep a bit before retrying.
      for (int i = 0; i < 10 && !g_stopAcquisition; i++)
        usleep(100000);
    } else {
      bool light_data_error = false;
      bool light_data_valid = true;
      // Acquire timing and sensor information as close together as possible.
      global_time_us = get_global_time_us();
      rtc_count = getRtcCount();
      light_data_error = (getAmbientLight(&light_reading_visible, &light_reading_ir) < 0);

      // it seems to return -83 when no sensor is connected
      light_data_valid = !(light_reading_visible < -80 || light_reading_ir < -80);
      if (!light_data_valid) {
        CETI_LOG("XXX readings are likely invalid");
      }

      // Write timing information.
      fprintf(light_data_file, "%lld", global_time_us);
      fprintf(light_data_file, ",%d", rtc_count);
      // Write any notes, then clear them so they are only written once.
      fprintf(light_data_file, ",%s", light_data_file_notes);
      if (light_data_error)
        fprintf(light_data_file, "ERROR | ");
      if (!light_data_valid)
        fprintf(light_data_file, "INVALID? | ");
      light_data_file_notes[0] = '\0';
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
      if (polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);
    }
  }
  g_light_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}

//-----------------------------------------------------------------------------
// Ambient light sensor LiteON LTR-329ALS-01 Interface
//-----------------------------------------------------------------------------

int getAmbientLight(int* pAmbientLightVisible, int* pAmbientLightIR) {
    int fd;

    LIGHT_TRY_OPEN(fd);

  // Read both wavelengths.
  *pAmbientLightVisible = i2cReadWordData(fd, 0x88); // visible
  *pAmbientLightIR = i2cReadWordData(fd, 0x8A);      // infrared
  i2cClose(fd);
  return (0);
}

int light_verify(void) {
  int fd;
  int result;
  uint8_t manu_id, part_id, rev_id; 

  LIGHT_TRY_OPEN(fd);

  if((result = i2cReadWordData(fd,0x87)) < 0) {
    CETI_LOG("XXXX Failed to read light sensor XXXX");
    i2cClose(fd);
    return result;
  } 

  manu_id = (uint8_t) result;
  if (manu_id != 0x05) {
    i2cClose(fd);
    return 1;
  }

  if((result = i2cReadWordData(fd,0x86)) < 0) {
    CETI_LOG("XXXX Failed to read light sensor XXXX");
    i2cClose(fd);
    return result;
  }
  i2cClose(fd);

  part_id = (uint8_t)result >> 4;
  rev_id = (uint8_t)result & 0x0F;

  if (part_id != 0x0a) 
    return 2;
  if (rev_id != 0x00) 
    return 3;

  //part part verified
  return 0;
}

int light_wake(void){
  int fd;

  LIGHT_TRY_OPEN(fd);
  return i2cWriteByteData(fd,0x80,0x1); // wake the light sensor up
  
}
