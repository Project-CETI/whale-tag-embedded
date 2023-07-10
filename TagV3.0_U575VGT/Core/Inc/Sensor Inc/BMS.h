/*
 * BMS.h
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 *      Sensor: DS2778
 *      Datasheet: analog.com/media/en/technical-documentation/data-sheets/DS2775-DS2778.pdf
 */

#ifndef BMS_H
#define BMS_H

#include "stm32u5xx_hal.h"

// Peripheral 7-bit Slave Address, (can be changed) check datasheet
#define BMS_ADDR	0b01011001

// DS2778 Data Registers
#define V1_ADDR		0x0C // 0x0C == MSB, 0x0D == LSB
#define V2_ADDR		0x1C // 0x1C == MSB, 0x1D == LSB
#define I_ADDR		0x0E // 0x0E == MSB, 0x0F == LSB
#define TEMP_ADDR	0x0A // 0x0A == MSB, 0x0B == LSB

#define BMS_DLEN	2

// Control registers
#define BMS_CTRL	0x60
#define BMS_OV_ADDR	0x7F

// set bits: NBEN, undervoltage threshold 2.60 V, and PSPIO bits
#define BMS_CTL_VAL	0x8E

// Overvoltage threshold, V = (678 + 2 * BMS_VOV) * 5 / 1024
// With 90, V = 4.19 (rounded)
#define BMS_VOV_VAL	90

#define R_SENSE		0.025

// Voltage units: 4.8828e-3 V, macro outputs value in V
#define TO_V(RAW_IN)	(float)((short)((RAW_IN[0] << 3) & (RAW_IN[1] >> 5)) *  4.8828E-3)

// Current units: 1.5625μV/RSNS, macro outputs value in mA
#define TO_I(RAW_IN)	(float)((short)((RAW_IN[0] << 8) & RAW_IN[1]) * 1.5625E-3 / R_SENSE)

// Temperature units: 0.125 C, macro outputs value in C
#define TO_C(RAW_IN)	(float)((short)((RAW_IN[0] << 3) & (RAW_IN[1] >> 5)) * 0.125)

typedef struct __BMS_TypeDef
{
	I2C_HandleTypeDef *i2c_handler;

	// Data buffers for I2C data
	uint8_t raw_v1[2];
	uint8_t raw_v2[2];
	uint8_t raw_i[2];
	uint8_t raw_temp[2];

	float v1;
	float v2;
	float i;
	float temp;

} BMS_HandleTypedef;

HAL_StatusTypeDef BMS_Init(BMS_HandleTypedef *BMS, I2C_HandleTypeDef *hi2c_device);
HAL_StatusTypeDef BMS_Get_Batt_Data(BMS_HandleTypedef *BMS);
HAL_StatusTypeDef BMS_Get_Temp(BMS_HandleTypedef *BMS);

#endif /* BMS_H */
