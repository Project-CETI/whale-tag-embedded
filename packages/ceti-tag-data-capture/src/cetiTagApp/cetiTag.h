#ifndef CETI_TAG_H
#define CETI_TAG_H

#include <stdint.h>

#define BATTERY_SHM_NAME "/battery_shm"
#define BATTERY_SEM_NAME "/battery_sem"

#define PRESSURE_SHM_NAME "/pressure_shm"
#define PRESSURE_SEM_NAME "/pressure_sem"

#define BATTERY_SAMPLING_PERIOD_US 1000000
#define PRESSURE_SAMPLING_PERIOD_US 1000000

typedef struct {
    uint32_t error;
    int      rtc_time_s;
    int64_t  sys_time_us;
    double   cell_voltage_v[2];
    double   cell_temperature_c[2];
    double   current_mA;
} CetiBatterySample;

typedef struct {
    uint32_t error;
    int      rtc_time_s;
    int64_t  sys_time_us;
    double   pressure_bar;
    double   temperature_c;
} CetiPressureSample;

#endif //CETI_TAG_H