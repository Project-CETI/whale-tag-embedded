//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "imu_log.h"

#include "../cetiTag.h"
#include "../launcher.h"
#include "../sensors/imu.h"
#include "../systemMonitor.h"
#include "../utils/error.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/timing.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// How often data is written to the to IMU log files
#define IMU_LOGGING_INTERVAL_US (1000000)

// Seems to log about 1GiB every 33 hours when nominally streaming quaternion
// at 20 Hz and accel/gyro/mag at 50 Hz Note that 2GB is the file size maximum
// for 32-bit systems
#define IMU_MAX_FILE_SIZE_MB 1024

#define IMU_MAX_FILEPATH_LENGTH 100

static CetiImuReportBuffer *imu_report_buffer;

int g_imu_log_thread_is_running = 0;

static bool imu_restarted_log[IMU_DATA_TYPE_COUNT] = {true, true, true, true};
static bool imu_new_log[IMU_DATA_TYPE_COUNT] = {true, true, true, true};

static char imu_data_filepath[IMU_DATA_TYPE_COUNT][IMU_MAX_FILEPATH_LENGTH];
static FILE *imu_data_file[IMU_DATA_TYPE_COUNT] = {
    [IMU_DATA_TYPE_QUAT] = NULL,
    [IMU_DATA_TYPE_ACCEL] = NULL,
    [IMU_DATA_TYPE_GYRO] = NULL,
    [IMU_DATA_TYPE_MAG] = NULL,
};
// These all compile to single continuous strings, aka why no comma between quotes.
static const char *imu_data_file_headers[IMU_DATA_TYPE_COUNT] = {
    [IMU_DATA_TYPE_QUAT] =
        "Sensor_delay_us"
        ",Quat_i"
        ",Quat_j"
        ",Quat_k"
        ",Quat_Re"
        ",Quat_accuracy",
    [IMU_DATA_TYPE_ACCEL] =
        "Sensor_delay_us"
        ",Accel_x_raw"
        ",Accel_y_raw"
        ",Accel_z_raw"
        ",Accel_status",
    [IMU_DATA_TYPE_GYRO] =
        "Sensor_delay_us"
        ",Gyro_x_raw"
        ",Gyro_y_raw"
        ",Gyro_z_raw"
        ",Gyro_status",
    [IMU_DATA_TYPE_MAG] =
        "Sensor_delay_us"
        ",Mag_x_raw"
        ",Mag_y_raw"
        ",Mag_z_raw"
        ",Mag_status",
};
static const char *imu_data_names[IMU_DATA_TYPE_COUNT] = {
    [IMU_DATA_TYPE_QUAT] = "quat",
    [IMU_DATA_TYPE_ACCEL] = "accel",
    [IMU_DATA_TYPE_GYRO] = "gyro",
    [IMU_DATA_TYPE_MAG] = "mag",
};

// close all imu data files
void imu_close_all_files(void) {
    for (int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
        if (imu_data_file[i] != NULL) {
            fclose(imu_data_file[i]);
            imu_data_file[i] = NULL;
        }
    }
}

// Find next available filename
int imu_init_data_files(void) {
    // Append a number to the filename base until one is found that doesn't exist yet.
    static int data_file_postfix_count = 0; // static to retain number last file index
    int data_file_exists = 0;

    do {
        data_file_exists = 0;
        // check that all filenames don't already exist
        for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT && !data_file_exists; i_type++) {
            snprintf(imu_data_filepath[i_type], IMU_MAX_FILEPATH_LENGTH, IMU_DATA_FILEPATH_BASE "_%s_%02d.csv", imu_data_names[i_type], data_file_postfix_count);
            data_file_exists = (access(imu_data_filepath[i_type], F_OK) != -1);
        }
        data_file_postfix_count++;
    } while (data_file_exists);

    int init_data_file_success = 0;
    for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
        // Open the new files
        init_data_file_success |= init_data_file(imu_data_file[i_type], imu_data_filepath[i_type],
                                                 &imu_data_file_headers[i_type], 1,
                                                 NULL, "imu_init_data_files()");
    }

    for (int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
        imu_new_log[i] = true;
    }

    for (int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
        imu_restarted_log[i] = true;
    }

    // Open the files
    for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
        imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");
        if (imu_data_file[i_type] == NULL) {
            imu_close_all_files();
            return -1;
        }
    }

    return init_data_file_success;
}

