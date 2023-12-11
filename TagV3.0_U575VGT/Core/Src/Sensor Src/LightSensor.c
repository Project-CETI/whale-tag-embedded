/*
 * LightSensor.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 *      Sensor: LTR-329ALS-01_DS_V1
 *      Datasheet: optoelectronics.liteon.com/upload/download/DS86-2014-0006/LTR-329ALS-01_DS_V1.pdf
 */

#include "Sensor Inc/LightSensor.h"
#include "util.h"
#include "main.h"
#include "ux_device_cdc_acm.h"
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart2;

// usb buffers
extern uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitLen;

// light sensor handler
extern LightSensorHandleTypedef light_sensor;

// threadX flags for light sensor
TX_EVENT_FLAGS_GROUP light_event_flags_group;

// uart debugging
bool light_get_samples = false;
bool light_unit_test = false;
HAL_StatusTypeDef light_unit_test_ret = HAL_ERROR;

/*** PRIVATE ***/

static inline ALSControlRegister __controlRegister_from_raw(uint8_t raw){
	return (ALSControlRegister){
		.gain = _RSHIFT(raw, 2, 3),
		.sw_reset = _RSHIFT(raw, 1, 1),
		.als_mode = _RSHIFT(raw, 0, 1),
	};
}

static inline uint8_t __controlRegister_into_raw(ALSControlRegister *reg){
	return _LSHIFT(reg->gain, 2, 3) 
		| _LSHIFT(reg->sw_reset, 1, 1) 
		| _LSHIFT(reg->als_mode, 0, 1);
}

static inline ALSStatusRegister __statusRegister_from_raw(uint8_t raw){
	return (ALSStatusRegister){
		.invalid = _RSHIFT(raw, 7, 1),
		.gain = _RSHIFT(raw, 4, 3),
		.new = _RSHIFT(raw, 2, 1),
	};
}

static inline uint8_t __statusRegister_into_raw(ALSStatusRegister *reg){
	return _LSHIFT(reg->invalid, 7, 1)
		| _LSHIFT(reg->gain, 4, 3)
		| _LSHIFT(reg->new, 2, 1);
}

static inline ALSMeasureRateRegister __measureRateReg_from_raw(uint8_t raw){
	return (ALSMeasureRateRegister){
		.measurement_time = _RSHIFT(raw, 0, 3),
		.integration_time =  _RSHIFT(raw, 3, 3),
	};
}

static inline uint8_t __measureRateReg_into_raw(ALSMeasureRateRegister *reg){
	return _LSHIFT(reg->measurement_time, 0, 3) 
		| _LSHIFT(reg->integration_time, 3, 3);
}

static inline ALSPartIDRegister __partIDReg_from_raw(uint8_t raw){
	return (ALSPartIDRegister){
		.revision_id = _RSHIFT(raw, 0, 4),
		.part_number_id = _RSHIFT(raw, 4, 4),
	};
}

static inline uint8_t __partIDReg_into_raw(ALSPartIDRegister * reg){
	return _LSHIFT(reg->revision_id, 0, 4)
		|  _LSHIFT(reg->part_number_id, 4, 4);
}

static inline ALSIntegrationTime __ALSIntegrationTime_get_max_from_freq_Hz(float freq_Hz){
	return (freq_Hz < ( 1.0 / 0.40 ))? ALS_INTEG_TIME_400_MS
		 : (freq_Hz < ( 1.0 / 0.35 ))? ALS_INTEG_TIME_350_MS
		 : (freq_Hz < ( 1.0 / 0.30 ))? ALS_INTEG_TIME_300_MS
		 : (freq_Hz < ( 1.0 / 0.25 ))? ALS_INTEG_TIME_250_MS
		 : (freq_Hz < ( 1.0 / 0.20 ))? ALS_INTEG_TIME_200_MS
		 : (freq_Hz < ( 1.0 / 0.15 ))? ALS_INTEG_TIME_150_MS
		 : (freq_Hz < ( 1.0 / 0.10 ))? ALS_INTEG_TIME_100_MS
		 : ALS_INTEG_TIME_50_MS;
}

static inline ALSMeasureTime __ALSMeasureTime_get_max_from_freq_Hz(float freq_Hz){
	return (freq_Hz <= ( 1.0 / 2.0 ))? ALS_MEAS_TIME_2000_MS
		 : (freq_Hz <= ( 1.0 / 1.0 ))? ALS_MEAS_TIME_1000_MS
		 : (freq_Hz <= ( 1.0 / 0.5 ))? ALS_MEAS_TIME_500_MS
		 : (freq_Hz <= ( 1.0 / 0.2 ))? ALS_MEAS_TIME_200_MS
		 : (freq_Hz <= ( 1.0 / 0.1 ))? ALS_MEAS_TIME_100_MS
		 : ALS_MEAS_TIME_50_MS;
}

/*** PUBLIC ***/

