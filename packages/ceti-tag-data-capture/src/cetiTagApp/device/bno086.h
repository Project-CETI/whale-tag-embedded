//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: IMU Sensor device driver for CEVA BNO086
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_BNO086__
#define __CETI_WHALE_TAG_HAL_BNO086__

#include "../utils/error.h"

#include <stdint.h>

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint16_t length;
    uint8_t channel;
    uint8_t seq_num;
} ShtpHeader;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint8_t report_id;
    uint8_t sequence_number;
    uint8_t status;
    uint8_t delay;
    union {
        struct {
            uint16_t x;
            uint16_t y;
            uint16_t z;
        } accelerometer;
        struct {
            uint16_t x;
            uint16_t y;
            uint16_t z;
        } gyroscope_calibrated;
        struct {
            uint16_t x;
            uint16_t y;
            uint16_t z;
        } magnetic_field_calibrated;
        struct {
            uint16_t i;
            uint16_t j;
            uint16_t k;
            uint16_t real;
            uint16_t accuracy;
        } rotation;
    } data;
} ShtpSensorReport;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint8_t report_id;
    uint32_t delay;
} ShtpTimebaseReport;

WTResult bno086_open(void);
WTResult bno086_close(void);
WTResult bno086_read_header(ShtpHeader *header);
WTResult bno086_read_reports(uint8_t *pBuffer, size_t len);
WTResult bno086_write(const uint8_t *pBuffer, size_t len);

#endif // __CETI_WHALE_TAG_HAL_BNO086__