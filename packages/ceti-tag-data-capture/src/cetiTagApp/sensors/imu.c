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
#include <math.h> // for fmin(), sqrt(), atan2(), M_PI
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> // for usleep()

// IMU error handling constants
#define IMU_CONSECUTIVE_ERROR_THRESHOLD 5
#define IMU_MAX_RECOVERY_ATTEMPTS 3
#define IMU_ERROR_SLEEP_MS 500
#define IMU_RECOVERY_TIMEOUT_MS 2000
#define IMU_DEGRADED_MODE_LOG_INTERVAL 50

//-----------------------------------------------------------------------------
// Global/static variables
//-----------------------------------------------------------------------------
int g_imu_thread_is_running = 0;

static int imu_is_connected = 0;
static uint8_t imu_sequence_numbers[6] = {0}; // Each of the 6 channels has a sequence number (a message counter)

static CetiImuReportBuffer *imu_report_buffer;

// semaphore to indicate that shared memory has bee updated
static sem_t *s_imu_report_ready;
static sem_t *s_imu_page_ready;

// IMU error handling state
static struct {
    int consecutive_error_count;
    int recovery_attempt_count;
    uint64_t last_error_time_us;
    bool degraded_mode;
    WTResult last_error_code;
} s_imu_error_state = {0};

// Forward declaration for recovery functions
static WTResult __imu_attempt_hardware_recovery(void);
static void __imu_enter_degraded_mode(void);
static void __imu_reset_error_state(void);

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
        int64_t wake_time_us = get_global_time_us();

        // Read IMU data with enhanced error handling and recovery
        WTResult read_result = imu_read_data();
        if (read_result != WT_OK) {
            char err_str[512];
            CETI_ERR("IMU read error: %s", wt_strerror_r(read_result, err_str, sizeof(err_str)));
            
            // Track error timing and increment consecutive error count
            s_imu_error_state.last_error_time_us = get_global_time_us();
            s_imu_error_state.consecutive_error_count++;
            s_imu_error_state.last_error_code = read_result;
            
            CETI_WARN("IMU consecutive error count: %d/%d", 
                      s_imu_error_state.consecutive_error_count, 
                      IMU_CONSECUTIVE_ERROR_THRESHOLD);
            
            // Check if we've exceeded the error threshold
            if (s_imu_error_state.consecutive_error_count >= IMU_CONSECUTIVE_ERROR_THRESHOLD) {
                // Only attempt recovery if we haven't exhausted our attempts
                if (s_imu_error_state.recovery_attempt_count < IMU_MAX_RECOVERY_ATTEMPTS) {
                    CETI_WARN("IMU error threshold exceeded, attempting hardware recovery");
                    s_imu_error_state.recovery_attempt_count++;
                    
                    // Attempt hardware recovery
                    WTResult recovery_result = __imu_attempt_hardware_recovery();
                    if (recovery_result == WT_OK) {
                        CETI_LOG("IMU hardware recovery successful");
                        __imu_reset_error_state();
                        continue; // Resume normal operation
                    } else {
                        CETI_ERR("IMU hardware recovery failed");
                        // Continue to check if we should enter degraded mode
                    }
                }
                
                // If we've exhausted recovery attempts, enter degraded mode
                if (s_imu_error_state.recovery_attempt_count >= IMU_MAX_RECOVERY_ATTEMPTS) {
                    if (!s_imu_error_state.degraded_mode) {
                        __imu_enter_degraded_mode();
                    }
                    
                    // In degraded mode, reduce error logging frequency to avoid spam
                    if ((s_imu_error_state.consecutive_error_count % IMU_DEGRADED_MODE_LOG_INTERVAL) == 0) {
                        CETI_WARN("IMU still in degraded mode (%d errors)", 
                                  s_imu_error_state.consecutive_error_count);
                    }
                }
            }
            
            // Brief delay before continuing to avoid overwhelming the system
            usleep(IMU_ERROR_SLEEP_MS * 1000);
            continue;
        }
        
        // Successful data read - reset error state if we were in error
        if (s_imu_error_state.consecutive_error_count > 0) {
            CETI_LOG("IMU communication restored after %d errors", 
                     s_imu_error_state.consecutive_error_count);
            __imu_reset_error_state();
        }

        // It's ok to sleep as sensor reports will just get
        // concatenated by the sensor hardware.
        int64_t elapsed_time = get_global_time_us() - wake_time_us;
        int64_t remaining_time = IMU_9DOF_SAMPLE_PERIOD_US - elapsed_time;
        if (remaining_time > 0) {
            usleep(remaining_time);
        }
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

