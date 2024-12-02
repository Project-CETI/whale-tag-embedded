//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------
#include "ecg_lod.h"

#include "../../utils/logging.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
static uint8_t latest_iox_register_value = 0xFF;
static WTResult latest_iox_status;

int g_ecg_lod_thread_is_running = 0;

int ecg_lod_init(void) {
    latest_iox_status = iox_init();
    // Initialize I2C/GPIO functionality for the IO expander.
    if (latest_iox_status == WT_OK)
        latest_iox_status = iox_set_mode(IOX_GPIO_ECG_LOD_P, IOX_MODE_INPUT);
    if (latest_iox_status == WT_OK)
        latest_iox_status = iox_set_mode(IOX_GPIO_ECG_LOD_N, IOX_MODE_INPUT);

    if (latest_iox_status != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to initialize ECG leads-off detection: %s", wt_strerror_r(latest_iox_status, err_str, sizeof(err_str)));
        return -1;
    }
    CETI_LOG("Successfully initialized ECG leads-off detection");
    return 0;
}

//-----------------------------------------------------------------------------
// Read/parse data
//-----------------------------------------------------------------------------

// Read both ECG leads-off detections (positive and negative electrodes).
// Will first read all inputs of the GPIO expander, then extract the desired bit.
// Will use a single IO expander reading, so
//   both detections are effectively sampled simultaneously
//   and the IO expander only needs to be queried once.
void ecg_get_latest_leadsOff_detections(int *leadsOff_p, int *leadsOff_n) {
    // Read the latest result, and request an asynchronous reading for the next iteration.
    if (latest_iox_status != WT_OK) {
        *leadsOff_p = ECG_LEADSOFF_INVALID_PLACEHOLDER;
        *leadsOff_n = ECG_LEADSOFF_INVALID_PLACEHOLDER;
    } else {
        *leadsOff_p = ((latest_iox_register_value >> IOX_GPIO_ECG_LOD_P) & 1);
        *leadsOff_n = ((latest_iox_register_value >> IOX_GPIO_ECG_LOD_N) & 1);
    }
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *ecg_lod_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_ecg_lod_thread_tid = gettid();

    // Set the thread CPU affinity.
    if (ECG_LOD_CPU >= 0) {
        pthread_t thread;
        thread = pthread_self();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(ECG_LOD_CPU, &cpuset);
        if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
            CETI_LOG("Successfully set affinity to CPU %d", ECG_LOD_CPU);
        else
            CETI_WARN("Failed to set affinity to CPU %d", ECG_LOD_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to read data in background");
    g_ecg_lod_thread_is_running = 1;
    long long sample_time_us;
    while (!g_stopAcquisition) {

        // Read the IO expander to get the latest detections.
        // The way the ecg code handles hardware errors, it makes sense to just directly call.
        sample_time_us = get_global_time_us();
        latest_iox_status = iox_read_register(IOX_REG_INPUT, &latest_iox_register_value);

        // If there was an error, wait a bit and then try to reinitialize.
        // if(latest_iox_status != WT_OK) {
        //   CETI_LOG("XXX The GPIO expander encountered an error retrieving the ECG leads-off detections");
        //   usleep(1000000);
        //   ecg_lod_init();
        //   usleep(10000);
        // }
        // this causes the log file to explode

        // Wait for the desired polling period.
        long long elapsed_time_us = get_global_time_us() - sample_time_us;
        long long sleep_duration_us = ECG_LOD_READ_POLLING_PERIOD_US - elapsed_time_us;
        if (sleep_duration_us > 0)
            usleep(sleep_duration_us);
    }
    g_ecg_lod_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
