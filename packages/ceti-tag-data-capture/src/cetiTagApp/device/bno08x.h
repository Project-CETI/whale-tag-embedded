//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: Device driver for CEVA BNO086 IMU
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_CEVA_BNO08x__
#define __CETI_WHALE_TAG_HAL_CEVA_BNO08x__

#include "../utils/error.h" //for WTResult

#include <stdint.h>

typedef enum {
    SHTP_CH_CMD = 0,
    SHTP_CH_EXE = 1,
    SHTP_CH_CONTROL = 2,
    SHTP_CH_REPORTS = 3,
    SHTP_CH_WAKE_REPORTS = 4,
    SHTP_CH_GYRO = 5,
} ShtpChannel;
#define SHTP_CH_COUNT (6)

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint16_t length;
    uint8_t channel;
    uint8_t seq_num;
} ShtpHeader;

WTResult wt_bno08x_read_header(ShtpHeader *pHeader);

#endif // __CETI_WHALE_TAG_HAL_CEVA_BNO08x__
