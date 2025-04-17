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

// === AUDIO ===
#define AUDIO_SHM_NAME "/audio_shm"
#define AUDIO_BLOCK_SEM_NAME "/audio_block_sem"
#define AUDIO_PAGE_SEM_NAME "/audio_page_sem"

// === BMS ===
#define BATTERY_SHM_NAME "/battery_shm"
#define BATTERY_SEM_NAME "/battery_sem"

// === ECG ===
#define ECG_SHM_NAME "/ecg_shm"
#define ECG_SAMPLE_SEM_NAME "/ecg_sample_sem"
#define ECG_PAGE_SEM_NAME "/ecg_page_sem"

// === IMU ===
#define IMU_REPORT_BUFFER_SHM_NAME "/imu_report_buffer_shm"
#define IMU_REPORT_SEM_NAME "/imu_report_sem"
#define IMU_PAGE_SEM_NAME "/imu_page_sem"

// === LIGHT ===
#define LIGHT_SHM_NAME "/light_shm"
#define LIGHT_SEM_NAME "/light_sem"

// === PRESSURE ===
#define PRESSURE_SHM_NAME "/pressure_shm"
#define PRESSURE_SEM_NAME "/pressure_sem"

// === RECOVERY ===
#define RECOVERY_SHM_NAME "/recovery_shm"
#define RECOVERY_SEM_NAME "/recovery_sem"

//-----------------------------------------------------------------------------
// Definitions/Configurations
//-----------------------------------------------------------------------------
// === AUDIO ===
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
// multiple of AUDIO_LCM_BYTES closest to 75 seconds @ 16-bit, 96kSPS, (14401536)
#define AUDIO_BUFFER_SIZE_BYTES_PER_CHANNEL (14401536)
#define AUDIO_BUFFER_SIZE_BYTES (AUDIO_CHANNELS * AUDIO_BUFFER_SIZE_BYTES_PER_CHANNEL)
#define AUDIO_BUFFER_SIZE_BLOCKS (AUDIO_BUFFER_SIZE_BYTES / SPI_BLOCK_SIZE)
#define AUDIO_BUFFER_SIZE_SAMPLE16 (AUDIO_BUFFER_SIZE_BYTES / (sizeof(int16_t) * AUDIO_CHANNELS))
#define AUDIO_BUFFER_SIZE_SAMPLE24 (AUDIO_BUFFER_SIZE_BYTES / (3 * AUDIO_CHANNELS))
#define AUDIO_BLOCK_FILL_SPEED_US(sample_rate, bit_depth) (SPI_BLOCK_SIZE * 1000000.0 / (AUDIO_CHANNELS * (sample_rate) * ((bit_depth) / 8)))
#define AUDIO_PAGE_FILL_SPEED_US(sample_rate, bit_depth) (AUDIO_BUFFER_SIZE_BYTES * 1000000.0 / (AUDIO_CHANNELS * (sample_rate) * ((bit_depth) / 8)))

// === BMS ===
#define BATTERY_SAMPLING_PERIOD_US 1000000

// === ECG ===
#define ECG_NUM_BUFFERS 2       // One for logging, one for writing.
#define ECG_BUFFER_LENGTH 10000 // Once a buffer fills, it will be flushed to a file
#define ECG_SAMPLING_PERIOD_US 1000
#define ECG_PAGE_FILL_PERIOD_US (ECG_SAMPLING_PERIOD_US * ECG_BUFFER_LENGTH)

// === IMU ===
#define IMU_BUFFER_FLUSH_INTERVAL_US (1000000)

#define IMU_QUATERNION_SAMPLE_PERIOD_US 50000 // rate for the computed orientation
#define IMU_9DOF_SAMPLE_PERIOD_US 20000       // rate for the accelerometer/gyroscope/magnetometer

#define IMU_REPORT_BUFFER_SIZE ( \
    (IMU_BUFFER_FLUSH_INTERVAL_US / IMU_QUATERNION_SAMPLE_PERIOD_US) + (IMU_BUFFER_FLUSH_INTERVAL_US / IMU_9DOF_SAMPLE_PERIOD_US) + (IMU_BUFFER_FLUSH_INTERVAL_US / IMU_9DOF_SAMPLE_PERIOD_US) + (IMU_BUFFER_FLUSH_INTERVAL_US / IMU_9DOF_SAMPLE_PERIOD_US))

