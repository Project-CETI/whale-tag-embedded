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

WTResult pressure_get_measurement_raw(int16_t *pPressure, int16_t *pTemp) {
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
    if (pPressure != NULL) {
        *pPressure = (((int16_t)raw[1]) << 8) | ((int16_t)raw[2]);
    }

    if (pTemp != NULL) {
        *pPressure = (((int16_t)raw[3]) << 8) | ((int16_t)raw[4]);
    }
    return WT_OK;
}

WTResult pressure_get_measurement(double *pPressureBar, double *pTempC) {
    int16_t pressure_data = 0;
    int16_t temperature_data = 0;

    // get raw register values
    WT_TRY(WT_DEV_PRESSURE, pressure_get_measurement_raw(&pressure_data, &temperature_data));

    // convert to bar
    if (pPressureBar != NULL) {
        *pPressureBar = KELLER_4LD_RAW_TO_PRESSURE_BAR(pressure_data);
    }

    // convert to deg C
    if (pTempC != NULL) {
        *pTempC = KELLER_4LD_RAW_TO_TEMPERATURE_C(temperature_data);
    }

    return WT_OK;
}
