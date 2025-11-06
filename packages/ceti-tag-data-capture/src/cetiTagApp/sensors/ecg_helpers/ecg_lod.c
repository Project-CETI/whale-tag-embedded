//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------
#include "ecg_lod.h"

#include "../../device/iox.h"
#include "../../utils/logging.h"
#include "../../utils/timing.h"

#include "../../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../../systemMonitor.h" // for the global CPU assignment variable to update

#include <pthread.h>
#include <unistd.h> // for usleep()

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
WTResult ecg_get_latest_leadsOff_detections(uint16_t *leadsOff_p, uint16_t *leadsOff_n) {
    // Read the latest result, and request an asynchronous reading for the next iteration.
    if (latest_iox_status != WT_OK) {
        *leadsOff_p = ECG_LEADSOFF_INVALID_PLACEHOLDER;
        *leadsOff_n = ECG_LEADSOFF_INVALID_PLACEHOLDER;
    } else {
        *leadsOff_p = (int16_t)((latest_iox_register_value >> IOX_GPIO_ECG_LOD_P) & 1);
        *leadsOff_n = (int16_t)((latest_iox_register_value >> IOX_GPIO_ECG_LOD_N) & 1);
    }
    return latest_iox_status;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *ecg_lod_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_ecg_lod_thread_tid = gettid();

    // Main loop while application is running.
    CETI_LOG("Starting loop to read data in background");
    g_ecg_lod_thread_is_running = 1;
    while (!g_stopAcquisition) {

        // Read the IO expander to get the latest detections.
        // The way the ecg code handles hardware errors, it makes sense to just directly call.
        int64_t task_start_us = get_monotonic_time_us();
        latest_iox_status = iox_read_register(IOX_REG_INPUT, &latest_iox_register_value);

        // Wait for the desired polling period.
        int64_t elapsed_time_us = get_monotonic_time_us() - task_start_us;
        int64_t sleep_duration_us = ECG_LOD_READ_POLLING_PERIOD_US - elapsed_time_us;
        if (sleep_duration_us > 0) {
            usleep(sleep_duration_us);
        }
    }
    g_ecg_lod_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
