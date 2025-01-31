//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: Device driver for CEVA BNO086 IMU
//-----------------------------------------------------------------------------

#include "bno08x.h"
#include "i2c.h"
#include "gpio.h"

#include <pigpio.h>

shtp_channel_seq_num[SHTP_CH_COUNT] = {};

static WTResult __wt_bno08x_write(void *buffer, size_t buffer_len) {
    //starts at index 3 so memcpy can be more efficient
    uint8_t header[5] = {
        [0] = 0x04, //set addr
        [1] = IMU_I2C_DEV_ADDR, //imu addr
        [2] = 0x02, //start
        [3] = 0x07, //write
        [4] = buffer_len, //length
    };

    const uint8_t tail[2] = {
        [0] = 0x03 //stop
        [1] = 0x00 //end
    };

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, header, sizeof(header), NULL, 0));
    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, buffer, buffer_len, NULL, 0));
    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, tail, sizeof(tail), NULL, 0));
    return WT_OK;
}


static WTResult wt_bno08x_read(void *buffer, size_t buffer_len) {
    //starts at index 3 so memcpy can be more efficient
    uint8_t header[6] = {
        [0] = 0x04, //set addr
        [1] = IMU_I2C_DEV_ADDR, //imu addr
        [2] = 0x02, //start
        [3] = 0x06, //read
        [4] = (((uint16_t) buffer_len) & 0xFF), //length lsb
        [5] = (((uint16_t) buffer_len >> 8) & 0xFF), //length msb
        [6] = 0x03 //stop
        [7] = 0x00 //end
    };

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, header, sizeof(header), buffer, buffer_len));
    return WT_OK;
}

WTResult wt_bno08x_read_header(ShtpHeader *pHeader) {
    __wt_bno08x_read(pHeader, sizeof(ShtpHeader));
}