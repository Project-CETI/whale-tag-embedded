/*
 * KellerDepth.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 */

#include "KellerDepth.h"


void Keller_init(Keller_HandleTypedef *keller_sensor, I2C_HandleTypeDef *hi2c_device) {
	keller_sensor->i2c_handler = hi2c_device;
}

HAL_StatusTypeDef Keller_get_data(Keller_HandleTypedef *keller_sensor) {
	HAL_StatusTypeDef ret_val = HAL_ERROR;
	uint8_t data_buf[1] = {0};
	data_buf[0] = KELLER_REQ;

	ret_val = HAL_I2C_Master_Transmit(keller_sensor->i2c_handler, 0x40 << 1, data_buf, 1, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	// Wait >=8ms or wait for EOC to go high (VDD) or check status byte for the busy flag
	// The easy solution is to wait for 8ms>, the faster solution is to read the Busy flag or (if MCU pin available) EOC pin
	HAL_Delay(8);

	ret_val = HAL_I2C_Master_Receive(keller_sensor->i2c_handler, KELLER_ADDR << 1, keller_sensor->raw_data, KELLER_DLEN, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	keller_sensor->status = keller_sensor->raw_data[0];
	keller_sensor->raw_pressure = ((uint16_t)keller_sensor->raw_data[1] << 8) | keller_sensor->raw_data[2];
	keller_sensor->raw_temp = ((uint16_t)keller_sensor->raw_data[3] << 8) | keller_sensor->raw_data[4];

	keller_sensor->temperature = TO_DEGC(keller_sensor->raw_temp);
	keller_sensor->pressure = TO_BAR(keller_sensor->raw_pressure);

	return ret_val;
}

