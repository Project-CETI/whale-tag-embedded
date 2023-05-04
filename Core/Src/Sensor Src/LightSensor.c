/*
 * LightSensor.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 */

#include "LightSensor.h"
#include "util.h"

/*** PRIVATE ***/
static inline ALSControlReg __controlReg_from_raw(uint8_t raw){
	return (ALSControlReg){
		.gain = _RSHIFT(raw, 2, 3),
		.sw_reset = _RSHIFT(raw, 1, 1),
		.als_mode = _RSHIFT(raw, 0, 1),
	};
}

static inline uint8_t __controlReg_into_raw(ALSControlReg *reg){
	return _LSHIFT(reg->gain, 2, 3) 
		| _LSHIFT(reg->sw_reset, 1, 1) 
		| _LSHIFT(reg->als_mode, 0, 1);
}

static inline ALSStatusReg __statusReg_from_raw(uint8_t raw){
	return (ALSStatusReg){
		.invalid = _RSHIFT(raw, 7, 1),
		.gain = _RSHIFT(raw, 4, 3),
		.new = _RSHIFT(raw, 2, 1),
	};
}

static inline ALSMeasureRateReg __measureRateReg_from_raw(uint8_t raw){
	(ALSMeasureRateReg){
		.measurement_time = _RSHIFT(raw, 0, 3),
		.integration_time =  _RSHIFT(raw, 3, 3),
	};
}

static inline uint8_t __measureRateReg_into_raw(ALSMeasureRateReg *reg){
	return _LSHIFT(reg->measurement_time, 0, 3) 
		| _LSHIFT(reg->integration_time, 3, 3);
}

static inline ALSPartIDReg __partIDReg_from_raw(uint8_t raw){
	return (ALSPartIDReg){
		.revision_id = _RSHIFT(raw, 0, 4),
		.part_number_id = _RSHIFT(raw, 4, 4),
	};
}

static inline uint8_t __partIDReg_into_raw(ALSPartIDReg * reg){
	return _LSHIFT(reg->revision_id, 0, 4)
		|  _LSHIFT(reg->part_number_id, 4, 4);
}

/*** PUBLIC ***/
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

HAL_StatusTypeDef Light_Sensor_WakeUp(Light_Sensor_HandleTypedef *light_sensor, ALSGain gain){
	uint8_t control_raw = __controlReg_into_raw(&(ALSControlReg){
		.gain = gain,
		.als_mode = LIGHT_WAKEUP
	});

	HAL_StatusTypeDef ret_val = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_CONTR, I2C_MEMADD_SIZE_8BIT, &control_raw, sizeof(control_raw), 100);


	return ret_val;
}


HAL_StatusTypeDef Light_Sensor_Set_DataRate(Light_Sensor_HandleTypedef *light_sensor, ALS_Integ_Time int_time, ALS_Meas_Rate meas_rate){
    uint8_t raw = __measureRateReg_into_raw(&(ALSMeasureRateReg){
        .integration_time = int_time,
        .measurement_time = meas_rate
    });

	HAL_StatusTypeDef ret_val = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_MEAS_RATE, I2C_MEMADD_SIZE_8BIT, &raw, sizeof(raw), 100);

	return ret_val;
}



HAL_StatusTypeDef Light_Sensor_Get_Data(Light_Sensor_HandleTypedef *light_sensor) {
	uint8_t status_raw;

	//read values and status together
    HAL_StatusTypeDef ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_DATA, I2C_MEMADD_SIZE_8BIT, &status_raw, 1, 100);
	if(ret_val != HAL_OK){
		return ret_val;
	}

	light_sensor->status = __statusReg_from_raw(status_raw);

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

HAL_StatusTypeDef Light_Sensor_Sleep(Light_Sensor_HandleTypedef *light_sensor){
	HAL_StatusTypeDef ret_val = HAL_ERROR;
    uint8_t raw = __controlReg_into_raw(&(ALSControlReg){
		.gain = light_sensor->gain,
		.als_mode = LIGHT_SLEEP
	});

	ret_val = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_CONTR, I2C_MEMADD_SIZE_8BIT, &raw, sizeof(raw), 100);

	return ret_val;
}

HAL_StatusTypeDef LightSensor_getPartID(Light_Sensor_HandleTypedef *light_sensor, ALSPartIDReg *dst){
    HAL_StatusTypeDef ret_val = HAL_ERROR;
	uint8_t raw = __partIDReg_into_raw(dst);
    ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_PART_ID_ADDR, I2C_MEMADD_SIZE_8BIT, &raw, sizeof(uint8_t), 100);
    return ret_val;
}

HAL_StatusTypeDef LightSensor_getManufacturer(Light_Sensor_HandleTypedef *light_sensor, ALSManufacIDRegister *dst){
    ALSManufacIDRegister ret_val;
    ret_val = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_MANUFAC_ID_ADDR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)dst, sizeof(ALSManufacIDRegister), 100);
    return ret_val;
}
