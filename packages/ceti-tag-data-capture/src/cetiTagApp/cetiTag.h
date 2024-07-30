#ifndef CETI_TAG_H
#define CETI_TAG_H

#include <stdint.h>

#define AUDIO_SHM_NAME "/audio_shm"
#define AUDIO_BLOCK_SEM_NAME "/audio_block_sem"
#define AUDIO_HALF_FULL_SEM_NAME "/audio_half_full_sem"
#define AUDIO_FULL_SEM_NAME "/audio_full_sem"


#define BATTERY_SHM_NAME "/battery_shm"
#define BATTERY_SEM_NAME "/battery_sem"

#define PRESSURE_SHM_NAME "/pressure_shm"
#define PRESSURE_SEM_NAME "/pressure_sem"

#define LIGHT_SHM_NAME "/light_shm"
#define LIGHT_SEM_NAME "/light_sem"


#define BATTERY_SAMPLING_PERIOD_US 1000000
#define LIGHT_SAMPLING_PERIOD_US 1000000
#define PRESSURE_SAMPLING_PERIOD_US 1000000

typedef struct {
    int32_t error;
    int     rtc_time_s;
    int64_t sys_time_us;
    double  cell_voltage_v[2];
    double  cell_temperature_c[2];
    double  current_mA;
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

// typedef struct {
//     int64_t sys_time_us;
//     uint8_t data[2][512];
// } CetiAudioBuffer;

#endif //CETI_TAG_H