void light_thread_entry(ULONG thread_input) {

	//Create event flags
	tx_event_flags_create(&light_event_flags_group, "Light Event Flags");

	//Initalize chip
	LightSensor_init(&hi2c2, &light_sensor);

	while (1) {
		ULONG actual_flags = 0;
		HAL_StatusTypeDef ret = HAL_ERROR;

		tx_event_flags_get(&light_event_flags_group, LIGHT_UNIT_TEST_FLAG | LIGHT_CMD_FLAG , TX_OR_CLEAR, &actual_flags, 1);
		if (actual_flags & LIGHT_UNIT_TEST_FLAG) {
			light_unit_test = true;
		}
		else if (actual_flags & LIGHT_READ_FLAG) {

		}
		else if (actual_flags & LIGHT_WRITE_FLAG) {

		}
		else if (actual_flags & LIGHT_CMD_FLAG) {
			if (usbReceiveBuf[3] == LIGHT_GET_SAMPLES_CMD) {
				light_get_samples = true;
			}
		}

		ret = LightSensor_get_data(&light_sensor);
		if (light_unit_test) {
			light_unit_test_ret = ret;
			light_unit_test = false;
			tx_event_flags_set(&light_event_flags_group, LIGHT_UNIT_TEST_DONE_FLAG, TX_OR);
		}
		HAL_Delay(1000);
	}
}

// Wait 100ms minimum after VDD is supplied to light sensor
HAL_StatusTypeDef LightSensor_init(I2C_HandleTypeDef *hi2c_device, LightSensorHandleTypedef *light_sensor) {
	
	light_sensor->i2c_handler = hi2c_device;
	
	// Maximum initial startup time is 1000 ms
	HAL_Delay(1000);

	HAL_StatusTypeDef ret = LightSensor_wake_up(light_sensor, GAIN_DEF);

	// Uncomment lines below and change
	LightSensor_set_data_rate(light_sensor, ALS_INTEG_TIME_100_MS, ALS_MEAS_TIME_500_MS);

	return ret;

}

HAL_StatusTypeDef LightSensor_set_sample_rate_Hz(LightSensorHandleTypedef *light_sensor, float freq_Hz){
	return LightSensor_set_data_rate(light_sensor, 
		__ALSIntegrationTime_get_max_from_freq_Hz(freq_Hz), 
		__ALSMeasureTime_get_max_from_freq_Hz(freq_Hz)
	);
}

HAL_StatusTypeDef LightSensor_wake_up(LightSensorHandleTypedef *light_sensor, ALSGain gain){
	uint8_t control_raw = __controlRegister_into_raw(&(ALSControlRegister){
		.gain = gain,
		.als_mode = LIGHT_WAKEUP
	});

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_CONTR, I2C_MEMADD_SIZE_8BIT, &control_raw, sizeof(control_raw), 100);
	if(ret != HAL_OK){
		return ret;
	}
	// Waits 10ms maximum for wakeup time of light sensor
	// A non-blocking solution can be found
	HAL_Delay(10);
	
	return HAL_OK;
}


HAL_StatusTypeDef LightSensor_set_data_rate(LightSensorHandleTypedef *light_sensor, ALSIntegrationTime int_time, ALSMeasureTime meas_rate){
    uint8_t raw = __measureRateReg_into_raw(&(ALSMeasureRateRegister){
        .integration_time = int_time,
        .measurement_time = meas_rate
    });

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_MEAS_RATE, I2C_MEMADD_SIZE_8BIT, &raw, sizeof(raw), 100);

	return ret;
}


HAL_StatusTypeDef LightSensor_get_data(LightSensorHandleTypedef *light_sensor) {
	uint8_t status_raw;

	//read values and status together
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_STATUS, I2C_MEMADD_SIZE_8BIT, &status_raw, 1, 100);
	if(ret != HAL_OK){
		return ret;
	}

	light_sensor->status = __statusRegister_from_raw(status_raw);

	// Check ALS data valid bit. If bit is 1, data is invalid
	if(light_sensor->status.invalid){
		return HAL_ERROR;
	}
	// Check data status bit. If 0 data is old
	if(!light_sensor->status.new){
		return HAL_ERROR;
	}

	ret = HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_DATA, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&light_sensor->data, 4, 100);

	if (light_get_samples) {

		// send light data over uart and usb
		HAL_UART_Transmit_IT(&huart2, &ret, 1);
		HAL_UART_Transmit_IT(&huart2, (uint8_t *) &light_sensor->data, sizeof(light_sensor->data));

		if (usbTransmitLen == 0) {
			memcpy(&usbTransmitBuf[0], &ret, 1);
			memcpy(&usbTransmitBuf[1], &light_sensor->data, sizeof(light_sensor->data));
			usbTransmitLen = sizeof(light_sensor->data) + 1;
		}
		light_get_samples = false;
	}

	return ret;
}

HAL_StatusTypeDef LightSensor_sleep(LightSensorHandleTypedef *light_sensor){
    uint8_t raw = __controlRegister_into_raw(&(ALSControlRegister){
		.gain = light_sensor->gain,
		.als_mode = LIGHT_SLEEP
	});

	return HAL_I2C_Mem_Write(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_CONTR, I2C_MEMADD_SIZE_8BIT, &raw, sizeof(raw), 100);
}

HAL_StatusTypeDef LightSensor_get_part_id(LightSensorHandleTypedef *light_sensor, ALSPartIDRegister *dst){
	uint8_t raw;
	HAL_RESULT_PROPAGATE(HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_PART_ID_ADDR, I2C_MEMADD_SIZE_8BIT, &raw, sizeof(uint8_t), 100));
    *dst = __partIDReg_from_raw(raw);
    return HAL_OK;
}

HAL_StatusTypeDef LightSensor_get_manufacturer(LightSensorHandleTypedef *light_sensor, ALSManufacIDRegister *dst){
    return HAL_I2C_Mem_Read(light_sensor->i2c_handler, ALS_ADDR << 1, ALS_MANUFAC_ID_ADDR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)dst, sizeof(ALSManufacIDRegister), 100);
}
