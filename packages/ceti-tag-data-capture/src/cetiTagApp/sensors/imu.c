//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "imu.h"
#include "../cetiTag.h"
#include "../device/bno08x.h"
#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/thread_error.h"
#include "../utils/timing.h" // for timestamps

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>    // for fmin()
#include <math.h>    // for fmin(), sqrt(), atan2(), M_PI
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> // for usleep()

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
typedef enum imu_data_type_e {
    IMU_DATA_TYPE_QUAT,
    IMU_DATA_TYPE_ACCEL,
    IMU_DATA_TYPE_GYRO,
    IMU_DATA_TYPE_MAG,
    IMU_DATA_TYPE_COUNT,
} IMUDataType;

int g_imu_thread_is_running = 0;
#define IMU_MAX_FILEPATH_LENGTH 100
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
static int imu_is_connected = 0;
static bool imu_restarted_log[IMU_DATA_TYPE_COUNT] = {true, true, true, true};
static bool imu_new_log[IMU_DATA_TYPE_COUNT] = {true, true, true, true};

// ToDo: consider buffering samples for slower read.
// latest sample pointers (shared memory)
static CetiImuQuatSample *imu_quaternion;
static CetiImuAccelSample *imu_accel_m_ss;
static CetiImuGyroSample *imu_gyro_rad_s;
static CetiImuMagSample *imu_mag_ut;

// semaphore to indicate that shared memory has bee updated
static sem_t *s_quat_ready;
static sem_t *s_accel_ready;
static sem_t *s_gyro_ready;
static sem_t *s_mag_ready;

