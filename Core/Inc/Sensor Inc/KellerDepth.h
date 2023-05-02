/*
 * KellerDepth.h
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 *      Sensor: Keller Pressure Transmitter series 4LD
 *      Datasheet: keller-druck.com/en/products/pressure-transmitters/oem-pressure-transmitters/series-4ld
 */

#ifndef KELLERDEPTH_H
#define KELLERDEPTH_H

#include "stm32u5xx_hal.h"

// Keller sensor I2C address
#define KELLER_ADDR 	0x40

// Request data address
#define KELLER_REQ		0xAC
#define KELLER_DLEN		5

#define P_MIN			0.0f
#define P_MAX			200.0f

#define TO_DEGC(RAW_IN)	(float)(((RAW_IN >> 4) - 24.0f) * 0.05 - 50.0f)
#define TO_BAR(RAW_IN)	(float)((RAW_IN - 16384) * (P_MAX - P_MIN) / 32768 + P_MIN)


typedef struct __Keller_Depth_TypeDef
{
	I2C_HandleTypeDef *i2c_handler;

	//Data buffer for I2C data
	uint8_t raw_data[5];

	//raw values read from sensor
	uint8_t status;
	uint16_t raw_pressure;
	uint16_t raw_temp;

	//Pressure in Bar
	float pressure;

	//Temperature in celsius
	float temperature;

} Keller_HandleTypedef;

void Keller_Init(Keller_HandleTypedef *keller_sensor, I2C_HandleTypeDef *hi2c_device);
HAL_StatusTypeDef Keller_Get_Data(Keller_HandleTypedef *keller_sensor);

#endif /* KELLERDEPTH_H */
