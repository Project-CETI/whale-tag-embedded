/*
 * LightSensor.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 */

#include "LightSensor.h"

// Wait 100ms minimum after VDD is supplied to light sensor
HAL_StatusTypeDef Light_Sensor_Init(Light_Sensor_HandleTypedef *light_sensor, I2C_HandleTypeDef *hi2c_device) {
	HAL_StatusTypeDef ret_val = HAL_ERROR;
	light_sensor->i2c_handler = hi2c_device;
	// Maximum initial startup time is 1000 ms
	HAL_Delay(1000);

	ret_val = Light_Sensor_WakeUp(light_sensor, GAIN_DEF);
	// Wait 10ms maximum for wakeup time of light sensor
	// A non-blocking solution can probably be found
	HAL_Delay(10);

	// Uncomment lines below and change
	//Light_Sensor_Set_DataRate(light_sensor, INTEG_TIME_DEF, MEAS_TIME_DEF);

	return ret_val;

}

HAL_StatusTypeDef Light_Sensor_WakeUp(Light_Sensor_HandleTypedef *light_sensor, ALS_Gain gain){
	HAL_StatusTypeDef ret_val = HAL_ERROR;
	ALSControlRegister als_contr = {
		.gain = gain,
		.als_mode = LIGHT_WAKEUP
	};

	ret_val = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_CONTR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&als_contr, sizeof(als_contr), 100);


	return ret_val;
}


HAL_StatusTypeDef Light_Sensor_Set_DataRate(Light_Sensor_HandleTypedef *light_sensor, ALS_Integ_Time int_time, ALS_Meas_Rate meas_rate){
	HAL_StatusTypeDef ret_val = HAL_ERROR;
    ALSMeasureRateRegister meas_rate_reg = {
        .integration_time = int_time,
        .measurement_time = meas_rate
    };

	ret_val = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_MEAS_RATE, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&meas_rate_reg, sizeof(meas_rate_reg), 100);

	return ret_val;
}

HAL_StatusTypeDef Light_Sensor_Get_Data(Light_Sensor_HandleTypedef *light_sensor) {
	HAL_StatusTypeDef ret_val = HAL_ERROR;

	//read values and status together
    ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_DATA, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&light_sensor->status, 1, 100);

	if(ret_val != HAL_OK){
		return ret_val;
	}

	// Check ALS data valid bit. If bit is 1, data is invalid
	if(light_sensor->status.invalid){
		return HAL_ERROR;
	}
	// Check data status bit. If 0 data is old
	if(!light_sensor->status.new){
		return HAL_ERROR;
	}

	ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_DATA, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&light_sensor->data, 4, 100);

	return ret_val;
}

__const __inline uint8_t __ALSPartIDRegister_getPartNumber(uint8_t part_id_reg){
    return part_id_reg >> 4;
}

__const __inline uint8_t __ALSPartIDRegister_revision(uint8_t part_id_reg){
    return part_id_reg & 0x0F;
}

HAL_StatusTypeDef Light_Sensor_Sleep(Light_Sensor_HandleTypedef *light_sensor){
	HAL_StatusTypeDef ret_val = HAL_ERROR;
    ALSControlRegister als_contr = {
		.gain = light_sensor->gain,
		.als_mode = LIGHT_SLEEP
	};

	ret_val = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_CONTR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&als_contr, sizeof(als_contr), 100);

	return ret_val;
}

HAL_StatusTypeDef LightSensor_getPartID(Light_Sensor_HandleTypedef *light_sensor, ALSPartIDRegister *dst){
    HAL_StatusTypeDef ret_val = HAL_ERROR;
    ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_PART_ID_ADDR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)dst, sizeof(ALSPartIDRegister), 100);
    return ret_val;
}

HAL_StatusTypeDef LightSensor_getManufacturer(Light_Sensor_HandleTypedef *light_sensor, ALSManufacIDRegister *dst){
    ALSManufacIDRegister ret_val;
    ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_MANUFAC_ID_ADDR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)dst, sizeof(ALSManufacIDRegister), 100);
    return ret_val;
}
