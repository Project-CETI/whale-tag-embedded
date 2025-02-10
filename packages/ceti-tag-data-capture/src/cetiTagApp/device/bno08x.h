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

#define BNO08X_FRS_ID_SYSTEM_ORIENTATION (0x2D41)
#define BNO08X_FRS_ID_MAGNETOMETER_ORIENTATION (0x2D4C)
#define BNO08X_FRS_ID_SERIAL_NUMBER (0x4B4B)

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint16_t length;
    uint8_t channel;
    uint8_t seq_num;
} ShtpHeader;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    ShtpHeader header;
    union {
        uint8_t report_id;
        struct {
            uint8_t report_id;
            uint8_t status;
            uint16_t offset;
        } frs_write_response;
        struct {
            uint8_t report_id;
            uint8_t length_status;
            uint16_t offset;
            uint8_t data[];
            uint16_t frs_type;
        } frs_read_response;
    } report;
    
} ShtpReport;


WTResult wt_bno08x_close(void);
WTResult wt_bno08x_enable_report(uint16_t report_id, uint32_t report_interval_us);
WTResult wt_bno08x_open(void);
void wt_bno08x_reset_hard(void);
WTResult wt_bno08x_read(void *buffer, size_t buffer_len);
WTResult wt_bno08x_read_header(ShtpHeader *pHeader);
WTResult wt_bno08x_set_system_orientation(double w, double x, double y, double z);
WTResult wt_bno08x_write(void *buffer, size_t buffer_len);
WTResult wt_bno08x_write_shtp_packet(ShtpChannel channel, void *pPacket, size_t packet_len);

#endif // __CETI_WHALE_TAG_HAL_CEVA_BNO08x__
