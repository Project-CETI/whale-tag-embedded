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

//==== Private Typedefs =======================================================


//==== Private Variables ======================================================
static int s_iox_i2c_fd = PI_NO_HANDLE;
static pthread_mutex_t s_write_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_read_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t latest_register_value = 0;
static int new_read_requested = 0;
int g_iox_thread_is_running = 0;

//==== Function Definitions ===================================================

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

/**
 * @brief initializes io expander.
 * 
 * @return WTResult 
 */
WTResult init_iox(void) {
    if(s_iox_i2c_fd < 0) {
        s_iox_i2c_fd = PI_TRY(WT_DEV_IOX, i2cOpen(IOX_I2C_BUS, IOX_I2C_DEV_ADDR, 0));
        atexit(iox_terminate);
    }
    CETI_LOG("Successfully initialized the IO expander");

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
WTResult iox_read_pin(int pin, int *pValue, int wait_for_new_value) {
    // Validate the requested pin.
    if ((pin < 0) || (pin > 7)) {
        return WT_RESULT(WT_DEV_IOX, WT_ERR_BAD_IOX_GPIO);
    }
    
    // Acquire a new reading, or request an asynchronous reading.
    iox_read_register(NULL, wait_for_new_value);
    
    // Extract the desired pin value.
    if (pValue != NULL) {
        pthread_mutex_lock(&s_read_lock);
        *pValue = ((latest_register_value >> pin) & 1);
        pthread_mutex_unlock(&s_read_lock);
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
WTResult iox_read_register(uint8_t *pValue, int wait_for_new_value) {
    // If blocking behavior is desired, wait for a new reading.
    if(wait_for_new_value)
    {
      pthread_mutex_lock(&s_read_lock);
      latest_register_value = PI_TRY(WT_DEV_IOX, i2cReadByteData(s_iox_i2c_fd, IOX_REG_INPUT));
      pthread_mutex_unlock(&s_read_lock);
    }
    // Otherwise, request an asynchronous update.
    else
      new_read_requested = 1;
    
    // Assign the result.
    if (pValue != NULL) {
        *pValue = latest_register_value;
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
 */
WTResult iox_write_register(IoxRegister reg, uint8_t value){
    pthread_mutex_lock(&s_write_lock);
    PI_TRY(WT_DEV_IOX, i2cWriteByteData(s_iox_i2c_fd, reg, value));
    pthread_mutex_unlock(&s_write_lock);
    return WT_OK;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *iox_thread(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_iox_thread_tid = gettid();

  // Set the thread CPU affinity.
  if (IOX_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(IOX_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", IOX_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", IOX_CPU);
  }

  // Main loop while application is running.
  CETI_LOG("Starting loop to read data in background");
  g_iox_thread_is_running = 1;
  while (!g_stopAcquisition) {
    // Perform a read operation if one has been requested.
    if(new_read_requested)
    {
      iox_read_register(NULL, 1);
      new_read_requested = 0;
    }
    // Wait for the desired polling period.
    usleep(IOX_READ_POLLING_PERIOD_US);
  }
  g_iox_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}
