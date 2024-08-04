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
#define AUDIO_PAGE_SEM_NAME "/audio_page_sem"

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

//-----------------------------------------------------------------------------
// Definitions/Configurations
//-----------------------------------------------------------------------------
//  - SPI block HWM * 32 bytes = 8192 bytes (25% of the hardware FIFO in this example)
//  - Sampling rate 96000 Hz
//  - 6 bytes per sample set (3 channels, 16 bits per channel)
// N.B. Make NUM_SPI_BLOCKS an integer multiple of 3 for alignment
// reasons
// SPI Block Size
#define HWM (512)                 // High Water Mark from Verilog code - these are 32 byte chunks (2 sample sets)
#define SPI_BLOCK_SIZE (HWM * 32) // make SPI block size <= HWM * 32 otherwise may underflow

// divisable by SPI_BLOCK_SIZE (512*32) = 8192,
// and divisble by 16-bit sample size (3*2) = 6 bytes per 3 channel sample;
// and divisible by 24-bit sample size (3*3) = 9 byte per 3 channel sample;
// and divisible by 16-bit 4 channel = 8
// and divisible by 24-bit 4 channel = 12
#define AUDIO_LCM_BYTES (147456)

#define AUDIO_CHANNELS (3)
//multiple of AUDIO_LCM_BYTES closest to 75 seconds @ 16-bit, 96kSPS, (14401536)
#define AUDIO_BUFFER_SIZE_BYTES_PER_CHANNEL (14401536)
#define AUDIO_BUFFER_SIZE_BYTES (AUDIO_CHANNELS*AUDIO_BUFFER_SIZE_BYTES_PER_CHANNEL)
#define AUDIO_BUFFER_SIZE_BLOCKS (AUDIO_BUFFER_SIZE_BYTES/SPI_BLOCK_SIZE)
#define AUDIO_BUFFER_SIZE_SAMPLE16 (AUDIO_BUFFER_SIZE_BYTES/ (sizeof(int16_t)*AUDIO_CHANNELS))
#define AUDIO_BUFFER_SIZE_SAMPLE24 (AUDIO_BUFFER_SIZE_BYTES/ (3*AUDIO_CHANNELS))
#define AUDIO_BLOCK_FILL_SPEED_US(sample_rate, bit_depth) (SPI_BLOCK_SIZE * 1000000.0/(AUDIO_CHANNELS * (sample_rate) * ((bit_depth)/ 8)))
#define AUDIO_PAGE_FILL_SPEED_US(sample_rate, bit_depth)  (AUDIO_BUFFER_SIZE_BYTES * 1000000.0/(AUDIO_CHANNELS * (sample_rate) * ((bit_depth)/ 8)))

#define BATTERY_SAMPLING_PERIOD_US    1000000
#define LIGHT_SAMPLING_PERIOD_US      1000000
#define PRESSURE_SAMPLING_PERIOD_US   1000000
#define IMU_QUATERNION_SAMPLE_PERIOD_US 50000 // rate for the computed orientation
#define IMU_9DOF_SAMPLE_PERIOD_US 20000 // rate for the accelerometer/gyroscope/magnetometer

//buffered data (high speed)
// ToDo: ECG

//-----------------------------------------------------------------------------
// Type Definitions
//-----------------------------------------------------------------------------
typedef struct {
    int page; // which buffer will be populated with new incoming data
    int block; // which block will be populated with new incoming data
    union {
        uint8_t raw[AUDIO_BUFFER_SIZE_BYTES];
        char    blocks[AUDIO_BUFFER_SIZE_BLOCKS][SPI_BLOCK_SIZE];
        uint8_t sample16[AUDIO_BUFFER_SIZE_SAMPLE16][AUDIO_CHANNELS][2];
        uint8_t sample24[AUDIO_BUFFER_SIZE_SAMPLE24][AUDIO_CHANNELS][3];
    }data[2];
} CetiAudioBuffer;

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