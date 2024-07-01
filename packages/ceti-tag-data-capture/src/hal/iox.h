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

// === Public Libraries =======================================================
#include "hal/error.h"

#include <stdint.h>

// === Pin Definitions ========================================================
#define WT_IOX_GPIO_5V_EN (0)
#define WT_IOX_GPIO_BOOT0 (1)
#define WT_IOX_GPIO_3V3_RF_EN (2)
// free (3)
#define WT_IOX_GPIO_BURNWIRE_ON (4)
// free (5)
#define WT_IOX_GPIO_ECG_LOD_N  (6)
#define WT_IOX_GPIO_ECG_LOD_P  (7)

// === Type Definitions =======================================================

typedef enum {
    WT_IOX_MODE_OUTPUT = 0,
    WT_IOX_MODE_INPUT = 1,
} WtIoxMode;

typedef enum pcal6408a_register_e {
    WT_IOX_REG_INPUT = 0x00,
    WT_IOX_REG_OUTPUT = 0x01,
    WT_IOX_REG_POLARITY = 0x02,
    WT_IOX_REG_CONFIGURATION = 0x03,
    WT_IOX_REG_STRENGTH_0 = 0x40,
    WT_IOX_REG_STRENGTH_1 = 0x41,
    WT_IOX_REG_INPUT_LATCH = 0x42,
    WT_IOX_REG_PUPD_ENABLE = 0x43,
    WT_IOX_REG_PUPD_SELECT = 0x44,
    WT_IOX_REG_INTERRUPT_MASK = 0x45,
    WT_IOX_REG_INTERRUPT_STATUS = 0x46,
    WT_IOX_REG_OUTPUT_PORT_CONFIG = 0x4F,
} PCAL6408ARegister;

// === Functions ==============================================================
WTResult wt_iox_init(void);
void wt_iox_terminate(void);
WTResult wt_iox_set_mode(int pin, WtIoxMode mode);
WTResult wt_iox_get_mode(int pin, WtIoxMode *mode);
WTResult wt_iox_read(int pin, int *value);
WTResult wt_iox_read_register(PCAL6408ARegister register, uint8_t *value);
WTResult wt_iox_write(int pin, int value);
WTResult wt_iox_write_register(PCAL6408ARegister register, uint8_t value);

#endif // __CETI_WHALE_TAG_HAL_IOX__
