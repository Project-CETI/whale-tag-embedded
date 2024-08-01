//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_IOX__
#define __CETI_WHALE_TAG_HAL_IOX__

#include "launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/timing.h"  // for get_global_time_us
#include "utils/error.h" // for WTResult type

#include <pigpio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h> // for uint8_t
#include <unistd.h> // for usleep()

// === Definitions ========================================================
#define IOX_GPIO_5V_EN (0)
#define IOX_GPIO_BOOT0 (1)
#define IOX_GPIO_3V3_RF_EN (2)
// free (3)
#define IOX_GPIO_BURNWIRE_ON (4)
// free (5)
#define IOX_GPIO_ECG_LOD_N  (6)
#define IOX_GPIO_ECG_LOD_P  (7)

#define IOX_READ_POLLING_PERIOD_US 1000

// === Type Definitions =======================================================
typedef enum {
    IOX_MODE_OUTPUT = 0,
    IOX_MODE_INPUT = 1,
} WtIoxMode;

typedef enum iox_register_e {
    IOX_REG_INPUT = 0x00,
    IOX_REG_OUTPUT = 0x01,
    IOX_REG_POLARITY = 0x02,
    IOX_REG_CONFIGURATION = 0x03,
    IOX_REG_STRENGTH_0 = 0x40,
    IOX_REG_STRENGTH_1 = 0x41,
    IOX_REG_INPUT_LATCH = 0x42,
    IOX_REG_PUPD_ENABLE = 0x43,
    IOX_REG_PUPD_SELECT = 0x44,
    IOX_REG_INTERRUPT_MASK = 0x45,
    IOX_REG_INTERRUPT_STATUS = 0x46,
    IOX_REG_OUTPUT_PORT_CONFIG = 0x4F,
} IoxRegister;

// === Functions ==============================================================
WTResult init_iox(void);
void iox_terminate(void);
WTResult iox_get_mode(int pin, WtIoxMode *mode);
WTResult iox_read_pin(int pin, int *value, int wait_for_new_value);
WTResult iox_read_register(uint8_t *value, int wait_for_new_value);
WTResult iox_set_mode(int pin, WtIoxMode mode);
WTResult iox_write(int pin, int value);
WTResult iox_write_register(IoxRegister reg, uint8_t value);
void* iox_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_iox_thread_is_running;

#endif // __CETI_WHALE_TAG_HAL_IOX_
