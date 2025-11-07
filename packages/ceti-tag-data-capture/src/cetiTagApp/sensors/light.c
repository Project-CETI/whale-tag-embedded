//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "light.h"

#include "../acq/decay.h"
#include "../cetiTag.h"
#include "../device/ltr329als.h"
#include "../launcher.h"      // for g_stopAcquisition, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/thread_error.h"
#include "../utils/timing.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_light_thread_is_running = 0;
#define LIGHT_CSV_HEADER      \
    "Timestamp [us]"          \
    ",RTC Count"              \
    ",Notes"                  \
    ",Ambient Light: Visible" \
    ",Ambient Light: IR"

CetiLightSample *g_light;
sem_t *light_data_ready;

static int s_log_restarted = 1;

int light_verify(void) {
    uint8_t manu_id, part_id, rev_id;

    WTResult result = als_get_manufacturer_id(&manu_id);

    if (result == WT_OK) {
        result = als_get_part_id(&part_id, &rev_id);
    }

    return ((result == WT_OK) && (manu_id == 0x05) && (part_id == 0x0a) && (rev_id == 0x00));
}

int init_light() {
    char err_str[512];
    int t_result = THREAD_OK;
    // setup shared memory
    g_light = create_shared_memory_region(LIGHT_SHM_NAME, sizeof(CetiLightSample));
    if (g_light == NULL) {
        CETI_ERR("Failed to create shared memory region: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    // setup semaphore
    light_data_ready = sem_open(LIGHT_SEM_NAME, O_CREAT, 0644, 0);
    if (light_data_ready == SEM_FAILED) {
        CETI_ERR("Failed to create semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }

    // Open an output file to write data.
    FILE *data_file = fopen(LIGHT_DATA_FILEPATH, "at");
    if (data_file == NULL) {
        CETI_ERR("Failed to open/create an output data file: " LIGHT_DATA_FILEPATH ": %s", strerror(errno));
        t_result |= THREAD_ERR_DATA_FILE_FAILED;
    } else {
        // There is a chance the file may be empty if a restart occured during
        // it's creation. Check if the file is empty, and add the header if it is empty (MSH)
        fseek(data_file, 0, SEEK_END);
        int size = ftell(data_file);
        if (size == 0) {
            fprintf(data_file, LIGHT_CSV_HEADER "\n");
        }
        fclose(data_file); // Close the file.
        s_log_restarted = 1;
        CETI_LOG("Using output data file: " LIGHT_DATA_FILEPATH);
    }

    g_light->error = als_wake();
    if (g_light->error != WT_OK) {
        CETI_ERR("Failed to initialize light sensor: %s", wt_strerror_r(g_light->error, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_HW;
    } else if (!light_verify()) {
        CETI_ERR("Could not verify light sensor");
        t_result |= THREAD_ERR_HW;
    } else {
        CETI_LOG("Successfully initialized the light sensor");
    }
    return t_result;
}

void light_update_sample(void) {
    // create sample
    g_light->sys_time_us = get_global_time_us();
    g_light->rtc_time_s = getRtcCount();
    g_light->error = als_get_measurement(&g_light->visible, &g_light->infrared);

    // push semaphore to indicate to user applications that new data is available
    sem_post(light_data_ready);
}

void light_sample_to_csv(FILE *fp, CetiLightSample *pSample) {
    // Write timing information.
    fprintf(fp, "%ld", g_light->sys_time_us);
    fprintf(fp, ",%d", g_light->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ",");
    if (s_log_restarted) {
        fprintf(fp, "Restarted! | ");
        s_log_restarted = 0;
    }
    if (g_light->error != 0) {
        char err_str[512];
        fprintf(fp, "ERROR(%s) | ", wt_strerror_r(g_light->error, err_str, sizeof(err_str)));
    }
    if (g_light->visible < -80 || g_light->infrared < -80) {
        fprintf(fp, "INVALID? | ");
    }

    // Write the sensor data.
    fprintf(fp, ",%d", g_light->visible);
    fprintf(fp, ",%d", g_light->infrared);
    // Finish the row of data and close the file.
    fprintf(fp, "\n");
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *light_thread(void *paramPtr) {
    AcqDecay decay = decay_new(5);
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_light_thread_tid = gettid();

    if ((g_light == NULL) || (light_data_ready == SEM_FAILED)) {
        CETI_ERR("Thread started without neccesary memory resources");
        g_light_thread_is_running = 0;
        CETI_ERR("Thread terminated");
        return NULL;
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    g_light_thread_is_running = 1;
    while (!g_stopAcquisition) {
        int64_t task_start_us = get_monotonic_time_us();
        if (!decay_shouldSample(&decay)) {
            usleep(LIGHT_SAMPLING_PERIOD_US);
            continue;
        }

        // Acquire timing and sensor information as close together as possible.
        light_update_sample();

        update_thread_device_status(THREAD_ALS_ACQ, g_light->error, __FUNCTION__);
        decay_update(&decay, g_light->error);

        if (!g_stopLogging) {
            FILE *light_data_file = fopen(LIGHT_DATA_FILEPATH, "at");
            if (light_data_file == NULL) {
                CETI_LOG("failed to open data output file: %s", LIGHT_DATA_FILEPATH);
            } else {
                light_sample_to_csv(light_data_file, g_light);
                fclose(light_data_file);
            }
        }

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        int64_t elapsed_time_us = get_monotonic_time_us() - task_start_us;
        int64_t polling_sleep_duration_us = LIGHT_SAMPLING_PERIOD_US - elapsed_time_us;
        if (polling_sleep_duration_us > 0)
            usleep(polling_sleep_duration_us);
    }

    als_sleep();

    sem_close(light_data_ready);
    sem_unlink(LIGHT_SEM_NAME);

    munmap(g_light, sizeof(CetiLightSample));
    shm_unlink(ECG_SHM_NAME);
    g_light = NULL;

    g_light_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
