//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: Ambient Light Sensor device driver for LiteON LTR-329ALS-01
//-----------------------------------------------------------------------------
#include "ltr329als.h"

#include "i2c.h"

#include <pigpio.h>
#include <unistd.h>

#define ALS_WAKEUP_TIME_US (10000)

#define ALS_CONTRL_GAIN_1 (0b000 << 2)
#define ALS_CONTRL_GAIN_2 (0b001 << 2)
#define ALS_CONTRL_GAIN_4 (0b010 << 2)
#define ALS_CONTRL_GAIN_8 (0b011 << 2)
#define ALS_CONTRL_GAIN_48 (0b101 << 2)
#define ALS_CONTRL_GAIN_968 (0b111 << 2)

#define ALS_CONTRL_RESET (0b1 << 1)

#define ALS_CONTRL_MODE_STANDBY (0b0 << 0)
#define ALS_CONTRL_MODE_ACTIVE (0b1 << 0)

#define ALS_CONTRL_DEFAULT (ALS_CONTRL_GAIN_1 | ALS_CONTRL_MODE_STANDBY)

#define ALS_MEAS_RATE_REPEAT_50_MS (0b000 << 0)
#define ALS_MEAS_RATE_REPEAT_100_MS (0b001 << 0)
#define ALS_MEAS_RATE_REPEAT_200_MS (0b010 << 0)
#define ALS_MEAS_RATE_REPEAT_500_MS (0b011 << 0)
#define ALS_MEAS_RATE_REPEAT_1000_MS (0b010 << 0)
#define ALS_MEAS_RATE_REPEAT_2000_MS (0b111 << 0)

#define ALS_MEAS_RATE_INTEGRATION_100_MS (0b000 << 3)
#define ALS_MEAS_RATE_INTEGRATION_50_MS (0b001 << 3)
#define ALS_MEAS_RATE_INTEGRATION_200_MS (0b010 << 3)
#define ALS_MEAS_RATE_INTEGRATION_400_MS (0b011 << 3)
#define ALS_MEAS_RATE_INTEGRATION_150_MS (0b100 << 3)
#define ALS_MEAS_RATE_INTEGRATION_250_MS (0b101 << 3)
#define ALS_MEAS_RATE_INTEGRATION_300_MS (0b110 << 3)
#define ALS_MEAS_RATE_INTEGRATION_350_MS (0b111 << 3)

#define ALS_MEAS_RATE_DEFAULT (ALS_MEAS_RATE_INTEGRATION_100_MS | ALS_MEAS_RATE_REPEAT_500_MS)

typedef enum {
    ALS_REG_CONTRL = 0x80,
    ALS_REG_MEAS_RATE = 0x85,
    ALS_REG_PART_ID = 0x86,
    ALS_REG_MANUFAC_ID = 0x87,
    ALS_REG_DATA_CH1 = 0x88,
    ALS_REG_DATA_CH0 = 0x8A,
    ALS_REG_STATUS = 0x8C,
} AlsRegAddr;

WTResult als_wake(void) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    PI_TRY(WT_DEV_LIGHT, i2cWriteByteData(fd, ALS_REG_CONTRL, ALS_CONTRL_GAIN_1 | ALS_CONTRL_MODE_ACTIVE), i2cClose(fd)); // wake the light sensor up
    i2cClose(fd);
    usleep(ALS_WAKEUP_TIME_US); // wait for sensor to wake before using
    return WT_OK;
}

WTResult als_get_measurement(int *pVisible, int *pInfrared) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    int visible = PI_TRY(WT_DEV_LIGHT, i2cReadWordData(fd, ALS_REG_DATA_CH1), i2cClose(fd));
    int infrared = PI_TRY(WT_DEV_LIGHT, i2cReadWordData(fd, ALS_REG_DATA_CH0), i2cClose(fd));
    i2cClose(fd);
    if (pVisible != NULL) {
        *pVisible = visible;
    }
    if (pInfrared != NULL) {
        *pInfrared = infrared;
    }
    return WT_OK;
}

WTResult als_get_manufacturer_id(uint8_t *pManuId) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    int manu_id = PI_TRY(WT_DEV_LIGHT, i2cReadByteData(fd, ALS_REG_MANUFAC_ID), i2cClose(fd));
    i2cClose(fd);

    if (pManuId != NULL) {
        *pManuId = (uint8_t)manu_id;
    }
    return WT_OK;
}

WTResult als_get_part_id(uint8_t *pPartId, uint8_t *pRevisionId) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    int raw = PI_TRY(WT_DEV_LIGHT, i2cReadByteData(fd, ALS_REG_PART_ID), i2cClose(fd));
    i2cClose(fd);

    if (pPartId != NULL) {
        *pPartId = (((uint8_t)raw) >> 4) & 0x0F;
    }

    if (pRevisionId != NULL) {
        *pRevisionId = ((uint8_t)raw) & 0x0F;
    }

    return WT_OK;
}