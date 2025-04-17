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
#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/thread_error.h"
#include "../utils/timing.h" // for timestamps

#include "../device/bno086.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h> // for fmin()
#include <math.h> // for fmin(), sqrt(), atan2(), M_PI
#include <pigpio.h>
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
int g_imu_thread_is_running = 0;

static int imu_is_connected = 0;
static uint8_t imu_sequence_numbers[6] = {0}; // Each of the 6 channels has a sequence number (a message counter)

static CetiImuReportBuffer *imu_report_buffer;

// semaphore to indicate that shared memory has bee updated
static sem_t *s_imu_report_ready;
static sem_t *s_imu_page_ready;

//-----------------------------------------------------------------------------
// Acquisition
//-----------------------------------------------------------------------------
int setupIMU(uint8_t enabled_features) {
    // Reset the IMU
    if (imu_is_connected) {
        bno086_close();
    }
    imu_is_connected = 0;

    // Open an I2C connection.
    int retval = bno086_open();
    if (retval != WT_OK) {
        CETI_ERR("Failed to connect to the IMU\n");
        imu_is_connected = 0;
        return -1;
    }

    imu_is_connected = 1;
    CETI_LOG("IMU connection opened\n");

    // Reset the message counters for each channel.
    for (int channel_index = 0; channel_index < sizeof(imu_sequence_numbers) / sizeof(uint8_t); channel_index++)
        imu_sequence_numbers[channel_index] = 0;

    // Enable desired feature reports.
    if (enabled_features & IMU_QUAT_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_ROTATION_VECTOR, IMU_QUATERNION_SAMPLE_PERIOD_US);
    }

    if (enabled_features & IMU_ACCEL_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_ACCELEROMETER, IMU_9DOF_SAMPLE_PERIOD_US);
    }
    if (enabled_features & IMU_GYRO_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED, IMU_9DOF_SAMPLE_PERIOD_US);
    }
    if (enabled_features & IMU_MAG_ENABLED) {
        imu_enable_feature_report(IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED, IMU_9DOF_SAMPLE_PERIOD_US);
    }

    return 0;
}

