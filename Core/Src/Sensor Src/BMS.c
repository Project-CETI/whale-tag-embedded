/*
 * BMS.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 */

#include "BMS.h"

HAL_StatusTypeDef BMS_Init(BMS_HandleTypedef *BMS, I2C_HandleTypeDef *hi2c_device) {
	HAL_StatusTypeDef ret_val = HAL_ERROR;
	BMS->i2c_handler = hi2c_device;
	uint8_t data_buf = 0;

	// Set undervoltage threshold
	data_buf = BMS_CTL_VAL;
	ret_val = HAL_I2C_Mem_Write(BMS->i2c_handler, BMS_ADDR << 1, BMS_CTRL, I2C_MEMADD_SIZE_8BIT, &data_buf, 1, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	// Set overvoltage threshold
	data_buf = BMS_VOV_VAL;
	ret_val = HAL_I2C_Mem_Write(BMS->i2c_handler, BMS_ADDR << 1, BMS_OV_ADDR, I2C_MEMADD_SIZE_8BIT, &data_buf, 1, 100);

	return ret_val;
}

HAL_StatusTypeDef BMS_Get_Batt_Data(BMS_HandleTypedef *BMS) {
	HAL_StatusTypeDef ret_val = HAL_ERROR;

	ret_val = HAL_I2C_Mem_Read(BMS->i2c_handler, BMS_ADDR << 1, V1_ADDR, I2C_MEMADD_SIZE_8BIT, BMS->raw_v1, BMS_DLEN, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	BMS->v1 = TO_V(BMS->raw_v1);

	ret_val = HAL_I2C_Mem_Read(BMS->i2c_handler, BMS_ADDR << 1, V2_ADDR, I2C_MEMADD_SIZE_8BIT, BMS->raw_v2, BMS_DLEN, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	BMS->v2 = TO_V(BMS->raw_v2);

	ret_val = HAL_I2C_Mem_Read(BMS->i2c_handler, BMS_ADDR << 1, I_ADDR, I2C_MEMADD_SIZE_8BIT, BMS->raw_i, BMS_DLEN, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	BMS->i = TO_V(BMS->raw_i);

	return ret_val;
}

HAL_StatusTypeDef BMS_Get_Temp(BMS_HandleTypedef *BMS) {
	HAL_StatusTypeDef ret_val = HAL_ERROR;

	ret_val = HAL_I2C_Mem_Read(BMS->i2c_handler, BMS_ADDR << 1, TEMP_ADDR, I2C_MEMADD_SIZE_8BIT, BMS->raw_temp, BMS_DLEN, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	BMS->temp = TO_C(BMS->raw_temp);

	return ret_val;
}
