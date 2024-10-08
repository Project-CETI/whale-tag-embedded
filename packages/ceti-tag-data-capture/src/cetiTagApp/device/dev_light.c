#include "dev_light.h"

#include "i2c.h"

#include <pigpio.h>
#include <stdint.h>

WTResult light_wake(void) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    PI_TRY(WT_DEV_LIGHT, i2cWriteByteData(fd, 0x80, 0x1), i2cClose(fd));
    i2cClose(fd);
    return WT_OK;
}

WTResult light_get_value(int *pVisible, int *pInfrared) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    if (pVisible != NULL) {
        *pVisible = PI_TRY(WT_DEV_LIGHT, i2cReadWordData(fd, 0x88), i2cClose(fd));
    }
    if (pInfrared != NULL) {
        *pInfrared = PI_TRY(WT_DEV_LIGHT, i2cReadWordData(fd, 0x8A), i2cClose(fd));
    }
    i2cClose(fd);
    return WT_OK;
}

WTResult light_get_manufacturer(uint8_t *pManuId) {
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    if (pManuId != NULL) {
        *pManuId = PI_TRY(WT_DEV_LIGHT, i2cReadByteData(fd, 0x87), i2cClose(fd));
    }
    i2cClose(fd);
    return WT_OK;
}

WTResult light_get_part(uint8_t *pPartId, uint8_t *pRevision) {
    uint8_t result;
    int fd = PI_TRY(WT_DEV_LIGHT, i2cOpen(ALS_I2C_BUS, ALS_I2C_DEV_ADDR, 0));
    result = (uint8_t)PI_TRY(WT_DEV_LIGHT, i2cReadByteData(fd, 0x86), i2cClose(fd));
    if (pPartId != NULL) {
        *pPartId = result >> 4;
    }
    if (pRevision != NULL) {
        *pRevision = result & 0x0F;
    }
    i2cClose(fd);
    return WT_OK;
}