int init_imu(void) {
    char err_str[512];
    int t_result = 0;
    // setup hardware
    if (setupIMU(IMU_ALL_ENABLED) < 0) {
        CETI_ERR("Failed to set up the IMU");
        t_result |= THREAD_ERR_HW;
    }

    // setup shared memory regions
    imu_report_buffer = create_shared_memory_region(IMU_REPORT_BUFFER_SHM_NAME, sizeof(CetiImuReportBuffer));
    if (imu_report_buffer == NULL) {
        CETI_ERR("Failed to create shared memory " IMU_REPORT_BUFFER_SHM_NAME ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    // setup semaphores
    s_imu_report_ready = sem_open(IMU_REPORT_SEM_NAME, O_CREAT, 0644, 0);
    if (s_imu_report_ready == SEM_FAILED) {
        CETI_ERR("Failed to create quaternion semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }
    s_imu_page_ready = sem_open(IMU_PAGE_SEM_NAME, O_CREAT, 0644, 0);
    if (s_imu_page_ready == SEM_FAILED) {
        CETI_ERR("Failed to create acceleration semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }

    if (t_result == THREAD_OK) {
        CETI_LOG("Successfully initialized the IMU");
    }

    return t_result;
}

//-----------------------------------------------------------------------------
// Acquisition
//-----------------------------------------------------------------------------
void *imu_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_imu_thread_tid = gettid();

    if ((imu_report_buffer == NULL) || (s_imu_report_ready == SEM_FAILED) || (s_imu_page_ready == SEM_FAILED)) {
        CETI_ERR("Thread started without neccesary memory resources");
        bno086_open(); // seems nice to stop the feature reports
        bno086_close();
        imu_is_connected = 0;
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
    long long start_global_time_us = get_global_time_us();
    g_imu_thread_is_running = 1;

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

        imu_read_data();

        // Note that no delay is added here to set the polling frequency,
        //  since the IMU feature reports will control the sampling rate.
    }
    bno086_open(); // seems nice to stop the feature reports
    bno086_close();
    imu_is_connected = 0;

    sem_close(s_imu_page_ready);
    sem_close(s_imu_report_ready);

    munmap(imu_report_buffer, sizeof(CetiImuReportBuffer));

    g_imu_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// BNO080 Interface
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

int imu_enable_feature_report(int report_id, uint32_t report_interval_us) {
    uint8_t setFeatureCommand[21] = {0};
    ShtpHeader shtpHeader = {0};

    // Populate the SHTP header (see 2.2.1 of "Sensor Hub Transport Protocol")
    setFeatureCommand[0] = sizeof(setFeatureCommand) & 0xFF; // Packet length LSB
    setFeatureCommand[1] = sizeof(setFeatureCommand) >> 8;   // Packet length MSB
    setFeatureCommand[2] = IMU_CHANNEL_CONTROL;
    setFeatureCommand[3] = imu_sequence_numbers[IMU_CHANNEL_CONTROL]++; // sequence number for this channel

    // Populate the Set Feature Command (see 6.5.4 of "SH-2 Reference Manual")
    setFeatureCommand[4] = IMU_SHTP_REPORT_SET_FEATURE_COMMAND;
    setFeatureCommand[5] = report_id;
    setFeatureCommand[6] = 0; // feature flags
    setFeatureCommand[7] = 0; // change sensitivity LSB
    setFeatureCommand[8] = 0; // change sensitivity MSB
    // Set the report interval in microseconds, as 4 bytes with LSB first
    for (int interval_byte_index = 0; interval_byte_index < 4; interval_byte_index++) {
        setFeatureCommand[9 + interval_byte_index] = (report_interval_us >> (8 * interval_byte_index)) & 0xFF;
    }
    setFeatureCommand[13] = 0; // batch interval LSB
    setFeatureCommand[14] = 0;
    setFeatureCommand[15] = 0;
    setFeatureCommand[16] = 0; // batch interval MSB
    setFeatureCommand[17] = 0; // sensor-specific configuration word LSB
    setFeatureCommand[18] = 0;
    setFeatureCommand[19] = 0;
    setFeatureCommand[20] = 0; // sensor-specific configuration word MSB

    // Write the command to enable the feature report.
    int retval = bno086_write(setFeatureCommand, sizeof(setFeatureCommand));
    if (retval < 0) {
        char err_str[512];
        CETI_ERR("I2C write failed enabling report %d: %s", report_id, wt_strerror_r(retval, err_str, sizeof(err_str)));
        return -1;
    }

    // Read a response header.
    retval = bno086_read_header(&shtpHeader);
    if (retval != WT_OK) {
        char err_str[512];
        CETI_ERR("I2C read failed enabling report %d: %s", report_id, wt_strerror_r(retval, err_str, sizeof(err_str)));
        return -1;
    }
    CETI_LOG("Enabled report %d.  Header is { length: %d, channel: %d, sequence_number: %d}",
             report_id, shtpHeader.length, shtpHeader.channel, shtpHeader.seq_num);

    return 0;
}

//-----------------------------------------------------------------------------

int imu_read_data() {
    int numBytesAvail;
    uint8_t pktBuff[256] = {0};
    ShtpHeader shtpHeader = {0};
    int retval;

    // Read a header to see how many bytes are available.
    long long global_time_us = get_global_time_us();
    int rtc_count = getRtcCount();
    int32_t timestamp_delay = 0;
    CetiImuReport *i_buffer = &imu_report_buffer->reports[imu_report_buffer->page][imu_report_buffer->sample];
    retval = bno086_read_header(&shtpHeader);
    numBytesAvail = fmin(256, (shtpHeader.length & 0x7FFF)); // msb is "continuation bit", not part of count
    if (retval == WT_OK) {
        retval = bno086_read_reports(pktBuff, numBytesAvail);
    }

    // check that no errors occured
    if ((retval != WT_OK)) { // make sure we have the right channel
        // log error for all imu reports
        i_buffer->sys_time_us = global_time_us;
        i_buffer->rtc_time_s = rtc_count;
        i_buffer->reading_delay = timestamp_delay;
        i_buffer->error = retval;
        imu_report_buffer->sample++;
        if (imu_report_buffer->sample == IMU_REPORT_BUFFER_SIZE) {
            imu_report_buffer->sample = 0;
            imu_report_buffer->page ^= 1;
            sem_post(s_imu_page_ready);
        }
        sem_post(s_imu_report_ready);
        return -1;
    }

    // Parse the data.
    size_t read_offset = 0;
    ShtpHeader *report_header = (ShtpHeader *)&pktBuff[read_offset];
    if (report_header->channel != IMU_CHANNEL_REPORTS) {
        return -1;
    }
    read_offset += sizeof(ShtpHeader);
    while (read_offset != report_header->length) {
        i_buffer = &imu_report_buffer->reports[imu_report_buffer->page][imu_report_buffer->sample];
        uint8_t report_id = pktBuff[read_offset];
        switch (report_id) {
            case IMU_SHTP_REPORT_BASE_TIMESTAMP: {
                timestamp_delay = ((ShtpTimebaseReport *)&pktBuff[read_offset])->delay;
                read_offset += 5;
                break;
            }

            case IMU_SENSOR_REPORTID_ACCELEROMETER: {
                i_buffer->sys_time_us = global_time_us;
                i_buffer->rtc_time_s = rtc_count;
                i_buffer->reading_delay = timestamp_delay;
                i_buffer->error = retval;
                memcpy(&i_buffer->report, &pktBuff[read_offset], sizeof(CetiImuAccelReport));
                // memcpy(imu_accel_m_ss, i_buffer, sizeof(CetiImuAccelSample));
                // sem_post(s_accel_ready);
                imu_report_buffer->sample++;
                if (imu_report_buffer->sample == IMU_REPORT_BUFFER_SIZE) {
                    imu_report_buffer->sample = 0;
                    imu_report_buffer->page ^= 1;
                    sem_post(s_imu_page_ready);
                }
                sem_post(s_imu_report_ready);
                read_offset += 10;
                break;
            }

            case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: {
                i_buffer->sys_time_us = global_time_us;
                i_buffer->rtc_time_s = rtc_count;
                i_buffer->reading_delay = timestamp_delay;
                i_buffer->error = retval;
                memcpy(&i_buffer->report, &pktBuff[read_offset], sizeof(CetiImuGyroReport));
                // memcpy(imu_gyro_rad_s, i_buffer, sizeof(CetiImuGyroSample));
                // sem_post(s_gyro_ready);
                imu_report_buffer->sample++;
                if (imu_report_buffer->sample == IMU_REPORT_BUFFER_SIZE) {
                    imu_report_buffer->sample = 0;
                    imu_report_buffer->page ^= 1;
                    sem_post(s_imu_page_ready);
                }
                sem_post(s_imu_report_ready);
                read_offset += 10;
                break;
            }

            case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: {
                i_buffer->sys_time_us = global_time_us;
                i_buffer->rtc_time_s = rtc_count;
                i_buffer->reading_delay = timestamp_delay;
                i_buffer->error = retval;
                memcpy(&i_buffer->report, &pktBuff[read_offset], sizeof(CetiImuMagReport));
                // memcpy(imu_mag_ut, i_buffer, sizeof(CetiImuMagSample));
                // sem_post(s_mag_ready);
                imu_report_buffer->sample++;
                if (imu_report_buffer->sample == IMU_REPORT_BUFFER_SIZE) {
                    imu_report_buffer->sample = 0;
                    imu_report_buffer->page ^= 1;
                    sem_post(s_imu_page_ready);
                }
                sem_post(s_imu_report_ready);
                read_offset += 10;
                break;
            }

            case IMU_SENSOR_REPORTID_ROTATION_VECTOR: {
                i_buffer->sys_time_us = global_time_us;
                i_buffer->rtc_time_s = rtc_count;
                i_buffer->reading_delay = timestamp_delay;
                i_buffer->error = retval;
                memcpy(&i_buffer->report, &pktBuff[read_offset], sizeof(CetiImuQuatReport));
                // memcpy(imu_quaternion, i_buffer, sizeof(CetiImuQuatSample));
                // sem_post(s_quat_ready);
                imu_report_buffer->sample++;
                if (imu_report_buffer->sample == IMU_REPORT_BUFFER_SIZE) {
                    imu_report_buffer->sample = 0;
                    imu_report_buffer->page ^= 1;
                    sem_post(s_imu_page_ready);
                }
                sem_post(s_imu_report_ready);
                read_offset += 14;
                break;
            }

            default: {
                read_offset = report_header->length;
                break;
            }
        }
    }

    return (int)0;
}
