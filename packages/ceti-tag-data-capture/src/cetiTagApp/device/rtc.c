//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "rtc.h"

#include "i2c.h"

#include <pigpio.h>

WTResult rtc_get_count(uint32_t *pCount) {
    int fd = PI_TRY(WT_DEV_RTC, i2cOpen(RTC_I2C_BUS, RTC_I2C_DEV_ADDR, 0));

    uint32_t rtcCount = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t byte = PI_TRY(WT_DEV_RTC, i2cReadByteData(fd, i), i2cClose(fd));
        rtcCount |= byte << (8 * i);
    }

    i2cClose(fd);

    if (pCount != NULL) {
        *pCount = rtcCount;
    }

    return WT_OK;
}

WTResult rtc_set_count(uint32_t count) {

    int fd = PI_TRY(WT_DEV_RTC, i2cOpen(RTC_I2C_BUS, RTC_I2C_DEV_ADDR, 0));

    for (int i = 0; i < 4; i++) {
        uint8_t byte = (uint8_t)((count >> (i * 8)) & 0xFF);
        PI_TRY(WT_DEV_RTC, i2cWriteByteData(fd, i, byte), i2cClose(fd));
    }

    i2cClose(fd);

    return WT_OK;
}
