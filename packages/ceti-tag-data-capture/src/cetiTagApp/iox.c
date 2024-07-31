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
#include "i2c.h"


// system libraries
#include <pigpio.h>
#include <pthread.h>
#include <stdlib.h>

//==== Private Typedefs =======================================================


//==== Private Variables ======================================================
static int s_iox_i2c_fd = PI_NO_HANDLE;
static pthread_mutex_t s_write_lock = PTHREAD_MUTEX_INITIALIZER;

//==== Function Definitions ===================================================
/**
 * @brief initializes io expander.
 * 
 * @return WTResult 
 */
WTResult iox_init(void) {
    if(s_iox_i2c_fd < 0) {
        s_iox_i2c_fd = PI_TRY(WT_DEV_IOX, i2cOpen(IOX_I2C_BUS, IOX_I2C_DEV_ADDR, 0));
        atexit(iox_terminate);
    }

    return WT_OK;
}

/**
 * @brief end io expander usage.
 */
void iox_terminate(void){
    if(!(s_iox_i2c_fd < 0)){
        i2cClose(s_iox_i2c_fd);
        s_iox_i2c_fd = PI_NO_HANDLE;
    }
}

/**
 * @brief sets gpio expander pin to either input or output
 * 
 * @param pin - gpio expander pin number
 * @param mode 
 * @return WTResult 
 */
WTResult iox_set_mode(int pin, WtIoxMode mode) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    switch (mode) {
    case IOX_MODE_INPUT: {
        pthread_mutex_lock(&s_write_lock); //prevent writes during
        int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, IOX_REG_CONFIGURATION), pthread_mutex_unlock(&s_write_lock));
        reg_value |= (1 << pin);
        PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, IOX_REG_CONFIGURATION, reg_value), pthread_mutex_unlock(&s_write_lock));
        pthread_mutex_unlock(&s_write_lock);
        }
        return WT_OK;

    case IOX_MODE_OUTPUT: {
        pthread_mutex_lock(&s_write_lock);
        int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, IOX_REG_CONFIGURATION), pthread_mutex_unlock(&s_write_lock));
        reg_value &= ~(1 << pin);
        PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, IOX_REG_CONFIGURATION, reg_value), pthread_mutex_unlock(&s_write_lock));
        pthread_mutex_unlock(&s_write_lock);
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
 * @param pMode - output pointer
 * @return WTResult 
 */
WTResult iox_get_mode(int pin, WtIoxMode *pMode) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, IOX_REG_CONFIGURATION));
    if (pMode != NULL) {
        *pMode = ((reg_value >> pin) & 1) ? IOX_MODE_INPUT : IOX_MODE_OUTPUT;
    }
    return WT_OK;
}

/**
 * @brief returns gpio expander pin input value
 * 
 * @param pin 
 * @param pValue - output pointer
 * @return WTResult 
 */
WTResult iox_read(int pin, int *pValue) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, IOX_REG_INPUT));
    if (pValue != NULL) {
        *pValue = ((reg_value >> pin) & 1);
    }
    return WT_OK;
}

/**
 * @brief Reads a value from a specified I/O Expander register
 * 
 * @param reg - register address 
 * @param pValue  - output pointer
 * @return WTResult
 */
WTResult iox_read_register(IoxRegister reg, uint8_t *pValue){
    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, reg));
    if (pValue != NULL) {
        *pValue = (uint8_t)reg_value;
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
WTResult iox_write(int pin, int value) {
    if ((pin < 0) || (pin > 7)){
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }

    pthread_mutex_lock(&s_write_lock);
    int reg_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, IOX_REG_OUTPUT), pthread_mutex_unlock(&s_write_lock));
    if (value) {
        reg_value |= (1 << pin);
    } else {
        reg_value &= ~(1 << pin);
    }
    PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, IOX_REG_OUTPUT, reg_value), pthread_mutex_unlock(&s_write_lock));
    pthread_mutex_unlock(&s_write_lock);

    return WT_OK;
}

/**
 * @brief Writes a value to a specified I/O Expander register
 * 
 * @param reg - register address 
 * @param value
 * @return WTResult
 * 
 * @note Additional thread syncronization mechanisms (mutex), may be required 
 * if modifying a register. see `iox_write_lock()` and `iox_write_unlock()`
 */
WTResult iox_write_register(IoxRegister reg, uint8_t value){
    PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, reg, value));
    return WT_OK;
}

/**
 * @brief Locks I/O expander write mutex
 * 
 * @return int returns 0 on success, and an error number otherwise
 */
int iox_write_lock(void) {
    return pthread_mutex_lock(&s_write_lock);
}

/**
 * @brief unlocks I/O expander write mutex
 * 
 * @return int returns 0 on success, and an error number otherwise
 */
int iox_write_unlock(void){
    return pthread_mutex_unlock(&s_write_lock);
}
