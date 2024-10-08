
//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_I2C_H__
#define __CETI_WHALE_TAG_HAL_I2C_H__

// I2C bus 0

// I2C bus 1
#define ALS_I2C_BUS (1)
#define BMS_I2C_BUS (1)
#define IOX_I2C_BUS (1)
#define RTC_I2C_BUS (1)

// BMS I2C Device Address
#define BMS_I2C_DEV_ADDR_UPPER (0x0b) // For internal memory range 180h-1FFh
#define IOX_I2C_DEV_ADDR (0x21)
#define ALS_I2C_DEV_ADDR (0x29)
#define BMS_I2C_DEV_ADDR_LOWER (0x36) // For internal memory range 000h-0FFh
#define RTC_I2C_DEV_ADDR (0x68)

#endif // __CETI_WHALE_TAG_HAL_I2C_H__