WTResult imu_read_data() {
    int numBytesAvail;
    uint8_t pktBuff[256] = {0};
    ShtpHeader shtpHeader = {0};
    WTResult retval;

    // Read a header to see how many bytes are available.
    long long global_time_us = get_global_time_us();
    int rtc_count = getRtcCount();
    int32_t timestamp_delay = 0;
    retval = bno086_read_header(&shtpHeader);
    if (retval == WT_OK) {
        numBytesAvail = fmin(256, (shtpHeader.length & 0x7FFF)); // msb is "continuation bit", not part of count
        if (numBytesAvail == 0) {
            return WT_RESULT(WT_DEV_IMU, WT_ERR_NO_DATA);
        }
        retval = bno086_read_reports(pktBuff, numBytesAvail);
    }

    // check that no errors occured
    CetiImuReport *i_buffer = &imu_report_buffer->reports[imu_report_buffer->page][imu_report_buffer->sample];
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
        return retval;
    }
    // Parse the data.
    size_t read_offset = 0;
    ShtpHeader *report_header = (ShtpHeader *)&pktBuff[read_offset];
    int bytes_read = report_header->length & 0x7FFF;
    if (report_header->channel != IMU_CHANNEL_REPORTS) {
        return WT_RESULT(WT_DEV_IMU, WT_ERR_INVALID_CHANNEL);
    }
    read_offset += sizeof(ShtpHeader);
    while (read_offset < bytes_read) {
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
                read_offset = bytes_read;
                break;
            }
        }
    }

    return WT_OK;
}

/**
 * @brief Reset IMU error handling state after successful operation
 */
static void __imu_reset_error_state(void) {
    s_imu_error_state.consecutive_error_count = 0;
    s_imu_error_state.recovery_attempt_count = 0;
    s_imu_error_state.degraded_mode = false;
    s_imu_error_state.last_error_code = WT_OK;
}

/**
 * @brief Enter degraded mode when recovery attempts have been exhausted
 */
static void __imu_enter_degraded_mode(void) {
    CETI_WARN("IMU entering degraded mode after %d consecutive errors", 
              s_imu_error_state.consecutive_error_count);
    s_imu_error_state.degraded_mode = true;
    
    // Send system notification about IMU degradation
    char err_str[512];
    CETI_ERR("IMU DEGRADED: %d ERRORS - Last error: %s", 
             s_imu_error_state.consecutive_error_count,
             wt_strerror_r(s_imu_error_state.last_error_code, err_str, sizeof(err_str)));
}

/**
 * @brief Attempt hardware recovery of the IMU
 *
 * @return WTResult WT_OK on success, error code on failure
 */
static WTResult __imu_attempt_hardware_recovery(void) {
    CETI_LOG("Attempting IMU hardware recovery (attempt %d/%d)", 
             s_imu_error_state.recovery_attempt_count, IMU_MAX_RECOVERY_ATTEMPTS);
    
    // Wait before attempting recovery to allow transient errors to clear
    usleep(IMU_ERROR_SLEEP_MS * 1000);
    
    // Attempt 1: Try I2C bus reset
    if (s_imu_error_state.recovery_attempt_count == 1) {
        CETI_LOG("IMU recovery attempt 1: I2C bus reset");
        
        // Close and reopen I2C connection
        if (imu_is_connected) {
            bno086_close();
            imu_is_connected = 0;
        }
        
        // Brief delay to allow hardware to settle
        usleep(100000); // 100ms
        
        // Reopen I2C connection
        WTResult open_result = bno086_open();
        if (open_result == WT_OK) {
            imu_is_connected = 1;
            
            // Reset sequence numbers
            for (int i = 0; i < sizeof(imu_sequence_numbers); i++) {
                imu_sequence_numbers[i] = 0;
            }
            
            CETI_LOG("IMU I2C bus reset successful");
            return WT_OK;
        } else {
            char err_str[512];
            CETI_ERR("IMU I2C bus reset failed: %s", wt_strerror_r(open_result, err_str, sizeof(err_str)));
            return open_result;
        }
    }
    
    // Attempt 2: Try full sensor reinitialization
    if (s_imu_error_state.recovery_attempt_count == 2) {
        CETI_LOG("IMU recovery attempt 2: Full sensor reinitialization");
        
        // Attempt complete reinitialization
        int setup_result = setupIMU(IMU_ALL_ENABLED);
        if (setup_result == 0) {
            CETI_LOG("IMU full reinitialization successful");
            return WT_OK;
        } else {
            CETI_ERR("IMU full reinitialization failed");
            return WT_RESULT(WT_DEV_IMU, WT_ERR_INIT_FAILED);
        }
    }
    
    // Attempt 3: Hardware reset (if available - future enhancement)
    if (s_imu_error_state.recovery_attempt_count == 3) {
        CETI_LOG("IMU recovery attempt 3: Hardware reset");
        
        // Close current connection
        if (imu_is_connected) {
            bno086_close();
            imu_is_connected = 0;
        }
        
        // Extended delay for hardware reset
        usleep(IMU_RECOVERY_TIMEOUT_MS * 1000);
        
        // Try complete reinitialization after reset
        int setup_result = setupIMU(IMU_ALL_ENABLED);
        if (setup_result == 0) {
            CETI_LOG("IMU hardware reset successful");
            return WT_OK;
        } else {
            CETI_ERR("IMU hardware reset failed");
            return WT_RESULT(WT_DEV_IMU, WT_ERR_HW_RESET_FAILED);
        }
    }
    
    // All recovery attempts exhausted
    return WT_RESULT(WT_DEV_IMU, WT_ERR_RECOVERY_FAILED);
}
