#include "bno086.h"
#include "gpio.h"
#include "i2c.h"

#include "../utils/timing.h"

#include <math.h>
#include <pigpio.h>
#include <string.h>
#include <unistd.h>

// This function initializes communications with the device.  It
// can initialize any GPIO pins and peripheral devices used to
// interface with the sensor hub.
// It should also perform a reset cycle on the sensor hub to
// ensure communications start from a known state.
WTResult bno086_open(void) {
    // gpioInitialise();

    // reset sensor
    gpioSetMode(IMU_N_RESET, PI_OUTPUT);
    usleep(10000);
    gpioWrite(IMU_N_RESET, PI_HIGH);
    usleep(100000);
    gpioWrite(IMU_N_RESET, PI_LOW);
    usleep(100000);
    gpioWrite(IMU_N_RESET, PI_HIGH);
    usleep(500000); // if this is about 150000 or less, the first feature report fails to start

    PI_TRY(WT_DEV_IMU, bbI2COpen(IMU_BB_I2C_SDA, IMU_BB_I2C_SCL, 200000));

    return WT_OK;
}

WTResult bno086_close(void) {
    bbI2CClose(IMU_BB_I2C_SDA);
    return WT_OK;
}

WTResult bno086_read_header(ShtpHeader *header) {
    const uint8_t hdr_request[9] = {
        0x04, // set address
        IMU_I2C_DEV_ADDR,
        0x02, // start
        0x01, // escape
        0x06, // read
        0x04, // #bytes lsb (read a 4-byte header)
        0x00, // #bytes msb
        0x03, // stop
        0x00, // end
    };

    if ((header == NULL)) {
        return WT_RESULT(WT_DEV_IMU, WT_ERR_IMU_INVALID_BUFFER);
    }

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, (char *)&hdr_request, sizeof(hdr_request), (char *)header, 4));

    return WT_OK;
}

WTResult bno086_read_reports(uint8_t *pBuffer, size_t len) {
    uint8_t packet_request[9] = {
        0x04, // set address
        IMU_I2C_DEV_ADDR,
        0x02, // start
        0x01, // escape
        0x06, // read
        0x00, // #bytes lsb (read a 4-byte header)
        0x00, // #bytes msb
        0x03, // stop
        0x00, // end
    };

    if ((pBuffer == NULL)) {
        return WT_RESULT(WT_DEV_IMU, WT_ERR_IMU_INVALID_BUFFER);
    }

    packet_request[5] = (len & 0xFF);
    packet_request[6] = ((len >> 8) & 0xFF);

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, (char *)&packet_request, sizeof(packet_request), (char *)pBuffer, len));
    return WT_OK;
}

WTResult bno086_write(const uint8_t *pBuffer, size_t len) {
    uint8_t writeCmdBuf[256 + 7];

    if ((pBuffer == NULL)) {
        return WT_RESULT(WT_DEV_IMU, WT_ERR_IMU_INVALID_BUFFER);
    }

    // Validate parameters
    if ((len == 0) || (len > sizeof(writeCmdBuf))) {
        return WT_RESULT(WT_DEV_IMU, WT_ERR_IMU_BAD_PKT_SIZE);
    }

    writeCmdBuf[0] = 0x04; // set address
    writeCmdBuf[1] = IMU_I2C_DEV_ADDR;
    writeCmdBuf[2] = 0x02; // start
    writeCmdBuf[3] = 0x07; // write
    writeCmdBuf[4] = len;  // length
    memcpy(&writeCmdBuf[5], pBuffer, len);
    writeCmdBuf[5 + len] = 0x03;     // stop
    writeCmdBuf[5 + len + 1] = 0x00; // end

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, (char *)&writeCmdBuf, (5 + len + 2), NULL, 0));

    return WT_OK;
}