// === LIGHT ===
#define LIGHT_SAMPLING_PERIOD_US 1000000

// === PRESSURE ===
#define PRESSURE_SAMPLING_PERIOD_US 1000000

//-----------------------------------------------------------------------------
// Type Definitions
//-----------------------------------------------------------------------------
// === AUDIO ===
typedef struct {
    int page;  // which buffer will be populated with new incoming data
    int block; // which block will be populated with new incoming data
    union {
        uint8_t raw[AUDIO_BUFFER_SIZE_BYTES];
        char blocks[AUDIO_BUFFER_SIZE_BLOCKS][SPI_BLOCK_SIZE];
        uint8_t sample16[AUDIO_BUFFER_SIZE_SAMPLE16][AUDIO_CHANNELS][2];
        uint8_t sample24[AUDIO_BUFFER_SIZE_SAMPLE24][AUDIO_CHANNELS][3];
    } data[2];
} CetiAudioBuffer;

// === BMS ===
typedef struct {
    int64_t sys_time_us;
    int32_t error;
    int rtc_time_s;
    double cell_voltage_v[2];
    double cell_temperature_c[2];
    double current_mA;
    double state_of_charge;
    uint16_t status;
    uint16_t protection_alert;
} CetiBatterySample;

// === ECG ===
typedef struct {
    int page;   // which buffer will be populated with new incoming data
    int sample; // which sample will be populated with new incoming data
    int lod_enabled;
    /* ToDo: Leaving this as seperate buffers to maintain existing code
     * structure, but the way memory is accessed (all field from a single
     * sample accessed at the same time and not all values of a single field
     * accessed at once), it may make more sense to encapsulate in a
     * CetiEcgSample struct so the 4kB TLB page for this mapped memory doesn't
     * result in a cache miss on every field access. Will need to be
     * benchmarked.
     * - MSH
     */
    long long sys_time_us[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH];
    int rtc_time_s[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH];
    long ecg_readings[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH];
    int leadsOff_readings_p[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH];
    int leadsOff_readings_n[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH];
    long long sample_indexes[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH];
} CetiEcgBuffer;

// === IMU ===
typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint8_t report_id;
    uint8_t sequence_number;
    uint8_t status;
    uint8_t delay;
    uint16_t x;
    uint16_t y;
    uint16_t z;
} CetiImuXYZReport;

typedef CetiImuXYZReport CetiImuAccelReport;
typedef CetiImuXYZReport CetiImuGyroReport;
typedef CetiImuXYZReport CetiImuMagReport;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint8_t report_id;
    uint8_t sequence_number;
    uint8_t status;
    uint8_t delay;
    uint16_t i;
    uint16_t j;
    uint16_t k;
    uint16_t real;
    uint16_t accuracy;
} CetiImuQuatReport;

typedef union {
    struct {
        uint8_t report_id;
        uint8_t sequence_number;
        uint8_t status;
        uint8_t delay;
    };
    CetiImuQuatReport quat;
    CetiImuAccelReport accel;
    CetiImuGyroReport gyro;
    CetiImuMagReport mag;
} CetiImuSensorReport;

// ToDo: make this storage struct publically accessible to other processes via
// shared memory objects. Currently this is handled by having the latest instance
// of data copied to `imu_quaternion`, `imu_accel_m_ss`, `imu_gyro_rad_s`, and
// `imu_mag_ut` but those additional copies and posts are kind of wasteful
typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    int64_t sys_time_us;
    uint32_t rtc_time_s;
    uint32_t reading_delay; // units 100 uS
    int32_t error;
    CetiImuSensorReport report;
} CetiImuReport;

typedef struct {
    uint32_t page;
    uint32_t sample;
    CetiImuReport reports[2][IMU_REPORT_BUFFER_SIZE];
} CetiImuReportBuffer;

// === LIGHT ===
typedef struct {
    int64_t sys_time_us;
    int rtc_time_s;
    int32_t error;
    int visible;
    int infrared;
} CetiLightSample;

// === PRESSURE ===
typedef struct {
    int64_t sys_time_us;
    int rtc_time_s;
    int32_t error;
    double pressure_bar;
    double temperature_c;
} CetiPressureSample;

typedef struct {
    int64_t sys_time_us;
    int rtc_time_s;
    char nmea_sentence[96];
} CetiRecoverySample;

#endif // CETI_TAG_H