void imu_log_report_to_quat_csv(FILE *fp, CetiImuReport *pReport) {
    fprintf(fp, "%ld", pReport->sys_time_us);
    fprintf(fp, ",%d", pReport->rtc_time_s);

    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); // notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_QUAT]) {
        fprintf(fp, "Restarted |");
        imu_restarted_log[IMU_DATA_TYPE_QUAT] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_QUAT]) {
        fprintf(fp, "New log file! | ");
        imu_new_log[IMU_DATA_TYPE_QUAT] = false;
    }
    if (pReport->error != WT_OK) {
        char err_str[512];
        fprintf(fp, "ERROR(%s) | ", wt_strerror_r(pReport->error, err_str, sizeof(err_str)));
        fprintf(fp, ", , , , , , \n");
    } else {
        // Write the sensor reading delay.
        fprintf(fp, ",%d", (pReport->reading_delay + pReport->report.delay) * 100);
        // Write accelerometer data
        fprintf(fp, ",%d", pReport->report.quat.i);
        fprintf(fp, ",%d", pReport->report.quat.j);
        fprintf(fp, ",%d", pReport->report.quat.k);
        fprintf(fp, ",%d", pReport->report.quat.real);
        fprintf(fp, ",%d", pReport->report.quat.accuracy);
    }
    fprintf(fp, "\n");
}

void imu_log_report_to_accel_csv(FILE *fp, CetiImuReport *pReport) {
    fprintf(fp, "%ld", pReport->sys_time_us);
    fprintf(fp, ",%d", pReport->rtc_time_s);

    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); // notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_ACCEL]) {
        fprintf(fp, "Restarted |");
        imu_restarted_log[IMU_DATA_TYPE_ACCEL] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_ACCEL]) {
        fprintf(fp, "New log file! | ");
        imu_new_log[IMU_DATA_TYPE_ACCEL] = false;
    }
    if (pReport->error != WT_OK) {
        char err_str[512];
        fprintf(fp, "ERROR(%s) | ", wt_strerror_r(pReport->error, err_str, sizeof(err_str)));
        fprintf(fp, ", , , , , \n");
    } else {
        // Write the sensor reading delay.
        fprintf(fp, ",%d", pReport->reading_delay + pReport->report.delay * 100);
        // Write accelerometer data
        fprintf(fp, ",%d", pReport->report.accel.x);
        fprintf(fp, ",%d", pReport->report.accel.y);
        fprintf(fp, ",%d", pReport->report.accel.z);
        fprintf(fp, ",%d", pReport->report.status);
    }
    fprintf(fp, "\n");
}

void imu_log_report_to_gyro_csv(FILE *fp, CetiImuReport *pReport) {
    fprintf(fp, "%ld", pReport->sys_time_us);
    fprintf(fp, ",%d", pReport->rtc_time_s);

    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); // notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_GYRO]) {
        fprintf(fp, "Restarted |");
        imu_restarted_log[IMU_DATA_TYPE_GYRO] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_GYRO]) {
        fprintf(fp, "New log file! | ");
        imu_new_log[IMU_DATA_TYPE_GYRO] = false;
    }
    if (pReport->error != WT_OK) {
        char err_str[512];
        fprintf(fp, "ERROR(%s) | ", wt_strerror_r(pReport->error, err_str, sizeof(err_str)));
        fprintf(fp, ", , , , , \n");
    } else {
        // Write the sensor reading delay.
        fprintf(fp, ",%d", pReport->reading_delay + pReport->report.delay * 100);
        // Write accelerometer data
        fprintf(fp, ",%d", pReport->report.gyro.x);
        fprintf(fp, ",%d", pReport->report.gyro.y);
        fprintf(fp, ",%d", pReport->report.gyro.z);
        fprintf(fp, ",%d", pReport->report.gyro.status);
    }
    fprintf(fp, "\n");
}

void imu_log_report_to_mag_csv(FILE *fp, CetiImuReport *pReport) {
    fprintf(fp, "%ld", pReport->sys_time_us);
    fprintf(fp, ",%d", pReport->rtc_time_s);

    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); // notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_MAG]) {
        fprintf(fp, "Restarted |");
        imu_restarted_log[IMU_DATA_TYPE_MAG] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_MAG]) {
        fprintf(fp, "New log file! | ");
        imu_new_log[IMU_DATA_TYPE_MAG] = false;
    }
    if (pReport->error != WT_OK) {
        char err_str[512];
        fprintf(fp, "ERROR(%s) | ", wt_strerror_r(pReport->error, err_str, sizeof(err_str)));
        fprintf(fp, ", , , , , \n");
    } else {
        // Write the sensor reading delay.
        fprintf(fp, ",%d", pReport->reading_delay + pReport->report.delay * 100);
        // Write accelerometer data
        fprintf(fp, ",%d", pReport->report.mag.x);
        fprintf(fp, ",%d", pReport->report.mag.y);
        fprintf(fp, ",%d", pReport->report.mag.z);
        fprintf(fp, ",%d", pReport->report.status);
    }
    fprintf(fp, "\n");
}

