//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//
// Notes: This is an I/O expander driver for a 
//-----------------------------------------------------------------------------
#include "iox.h"

//==== Private Libraries ======================================================
// local objects
#include "error.h"
#include "i2c.h"


// system libraries
#include <pigpio.h>
#include <stdlib.h>

//==== Private Typedefs =======================================================


//==== Private Variables ======================================================
static int s_iox_i2c_fd = PI_NO_HANDLE;

//==== Function Definitions ===================================================
/**
 * @brief initializes io expander
 * 
 * @return WTResult 
 */
WTResult wt_iox_init(void) {
    if(s_iox_i2c_fd < 0) {
        s_iox_i2c_fd = PI_TRY(WT_DEV_IOX, i2cOpen(WT_IOX_I2C_BUS, WT_IOX_I2C_DEV_ADDR, 0));
        atexit(wt_iox_terminate);
    }

    return WT_OK;
}

/**
 * @brief end io expander usage. Called automatically at exit.
 */
void wt_iox_terminate(void){
    i2cClose(s_iox_i2c_fd);
    s_iox_i2c_fd = PI_NO_HANDLE;
}

/**
 * @brief sets gpio expander pin to either input or output
 * 
 * @param pin - gpio expander pin number
 * @param mode 
 * @return WTResult 
 */
WTResult wt_iox_set_mode(int pin, WtIoxMode mode) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    switch (mode) {
    case WT_IOX_MODE_INPUT: {
        int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, WT_IOX_REG_CONFIGURATION));
        reg_value |= (1 << pin);
        PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, WT_IOX_REG_CONFIGURATION, reg_value));
        }
        return WT_OK;

    case WT_IOX_MODE_OUTPUT: {
        int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, WT_IOX_REG_CONFIGURATION));
        reg_value &= ~(1 << pin);
        PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, WT_IOX_REG_CONFIGURATION, reg_value));
        }
        return WT_OK;
    
    default:
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_MODE);
    }
}

/**
 * @brief returns gpio expander pin mode
 * 
 * @param pin 
 * @param mode - output pointer
 * @return WTResult 
 */
WTResult wt_iox_get_mode(int pin, WtIoxMode *mode) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, WT_IOX_REG_CONFIGURATION));
    if (mode != NULL) {
        *mode = ((reg_value >> pin) & 1) ? WT_IOX_MODE_INPUT : WT_IOX_MODE_OUTPUT;
    }
    return WT_OK;
}

/**
 * @brief returns gpio expander pin input value
 * 
 * @param pin 
 * @param value - output pointer
 * @return WTResult 
 */
WTResult wt_iox_read(int pin, int *value) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, WT_IOX_REG_INPUT));
    if (value != NULL) {
        *value = ((reg_value >> pin) & 1);
    }
    return WT_OK;
}

WTResult wt_iox_read_register(PCAL6408ARegister reg, uint8_t *value){
    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, reg));
    if (value != NULL) {
        *value = (uint8_t)reg_value;
    }
    return WT_OK;
}

/**
 * @brief sets gpio expander pin output value
 * 
 * @param pin 
 * @param value 
 * @return WTResult 
 */
WTResult wt_iox_write(int pin, int value) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, WT_IOX_REG_OUTPUT));
    if (value) {
        reg_value |= (1 << pin);
    } else {
        reg_value &= ~(1 << pin);
    }
    PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, WT_IOX_REG_OUTPUT, reg_value));

    return WT_OK;
}

WTResult wt_iox_write_register(PCAL6408ARegister reg, uint8_t value){
    PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, reg, value));
    return WT_OK;
}