int init_imu() {
    char err_str[512];
    int t_result = 0;
    if (setupIMU(IMU_ALL_ENABLED) < 0) {
        CETI_ERR("Failed to set up the IMU");
        t_result |= THREAD_ERR_HW;
    }

    for (int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
        imu_new_log[i] = true;
    }

    // setup shared memory regions
    imu_quaternion = create_shared_memory_region(IMU_QUAT_SHM_NAME, sizeof(CetiImuQuatSample));
    if (imu_quaternion == NULL) {
        CETI_ERR("Failed to create shared memory " IMU_QUAT_SHM_NAME ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    imu_accel_m_ss = create_shared_memory_region(IMU_ACCEL_SHM_NAME, sizeof(CetiImuAccelSample));
    if (imu_accel_m_ss == NULL) {
        CETI_ERR("Failed to create shared memory " IMU_ACCEL_SHM_NAME ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    imu_gyro_rad_s = create_shared_memory_region(IMU_GYRO_SHM_NAME, sizeof(CetiImuGyroSample));
    if (imu_gyro_rad_s == NULL) {
        CETI_ERR("Failed to create shared memory " IMU_GYRO_SHM_NAME ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    imu_mag_ut = create_shared_memory_region(IMU_MAG_SHM_NAME, sizeof(CetiImuMagSample));
    if (imu_mag_ut == NULL) {
        CETI_ERR("Failed to create shared memory " IMU_MAG_SHM_NAME ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    // setup semaphores
    s_quat_ready = sem_open(IMU_QUAT_SEM_NAME, O_CREAT, 0644, 0);
    if (s_quat_ready == SEM_FAILED) {
        CETI_ERR("Failed to create quaternion semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }
    s_accel_ready = sem_open(IMU_ACCEL_SEM_NAME, O_CREAT, 0644, 0);
    if (s_accel_ready == SEM_FAILED) {
        CETI_ERR("Failed to create acceleration semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }
    s_gyro_ready = sem_open(IMU_GYRO_SEM_NAME, O_CREAT, 0644, 0);
    if (s_gyro_ready == SEM_FAILED) {
        CETI_ERR("Failed to create gyroscope semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }
    s_mag_ready = sem_open(IMU_MAG_SEM_NAME, O_CREAT, 0644, 0);
    if (s_mag_ready == SEM_FAILED) {
        CETI_ERR("Failed to create magnetometer semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }

    // Open an output file to write data.
    if (imu_init_data_files() < 0) {
        CETI_ERR("Failed to open imu data_files: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_DATA_FILE_FAILED;
    }

    if (t_result == THREAD_OK) {
        CETI_LOG("Successfully initialized the IMU");
    }

    return t_result;
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
        imu_restarted_log[i] = true;
    }
    return init_data_file_success;
}

// close all imu data files
void imu_close_all_files(void) {
    for (int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
        if (imu_data_file[i] != NULL) {
            fclose(imu_data_file[i]);
            imu_data_file[i] = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void imu_quat_sample_to_csv(FILE *fp, CetiImuQuatSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
    fprintf(fp, ","); // notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_QUAT]) {
        fprintf(fp, "Restarted |");
        imu_restarted_log[IMU_DATA_TYPE_QUAT] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_QUAT]) {
        fprintf(fp, "New log file! | ");
        imu_new_log[IMU_DATA_TYPE_QUAT] = false;
    }
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    fprintf(fp, ",%d", pSample->i);
    fprintf(fp, ",%d", pSample->j);
    fprintf(fp, ",%d", pSample->k);
    fprintf(fp, ",%d", pSample->real);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void imu_accel_sample_to_csv(FILE *fp, CetiImuAccelSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
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
    // Write the sensor reading delay.
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    // Write accelerometer data
    fprintf(fp, ",%d", pSample->x);
    fprintf(fp, ",%d", pSample->y);
    fprintf(fp, ",%d", pSample->z);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void imu_gyro_sample_to_csv(FILE *fp, CetiImuGyroSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
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
    // Write the sensor reading delay.
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    // Write accelerometer data
    fprintf(fp, ",%d", pSample->x);
    fprintf(fp, ",%d", pSample->y);
    fprintf(fp, ",%d", pSample->z);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void imu_mag_sample_to_csv(FILE *fp, CetiImuMagSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
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
    // Write the sensor reading delay.
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    // Write accelerometer data
    fprintf(fp, ",%d", pSample->x);
    fprintf(fp, ",%d", pSample->y);
    fprintf(fp, ",%d", pSample->z);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void *imu_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_imu_thread_tid = gettid();

    if ((imu_quaternion == NULL) || (imu_accel_m_ss == NULL) || (imu_gyro_rad_s == NULL) || (imu_mag_ut == NULL) || (s_quat_ready == SEM_FAILED) || (s_accel_ready == SEM_FAILED) || (s_gyro_ready == SEM_FAILED) || (s_mag_ready == SEM_FAILED)) {
        CETI_ERR("Thread started without neccesary memory resources");
        wt_bno08x_reset_hard(); // seems nice to stop the feature reports
        wt_bno08x_close();
        imu_is_connected = 0;
        imu_close_all_files();
        g_imu_thread_is_running = 0;
        CETI_ERR("Thread terminated");
        return NULL;
    }

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

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    IMUDataType report_type;
    int report_id_updated = -1;
    long long global_time_us = get_global_time_us();
    long long start_global_time_us = get_global_time_us();
    long long imu_last_data_file_flush_time_us = get_global_time_us();
    g_imu_thread_is_running = 1;
    // open all files
    for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++)
        imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");

    while (!g_stopAcquisition) {
        if (!imu_is_connected) {
            // If there was an error, try to reset the IMU.
            // Ignore the initial few seconds though, since sometimes it takes a little bit to get in sync.
            if (get_global_time_us() - start_global_time_us > 10000000) {
                CETI_ERR("Unable to connect to IMU");
                usleep(5000000);
                setupIMU(IMU_ALL_ENABLED);
                start_global_time_us = get_global_time_us();
            }
            continue; // restart loop
        }

        report_id_updated = imu_read_data();

        // no data received yet
        if (report_id_updated == -1) {
            // timeout reached
            if ((get_global_time_us() - global_time_us > 5000000) && (get_global_time_us() - start_global_time_us > 10000000)) {
                CETI_ERR("Unable to reading from IMU");
                usleep(5000000);
                setupIMU(IMU_ALL_ENABLED);
                start_global_time_us = get_global_time_us();
            }
            usleep(2000); // Note that we are streaming 4 reports at 50 Hz, so we expect data to be available at about 200 Hz
            continue;     // restart loop
        }

        // Acquire timing information for this sample.
        global_time_us = get_global_time_us();

        // Check whether data was actually received,
        //  or whether the loop exited due to other conditions.
        if (report_id_updated < 0) {
            continue; // restart loop
        }

        switch (report_id_updated) {
            case IMU_SENSOR_REPORTID_ROTATION_VECTOR: report_type = IMU_DATA_TYPE_QUAT; break;
            case IMU_SENSOR_REPORTID_ACCELEROMETER: report_type = IMU_DATA_TYPE_ACCEL; break;
            case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: report_type = IMU_DATA_TYPE_GYRO; break;
            case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: report_type = IMU_DATA_TYPE_MAG; break;
            default: report_type = -1; break;
        }

        if (report_type == -1) {
            // unknown report type
            CETI_WARN("Unknown IMU report type: 0x%02x", report_id_updated);
            continue;
        }

        if (!g_stopLogging) {
            // check that all imu data files are open
            int unopened_file = false;
            for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT && !unopened_file; i_type++) {
                if (imu_data_file[i_type] == NULL) {
                    CETI_ERR("Failed to open data output file: %s", imu_data_filepath[i_type]);
                    imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");
                    unopened_file = (imu_data_file[i_type] == NULL);
                }
            }

            // A file failed to open
            if (unopened_file) {
                usleep(100000); // Sleep a bit before retrying.
                continue;       // restart loop
            }

            // buffer data for other applications
            switch (report_type) {
                case IMU_DATA_TYPE_QUAT: {
                    // copy data to shared buffer
                    imu_quat_sample_to_csv(imu_data_file[IMU_DATA_TYPE_QUAT], imu_quaternion);
                    break;
                }

                case IMU_DATA_TYPE_ACCEL: {
                    // ToDo: buffer samples?
                    imu_accel_sample_to_csv(imu_data_file[IMU_DATA_TYPE_ACCEL], imu_accel_m_ss);
                } break;
                case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: {
                    // ToDo: buffer samples?
                    imu_gyro_sample_to_csv(imu_data_file[IMU_DATA_TYPE_GYRO], imu_gyro_rad_s);
                } break;

                case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: {
                    // ToDo: buffer samples?
                    imu_mag_sample_to_csv(imu_data_file[IMU_DATA_TYPE_MAG], imu_mag_ut);
                } break;

                default:
                    break;
            }

            // Flush the files periodically.
            if ((get_global_time_us() - imu_last_data_file_flush_time_us >= IMU_DATA_FILE_FLUSH_PERIOD_US)) {
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

                // Record the flush time.
                imu_last_data_file_flush_time_us = get_global_time_us();
            }
        }

        // Note that no delay is added here to set the polling frequency,
        //  since the IMU feature reports will control the sampling rate.
    }
    wt_bno08x_reset_hard(); // seems nice to stop the feature reports
    wt_bno08x_close();
    imu_is_connected = 0;
    imu_close_all_files();

    sem_close(s_quat_ready);
    sem_close(s_accel_ready);
    sem_close(s_gyro_ready);
    sem_close(s_mag_ready);

    munmap(imu_quaternion, sizeof(CetiImuQuatSample));
    munmap(imu_accel_m_ss, sizeof(CetiImuAccelSample));
    munmap(imu_gyro_rad_s, sizeof(CetiImuGyroSample));
    munmap(imu_mag_ut, sizeof(CetiImuMagSample));
    g_imu_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// BNO080 Interface
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
int setupIMU(uint8_t enabled_features) {
    char err_str[512];

    // Reset the IMU
    if (imu_is_connected) {
        wt_bno08x_close();
    }
    imu_is_connected = 0;
    wt_bno08x_reset_hard();
#if IMU_REORIENTATION_ENABLE != 0
    wt_bno08x_set_system_orientation(IMU_REORITENTATION_W, IMU_REORITENTATION_X, IMU_REORITENTATION_Y, IMU_REORITENTATION_Z);
#endif
    // Open an I2C connection.
    WTResult retval = wt_bno08x_open();
    if (retval != WT_OK) {
        CETI_ERR("Failed to connect to the IMU: %s\n", wt_strerror_r(retval, err_str, sizeof(err_str)));
        return -1;
    }
    imu_is_connected = 1;
    CETI_LOG("IMU connection opened\n");

    // Enable desired feature reports.
    if (enabled_features & IMU_QUAT_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_ROTATION_VECTOR, IMU_QUATERNION_SAMPLE_PERIOD_US);
        usleep(100000);
    }

    if (enabled_features & IMU_ACCEL_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_ACCELEROMETER, IMU_9DOF_SAMPLE_PERIOD_US);
        usleep(100000);
    }
    if (enabled_features & IMU_GYRO_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED, IMU_9DOF_SAMPLE_PERIOD_US);
        usleep(100000);
    }
    if (enabled_features & IMU_MAG_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED, IMU_9DOF_SAMPLE_PERIOD_US);
        usleep(100000);
    }

    return 0;
}

int imu_enable_feature_report(int report_id, uint32_t report_interval_us) {
    ShtpHeader response_header = {0};
    WTResult hw_result = WT_OK;

    // Write the command to enable the feature report.
    hw_result = wt_bno08x_enable_report(report_id, report_interval_us);
    if (hw_result == WT_OK) {
        hw_result = wt_bno08x_read_header(&response_header);
    }
    // report any errors
    if (hw_result != WT_OK) {
        char err_str[512];
        CETI_ERR("BMS failed enabling report %d: %s", report_id, wt_strerror_r(hw_result, err_str, sizeof(err_str)));
        return -1;
    }

    CETI_LOG("Enabled report %d.  Header is 0x%04X, 0x%02X  0x%02X",
             report_id, response_header.length, response_header.channel, response_header.seq_num);

    return 0;
}

//-----------------------------------------------------------------------------
int imu_read_data() {
    int numBytesAvail;
    char pktBuff[256] = {0};
    WTResult hw_result = WT_OK;

    // Read available data.
    hw_result = wt_bno08x_read_shtp_packet(pktBuff, numBytesAvail);
    if ((hw_result != WT_OK) || pktBuff[2] != IMU_CHANNEL_REPORTS) { // make sure we have the right channel
        return -1;
    }

    long long global_time_us = get_global_time_us();
    int rtc_count = getRtcCount();
    uint8_t report_id = pktBuff[9];
    int32_t timestamp_delay_us = (int32_t)(((uint32_t)pktBuff[8] << (8 * 3)) | ((uint32_t)pktBuff[7] << (8 * 2)) | ((uint32_t)pktBuff[6] << (8 * 1)) | ((uint32_t)pktBuff[5] << (8 * 0)));

    // Parse the data in the report.
    uint8_t status = pktBuff[11] & 0x03; // Get status bits
    uint16_t data[3] = {
        ((uint16_t)pktBuff[14] << 8 | pktBuff[13]),
        ((uint16_t)pktBuff[16] << 8 | pktBuff[15]),
        ((uint16_t)pktBuff[18] << 8 | pktBuff[17])};

    switch (report_id) {
        case IMU_SENSOR_REPORTID_ROTATION_VECTOR: {
            imu_quaternion->sys_time_us = global_time_us;
            imu_quaternion->rtc_time_s = rtc_count;
            imu_quaternion->reading_delay_us = timestamp_delay_us;
            memcpy(&imu_quaternion->i, data, 3 * sizeof(int16_t));
            imu_quaternion->real = (uint16_t)pktBuff[20] << 8 | pktBuff[19];
            imu_quaternion->accuracy = (uint16_t)pktBuff[22] << 8 | pktBuff[21];
            sem_post(s_quat_ready);
            break;
        }
        case IMU_SENSOR_REPORTID_ACCELEROMETER: {
            imu_accel_m_ss->sys_time_us = global_time_us;
            imu_accel_m_ss->rtc_time_s = rtc_count;
            imu_accel_m_ss->reading_delay_us = timestamp_delay_us;
            memcpy(&imu_accel_m_ss->x, data, 3 * sizeof(int16_t));
            imu_accel_m_ss->accuracy = status;
            sem_post(s_accel_ready);
            break;
        }
        case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: {
            imu_gyro_rad_s->sys_time_us = global_time_us;
            imu_gyro_rad_s->rtc_time_s = rtc_count;
            imu_gyro_rad_s->reading_delay_us = timestamp_delay_us;
            memcpy(&imu_gyro_rad_s->x, data, 3 * sizeof(int16_t));
            imu_gyro_rad_s->accuracy = status;
            sem_post(s_gyro_ready);
            break;
        }
        case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: {
            imu_mag_ut->sys_time_us = global_time_us;
            imu_mag_ut->rtc_time_s = rtc_count;
            imu_mag_ut->reading_delay_us = timestamp_delay_us;
            memcpy(&imu_mag_ut->x, data, 3 * sizeof(int16_t));
            imu_mag_ut->accuracy = status;
            sem_post(s_mag_ready);
            break;
        }
    }
    return (int)report_id;
}
