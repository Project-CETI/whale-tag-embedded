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
#include "../device/keller4ld.h"
#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/thread_error.h"
#include "../utils/timing.h"

// === Private System Libraries ===
#include <errno.h>
#include <fcntl.h>
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h> // for usleep()

//-----------------------------------------------------------------------------
// Global/static variables
//-----------------------------------------------------------------------------
int g_pressureTemperature_thread_is_running = 0;
static int s_log_restarted = 1;
#define PRESSURE_CSV_HEADER \
    "Timestamp [us]"        \
    ",RTC Count"            \
    ",Notes"                \
    ",Pressure [bar]"       \
    ",Water Temperature [C]"

// Store global versions of the latest readings since the state machine will use
// them.
CetiPressureSample *g_pressure = NULL;
static sem_t *s_pressure_data_ready;

//-----------------------------------------------------------------------------

void pressure_update_sample(void) {
    // Acquire timing and sensor information as close together as possible.
    g_pressure->sys_time_us = get_global_time_us();
    g_pressure->rtc_time_s = getRtcCount();
    g_pressure->error = pressure_get_measurement(&g_pressure->pressure_bar, &g_pressure->temperature_c);

    // push semaphore to indicate to user applications that new data is available
    sem_post(s_pressure_data_ready);
}

/**
 * @brief convert pressure semsor sample to human readable csv
 */
void pressure_sample_to_csv(FILE *fp, CetiPressureSample *pSample) {
    // Write timing information.
    fprintf(fp, "%ld", g_pressure->sys_time_us);
    fprintf(fp, ",%d", g_pressure->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    if (s_log_restarted) {
        s_log_restarted = 0;
        fprintf(fp, "Restarted! | ");
    }

    if (g_pressure->error != 0) {
        fprintf(fp, "ERROR(%s) | ", wt_strerror(g_pressure->error));
    }

    // Write the sensor data.
    fprintf(fp, ",%.3f", g_pressure->pressure_bar);
    fprintf(fp, ",%.3f", g_pressure->temperature_c);
    // Finish the row of data and close the file.
    fprintf(fp, "\n");
}

//-----------------------------------------------------------------------------
// CetiTagApp - Main thread
//-----------------------------------------------------------------------------
int init_pressureTemperature(void) {
    // check that hardware is communicating, but don't worry about values
    WTResult hw_result = pressure_get_measurement(NULL, NULL);
    if (hw_result != WT_OK) {
        CETI_ERR("Failed to read pressure sensor: %s", wt_strerror(hw_result));
        return -1;
    }

    // setup shared memory
    g_pressure = create_shared_memory_region(PRESSURE_SHM_NAME, sizeof(CetiPressureSample));

    // setup semaphore
    s_pressure_data_ready = sem_open(PRESSURE_SEM_NAME, O_CREAT, 0644, 0);
    if (s_pressure_data_ready == SEM_FAILED) {
        CETI_ERR("Failed to create semaphore");
        return -1;
    }

    // Open an output file to write data.
    int data_file_exists = (access(PRESSURETEMPERATURE_DATA_FILEPATH, F_OK) != -1);
    FILE *data_file = fopen(PRESSURETEMPERATURE_DATA_FILEPATH, "at");
    if (data_file == NULL) {
        CETI_ERR("Failed to open/create an output data file: " PRESSURETEMPERATURE_DATA_FILEPATH ": %s", strerror(errno));
        return -1;
    }

    // Write headers if the file didn't already exist.
    if (!data_file_exists) {
        fprintf(data_file, PRESSURE_CSV_HEADER "\n");
    }
    fclose(data_file); // Close the file.
    s_log_restarted = 1;
    CETI_LOG("Using output data file: " PRESSURETEMPERATURE_DATA_FILEPATH);

    CETI_LOG("Successfully initialized the pressure/temperature sensor.");
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
        if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0) {
            CETI_LOG("Successfully set affinity to CPU %d", PRESSURETEMPERATURE_CPU);
        } else {
            CETI_WARN("Failed to set affinity to CPU %d", PRESSURETEMPERATURE_CPU);
        }
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    int64_t polling_sleep_duration_us;
    g_pressureTemperature_thread_is_running = 1;
    while (!g_stopAcquisition) {
        // update sample for system
        pressure_update_sample();
        update_thread_device_status(THREAD_PRESSURE_ACQ, g_pressure->error, __FUNCTION__);

        // log sample
        if (!g_stopLogging) {
            FILE *fp = fopen(PRESSURETEMPERATURE_DATA_FILEPATH, "at");
            if (fp == NULL) {
                CETI_LOG("failed to open data output file: " PRESSURETEMPERATURE_DATA_FILEPATH);
            } else {
                pressure_sample_to_csv(fp, g_pressure);
                fclose(fp);
            }
        }

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = PRESSURE_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - g_pressure->sys_time_us;
        if (polling_sleep_duration_us > 0) {
            usleep(polling_sleep_duration_us);
        }
    }
    g_pressureTemperature_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