void *imu_log_thread(void *paramPtr) {
    g_imu_thread_writeData_tid = gettid();

    // Set the thread CPU affinity.
    if (IMU_CPU >= 0) {
        pthread_t thread;
        thread = pthread_self();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(IMU_CPU, &cpuset);
        if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
            CETI_LOG("Successfully set affinity to CPU %d", IMU_CPU);
        else
            CETI_ERR("Failed to set affinity to CPU %d", IMU_CPU);
    }

    // open shared memory object
    imu_report_buffer = shm_open_read(IMU_REPORT_BUFFER_SHM_NAME, sizeof(CetiImuReportBuffer));
    if (imu_report_buffer == NULL) {
        char err_str[512];
        CETI_ERR("Failed to create shared memory " IMU_REPORT_BUFFER_SHM_NAME ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        return NULL;
    }

    static uint32_t processing_page = 0;

    // open imu logging files
    while (imu_init_data_files()) {
        char error_str[512];
        strncpy(error_str, strerror(errno), sizeof(error_str) - 1);
        CETI_ERR("Failed to open data output file: %s", error_str);
        usleep(IMU_LOGGING_INTERVAL_US);
    }
    g_imu_log_thread_is_running = 1;
    CETI_LOG("Imu logging thread started Ok!");

    // begin logging
    while (!g_stopAcquisition) {
        int64_t log_time_us = get_global_time_us();
        if (g_stopLogging) {
            usleep(IMU_LOGGING_INTERVAL_US);
            continue;
        }

        // check that data is ready to be written
        if (imu_report_buffer->page == processing_page) {
            // buffer not full wait a bit longer
            usleep(IMU_LOGGING_INTERVAL_US / 10);
            continue;
        }

        // write all logged raw samples
        for (int i = 0; i < IMU_REPORT_BUFFER_SIZE; i++) {
            CetiImuReport *i_report = &imu_report_buffer->reports[processing_page][i];
            if ((i_report->report.report_id == IMU_SENSOR_REPORTID_ROTATION_VECTOR) || (i_report->error != WT_OK)) {
                imu_log_report_to_quat_csv(imu_data_file[IMU_DATA_TYPE_QUAT], i_report);
            }
            if ((i_report->report.report_id == IMU_SENSOR_REPORTID_ACCELEROMETER) || (i_report->error != WT_OK)) {
                imu_log_report_to_accel_csv(imu_data_file[IMU_DATA_TYPE_ACCEL], i_report);
            }
            if ((i_report->report.report_id == IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED) || (i_report->error != WT_OK)) {
                imu_log_report_to_gyro_csv(imu_data_file[IMU_DATA_TYPE_GYRO], i_report);
            }
            if ((i_report->report.report_id == IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED) || (i_report->error != WT_OK)) {
                imu_log_report_to_mag_csv(imu_data_file[IMU_DATA_TYPE_MAG], i_report);
            }
        }
        processing_page ^= 1;

        // Create new files if files are getting too large.
        size_t imu_data_file_size_b = 0;
        // Check the file sizes and close the files.
        for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
            fseek(imu_data_file[i_type], 0L, SEEK_END);
            size_t i_size = ftell(imu_data_file[i_type]);
            fflush(imu_data_file[i_type]);
            if (i_size > imu_data_file_size_b)
                imu_data_file_size_b = i_size;
        }
        // If any file size limit has been reached, start a new file.
        if ((imu_data_file_size_b >= (long)(IMU_MAX_FILE_SIZE_MB) * 1024L * 1024L || imu_data_file_size_b < 0) && !g_stopAcquisition) {
            imu_close_all_files();
            imu_init_data_files();

            // Open the files again.
            for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
                imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");
            }
        }

        // sleep
        int64_t elapsed_time_us = (get_global_time_us() - log_time_us);
        int64_t remaining_time_us = IMU_LOGGING_INTERVAL_US - elapsed_time_us;
        if (remaining_time_us >= 0) {
            usleep(remaining_time_us);
        }
    }

    // ToDo: Log partial pages

    imu_close_all_files();
    g_imu_log_thread_is_running = 0;
    CETI_LOG("Done!");

    return NULL;
}
