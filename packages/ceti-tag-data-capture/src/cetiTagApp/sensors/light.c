//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "light.h"

#include "../utils/memory.h"

#include <fcntl.h>
#include <semaphore.h>

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

#define LIGHT_TRY_OPEN(fd)                                 \
    if ((fd = i2cOpen(1, ADDR_LIGHT, 0)) < 0) {            \
        CETI_ERR("Failed to connect to the light sensor"); \
        return -1;                                         \
    }

// Global/static variables
int g_light_thread_is_running = 0;
static FILE *light_data_file = NULL;
static char light_data_file_notes[256] = "";
static const char *light_data_file_headers[] = {
    "Ambient Light: Visible",
    "Ambient Light: IR",
};
static const int num_light_data_file_headers = sizeof(light_data_file_headers) / sizeof(*light_data_file_headers);
CetiLightSample *g_light;
sem_t *light_data_ready;

int init_light() {
    if (light_wake() != 0) {
        CETI_ERR("Failed to wake light sensor");
        return -1;
    }
    CETI_LOG("Successfully initialized the light sensor");

    // setup shared memory
    g_light = create_shared_memory_region(LIGHT_SHM_NAME, sizeof(CetiLightSample));

    // setup semaphore
    light_data_ready = sem_open(LIGHT_SEM_NAME, O_CREAT, 0644, 0);
    if (light_data_ready == SEM_FAILED) {
        perror("sem_open");
        CETI_ERR("Failed to create semaphore");
        return -1;
    }

    // Open an output file to write data.
    if (init_data_file(light_data_file, LIGHT_DATA_FILEPATH,
                       light_data_file_headers, num_light_data_file_headers,
                       light_data_file_notes, "init_light()") < 0)
        return -1;

    return 0;
}

void light_update_sample(void) {
    // create sample
    g_light->sys_time_us = get_global_time_us();
    g_light->rtc_time_s = getRtcCount();
    g_light->error = getAmbientLight(&g_light->visible, &g_light->infrared);

    // push semaphore to indicate to user applications that new data is available
    sem_post(light_data_ready);
}

void light_sample_to_csv(FILE *fp, CetiLightSample *pSample) {
    // Write timing information.
    fprintf(fp, "%ld", g_light->sys_time_us);
    fprintf(fp, ",%d", g_light->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ",%s", light_data_file_notes);
    if (g_light->error != 0)
        fprintf(fp, "ERROR | ");
    if (g_light->visible < -80 || g_light->infrared < -80) {
        CETI_WARN("Readings are likely invalid");
        fprintf(fp, "INVALID? | ");
    }
    light_data_file_notes[0] = '\0';
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
            CETI_WARN("Failed to set affinity to CPU %d", LIGHT_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    int64_t polling_sleep_duration_us;
    g_light_thread_is_running = 1;
    while (!g_stopAcquisition) {
        // Acquire timing and sensor information as close together as possible.
        light_update_sample();

        if (!g_stopLogging) {
            light_data_file = fopen(LIGHT_DATA_FILEPATH, "at");
            if (light_data_file == NULL) {
                CETI_LOG("failed to open data output file: %s", LIGHT_DATA_FILEPATH);
            } else {
                light_sample_to_csv(light_data_file, g_light);
                fclose(light_data_file);
            }
        }

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = LIGHT_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - g_light->sys_time_us;
        if (polling_sleep_duration_us > 0)
            usleep(polling_sleep_duration_us);
    }
    g_light_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Ambient light sensor LiteON LTR-329ALS-01 Interface
//-----------------------------------------------------------------------------

int getAmbientLight(int *pAmbientLightVisible, int *pAmbientLightIR) {
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

    if ((result = i2cReadWordData(fd, 0x87)) < 0) {
        CETI_ERR("Failed to read light sensor");
        i2cClose(fd);
        return result;
    }

    manu_id = (uint8_t)result;
    if (manu_id != 0x05) {
        i2cClose(fd);
        return 1;
    }

    if ((result = i2cReadWordData(fd, 0x86)) < 0) {
        CETI_ERR("Failed to read light sensor");
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

    // part part verified
    return 0;
}

int light_wake(void) {
    int fd;

    LIGHT_TRY_OPEN(fd);
    return i2cWriteByteData(fd, 0x80, 0x1); // wake the light sensor up
    i2cClose(fd);
}
