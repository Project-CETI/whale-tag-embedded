//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// File Description: This header is the minimum set of definitions required
//     for shared memory to be used for sampled tag data
//-----------------------------------------------------------------------------
#ifndef CETI_TAG_H
#define CETI_TAG_H

#include <stdint.h>

#define AUDIO_SHM_NAME "/audio_shm"
#define AUDIO_BLOCK_SEM_NAME "/audio_block_sem"
#define AUDIO_HALF_FULL_SEM_NAME "/audio_half_full_sem"

#define BATTERY_SHM_NAME "/battery_shm"
#define BATTERY_SEM_NAME "/battery_sem"

#define IMU_QUAT_SHM_NAME "/imu_quat_shm"
#define IMU_QUAT_SEM_NAME "/imu_quat_sample_sem"
#define IMU_ACCEL_SHM_NAME "/imu_accel_shm"
#define IMU_ACCEL_SEM_NAME "/imu_accel_sample_sem"
#define IMU_GYRO_SHM_NAME "/imu_gyro_shm"
#define IMU_GYRO_SEM_NAME "/imu_gyro_sample_sem"
#define IMU_MAG_SHM_NAME "/imu_mag_shm"
#define IMU_MAG_SEM_NAME "/imu_mag_sample_sem"

#define LIGHT_SHM_NAME "/light_shm"
#define LIGHT_SEM_NAME "/light_sem"

#define PRESSURE_SHM_NAME "/pressure_shm"
#define PRESSURE_SEM_NAME "/pressure_sem"

#define BATTERY_SAMPLING_PERIOD_US    1000000
#define LIGHT_SAMPLING_PERIOD_US      1000000
#define PRESSURE_SAMPLING_PERIOD_US   1000000
#define IMU_QUATERNION_SAMPLE_PERIOD_US 50000 // rate for the computed orientation
#define IMU_9DOF_SAMPLE_PERIOD_US 20000 // rate for the accelerometer/gyroscope/magnetometer

//buffered data (high speed)
// ToDo: ECG
// ToDo: AUIDO


typedef struct {
    int32_t error;
    int     rtc_time_s;
    int64_t sys_time_us;
    double  cell_voltage_v[2];
    double  cell_temperature_c[2];
    double  current_mA;
    uint16_t status;
    uint16_t protection_alert;
} CetiBatterySample;

typedef struct {
    int32_t error;
    int     rtc_time_s;
    int64_t sys_time_us;
    int     visible;
    int     infrared;
} CetiLightSample;

typedef struct {
    int32_t error;
    int     rtc_time_s;
    int64_t sys_time_us;
    double  pressure_bar;
    double  temperature_c;
} CetiPressureSample;

typedef struct {
    int64_t sys_time_us;
    int64_t reading_delay_us;
    int     rtc_time_s;
    int16_t i;
    int16_t j;
    int16_t k;
    int16_t real;
    int16_t accuracy;
} CetiImuQuatSample;

typedef struct {
    int64_t sys_time_us;
    int64_t reading_delay_us;
    int     rtc_time_s;
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t accuracy;
} CetiImuAccelSample;


typedef struct {
    int64_t sys_time_us;
    int64_t reading_delay_us;
    int     rtc_time_s;
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t accuracy;
} CetiImuGyroSample;

typedef struct {
    int64_t sys_time_us;
    int64_t reading_delay_us;
    int     rtc_time_s;
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t accuracy;
} CetiImuMagSample;

#endif //CETI_TAG_H