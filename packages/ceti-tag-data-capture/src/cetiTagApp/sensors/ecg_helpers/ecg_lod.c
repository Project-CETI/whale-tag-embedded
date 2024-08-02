//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------
#include "ecg_lod.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
static int latest_register_value = 0;
static int new_read_requested = 0;

int g_ecg_lod_thread_is_running = 0;
//-----------------------------------------------------------------------------
// Read/parse data
//-----------------------------------------------------------------------------

// Read both ECG leads-off detections (positive and negative electrodes).
// Will first read all inputs of the GPIO expander, then extract the desired bit.
// Will use a single IO expander reading, so
//   both detections are effectively sampled simultaneously
//   and the IO expander only needs to be queried once.
void ecg_read_leadsOff(int* leadsOff_p, int* leadsOff_n) {
  // Read the latest result, and request an asynchronous reading for the next iteration.
  if (latest_register_value < 0) {
    *leadsOff_p = -1;
    *leadsOff_n = -1;
  } else {
    // Extract the desired pins.
    *leadsOff_p = ((latest_register_value >> IOX_GPIO_ECG_LOD_P) & 1);
    *leadsOff_n = ((latest_register_value >> IOX_GPIO_ECG_LOD_N) & 1);
  }
  new_read_requested = 1;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
int ecg_lod_init(void){
    WTResult hw_result = iox_init();
    // Initialize I2C/GPIO functionality for the IO expander.
    if (hw_result == WT_OK) hw_result = iox_set_mode(IOX_GPIO_ECG_LOD_N, IOX_MODE_INPUT);
    if (hw_result == WT_OK) hw_result = iox_set_mode(IOX_GPIO_ECG_LOD_P, IOX_MODE_INPUT);

    if (hw_result != WT_OK){
        CETI_ERR("Failed to initialize ecg leadsoff detection: %s", wt_strerror(hw_result));
        return -1;
    }
    CETI_LOG("Successfully initialized ecg leadsoff detection");
    return 0;
}

void *ecg_lod_thread(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_ecg_lod_thread_tid = gettid();

  // Set the thread CPU affinity.
  if (IOX_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(IOX_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", IOX_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", IOX_CPU);
  }

  // Main loop while application is running.
  CETI_LOG("Starting loop to read data in background");
  g_ecg_lod_thread_is_running = 1;
  long long sample_time_us;
  while (!g_stopAcquisition) {
    sample_time_us = get_global_time_us();
    // Perform a read operation if one has been requested.
    if(new_read_requested) {
      uint8_t value;
      //the way the ecg code handles hwerrors, it makes sense to just directly call 
      if(iox_read_register(IOX_REG_INPUT, &value) != WT_OK){
        latest_register_value = -1;
      } else {
        latest_register_value = (int)value;
      }
      new_read_requested = 0;
    }
    
    // Wait for the desired polling period.
    long long elapsed_time_us = get_global_time_us() - sample_time_us;
    long long sleep_duration_us = ECG_LOD_READ_POLLING_PERIOD_US - elapsed_time_us;
    if (sleep_duration_us)
      usleep(sleep_duration_us);
  }
  g_ecg_lod_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}
