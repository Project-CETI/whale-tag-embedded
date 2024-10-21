//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef IMU_H
#define IMU_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "../cetiTag.h"       //for CetiImuQuatSample
#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"
#include "../utils/timing.h" // for timestamps

#include <inttypes.h>
#include <math.h> // for fmin()
#include <math.h> // for fmin(), sqrt(), atan2(), M_PI
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for usleep()

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
// Seems to log about 1GiB every 33 hours when nominally streaming quaternion
// at 20 Hz and accel/gyro/mag at 50 Hz Note that 2GB is the file size maximum
// for 32-bit systems
#define IMU_MAX_FILE_SIZE_MB 1024

// How often to close/reopen the data file (avoid doing it at every sample to
// reduce delays in the loop)
#define IMU_DATA_FILE_FLUSH_PERIOD_US 1000000

#define BUS_IMU 0x00 // IMU is only device on i2c0
#define ADDR_IMU 0x4A
#define IMU_N_RESET 4
// Bitbang IMU I2C
#define IMU_BB_I2C_SDA 23
#define IMU_BB_I2C_SCL 24

// Registers
#define IMU_CHANNEL_COMMAND 0
#define IMU_CHANNEL_EXECUTABLE 1
#define IMU_CHANNEL_CONTROL 2
#define IMU_CHANNEL_REPORTS 3
#define IMU_CHANNEL_WAKE_REPORTS 4
#define IMU_CHANNEL_GYRO 5

// All the ways we can configure or talk to the BNO080, figure 34, page 36
// reference manual These are used for low level communication with the sensor,
// on channel 2
#define IMU_SHTP_REPORT_COMMAND_RESPONSE 0xF1
#define IMU_SHTP_REPORT_COMMAND_REQUEST 0xF2
#define IMU_SHTP_REPORT_FRS_READ_RESPONSE 0xF3
#define IMU_SHTP_REPORT_FRS_READ_REQUEST 0xF4
#define IMU_SHTP_REPORT_PRODUCT_ID_RESPONSE 0xF8
#define IMU_SHTP_REPORT_PRODUCT_ID_REQUEST 0xF9
#define IMU_SHTP_REPORT_BASE_TIMESTAMP 0xFB
#define IMU_SHTP_REPORT_SET_FEATURE_COMMAND 0xFD

// All the different sensors and features we can get reports from
// These are used when enabling a given sensor
#define IMU_SENSOR_REPORTID_ACCELEROMETER 0x01
#define IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED 0x02
#define IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED 0x03
#define IMU_SENSOR_REPORTID_LINEAR_ACCELERATION 0x04
#define IMU_SENSOR_REPORTID_ROTATION_VECTOR 0x05
#define IMU_SENSOR_REPORTID_GRAVITY 0x06
#define IMU_SENSOR_REPORTID_GAME_ROTATION_VECTOR 0x08
#define IMU_SENSOR_REPORTID_GEOMAGNETIC_ROTATION_VECTOR 0x09
#define IMU_SENSOR_REPORTID_GYRO_INTEGRATED_ROTATION_VECTOR 0x2A
#define IMU_SENSOR_REPORTID_TAP_DETECTOR 0x10
#define IMU_SENSOR_REPORTID_STEP_COUNTER 0x11
#define IMU_SENSOR_REPORTID_STABILITY_CLASSIFIER 0x13
#define IMU_SENSOR_REPORTID_RAW_ACCELEROMETER 0x14
#define IMU_SENSOR_REPORTID_RAW_GYROSCOPE 0x15
#define IMU_SENSOR_REPORTID_RAW_MAGNETOMETER 0x16
#define IMU_SENSOR_REPORTID_PERSONAL_ACTIVITY_CLASSIFIER 0x1E
#define IMU_SENSOR_REPORTID_AR_VR_STABILIZED_ROTATION_VECTOR 0x28
#define IMU_SENSOR_REPORTID_AR_VR_STABILIZED_GAME_ROTATION_VECTOR 0x29

// // Record IDs from figure 29, page 29 reference manual
// // These are used to read the metadata for each sensor type
// #define IMU_FRS_RECORDID_ACCELEROMETER 0xE302
// #define IMU_FRS_RECORDID_GYROSCOPE_CALIBRATED 0xE306
// #define IMU_FRS_RECORDID_MAGNETIC_FIELD_CALIBRATED 0xE309
// #define IMU_FRS_RECORDID_ROTATION_VECTOR 0xE30B

// // Reset complete packet (BNO08X Datasheet p.24 Figure 1-27)
// #define IMU_EXECUTABLE_RESET_COMPLETE 0x1

// // Command IDs from section 6.4, page 42
// // These are used to calibrate, initialize, set orientation, tare etc the
// /sensor
// #define IMU_COMMAND_ERRORS 1
// #define IMU_COMMAND_COUNTER 2
// #define IMU_COMMAND_TARE 3
// #define IMU_COMMAND_INITIALIZE 4
// #define IMU_COMMAND_DCD 6
// #define IMU_COMMAND_ME_CALIBRATE 7
// #define IMU_COMMAND_DCD_PERIOD_SAVE 9
// #define IMU_COMMAND_OSCILLATOR 10
// #define IMU_COMMAND_CLEAR_DCD 11

// #define IMU_CALIBRATE_ACCEL 0
// #define IMU_CALIBRATE_GYRO 1
// #define IMU_CALIBRATE_MAG 2
// #define IMU_CALIBRATE_PLANAR_ACCEL 3
// #define IMU_CALIBRATE_ACCEL_GYRO_MAG 4
// #define IMU_CALIBRATE_STOP 5

// imu_setup report enable bits
#define IMU_QUAT_ENABLED (1 << IMU_SENSOR_REPORTID_ROTATION_VECTOR)
#define IMU_ACCEL_ENABLED (1 << IMU_SENSOR_REPORTID_ACCELEROMETER)
#define IMU_GYRO_ENABLED (1 << IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED)
#define IMU_MAG_ENABLED (1 << IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED)
#define IMU_ALL_ENABLED (IMU_QUAT_ENABLED | IMU_ACCEL_ENABLED | IMU_GYRO_ENABLED | IMU_MAG_ENABLED)

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_imu_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_imu();
int imu_init_data_files(void);
int resetIMU();
int setupIMU(uint8_t enabled_features);
int imu_enable_feature_report(int report_id, uint32_t report_interval_us);
int imu_read_data();
void *imu_thread(void *paramPtr);
#endif // IMU_H
