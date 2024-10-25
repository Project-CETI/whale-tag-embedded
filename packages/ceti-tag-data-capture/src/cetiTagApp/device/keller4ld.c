//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: PDevice driver for Keller 4LD pressure transmitter
//-----------------------------------------------------------------------------

#include "keller4ld.h"

#include "i2c.h"

#include <pigpio.h>
#include <stdint.h>
#include <unistd.h>

#define KELLER_4LD_REQUEST_WAIT_TIME_US (8000)

// Keller 4LD Pressure Sensor 200 bar
// Reference pressure is a 1 bar abs
#define PRESSURE_MIN 0   // bar
#define PRESSURE_MAX 200 // bar

typedef enum {
    KELLER_4LD_CMD_REQUEST_MEASUREMENT = 0xAC,
} Keller4ldCommand;

WTResult pressure_get_measurement(double *pPressureBar, double *pTempC) {
    char raw[5] = {};
    int fd = PI_TRY(WT_DEV_PRESSURE, i2cOpen(PRESSURE_I2C_BUS, PRESSURE_I2C_DEV_ADDR, 0));
    PI_TRY(WT_DEV_PRESSURE, i2cWriteByte(fd, KELLER_4LD_CMD_REQUEST_MEASUREMENT), i2cClose(fd)); // measurement request from the device
    usleep(KELLER_4LD_REQUEST_WAIT_TIME_US);                                                     // wait for the measurement to finish
    PI_TRY(WT_DEV_PRESSURE, i2cReadDevice(fd, raw, sizeof(raw)), i2cClose(fd));                  // read the measurement
    i2cClose(fd);

    // parse status byte to verify validity
    uint8_t status = raw[0];
    if ((status & 0b11000100) != 0b01000000) { // invalid packet
        return WT_RESULT(WT_DEV_PRESSURE, WT_ERR_PRESSURE_INVALID_RESPONSE);
    }

    if (status & 0b00100000) { // device is busy
        return WT_RESULT(WT_DEV_PRESSURE, WT_ERR_PRESSURE_BUSY);
    }

    // packet is ok, parse data
    if (pPressureBar != NULL) {
        int16_t pressure_data = (((int16_t)raw[1]) << 8) | ((int16_t)raw[2]);
        // convert to bar
        *pPressureBar = ((PRESSURE_MAX - PRESSURE_MIN) / 32768.0) * (pressure_data - 16384);
    }

    if (pTempC != NULL) {
        int16_t temperature_data = (((int16_t)raw[3]) << 8) | ((int16_t)raw[4]);
        // convert to deg C
        *pTempC = (double)((temperature_data >> 4) - 24) * .05 - 50.0;
    }
    return WT_OK;
}
