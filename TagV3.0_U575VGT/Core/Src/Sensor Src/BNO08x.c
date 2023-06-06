/*
 * BNO08x.c
 *
 *  Created on: Feb 8, 2023
 *      Author: Amjad Halis, Kaveet Grewal
 *
 * 	Source file for the IMU drivers. See header file for more details
 */

#include "BNO08x.h"
#include "stm32u5xx_hal_gpio.h"
#include "stm32u5xx_hal_spi.h"
#include <stdbool.h>
uint32_t good_counter = 0;
uint32_t bad_counter = 0;
uint32_t reinit_counter = 0;

static void IMU_read_startup_data(IMU_HandleTypeDef* imu);
static HAL_StatusTypeDef IMU_poll_new_data(IMU_HandleTypeDef* imu, uint32_t timeout);

void IMU_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu){

	imu->hspi = hspi;

	//GPIO info
	imu->cs_port = IMU_CS_GPIO_Port;
	imu->cs_pin = IMU_CS_Pin;

	imu->int_port = IMU_INT_GPIO_Port;
	imu->int_pin = IMU_INT_Pin;

	imu->wake_port = IMU_WAKE_GPIO_Port;
	imu->wake_pin = IMU_WAKE_Pin;


	HAL_Delay(1000);
	IMU_read_startup_data(imu);

	HAL_GPIO_WritePin(imu->wake_port, imu->wake_pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(imu->wake_port, imu->wake_pin, GPIO_PIN_RESET);

	if (IMU_poll_new_data(imu, HAL_MAX_DELAY) == HAL_TIMEOUT){
		return;
	}

	//Need to setup the IMU to send the appropriate data to us. Transmit a "set feature" command to start receiving rotation data.
	//All non-populated bytes are left as default 0.
	uint8_t transmitData[256] = {0};

	//Configure SHTP header (first 4 bytes)
	transmitData[0] = IMU_CONFIGURE_ROTATION_VECTOR_REPORT_LENGTH; //LSB
	transmitData[1] = 0x00; //MSB
	transmitData[2] = IMU_CONTROL_CHANNEL;

	//Indicates we want to start receiving a report
	transmitData[4] = IMU_SET_FEATURE_REPORT_ID;

	//Indicates we want to receive rotation vector reports
	transmitData[5] = IMU_ROTATION_VECTOR_REPORT_ID;

	//Set how often we want to receive data -> 0x0007A120 results in data every 0.5 seconds
	transmitData[9] = IMU_REPORT_INTERVAL_0; //LSB
	transmitData[10] = IMU_REPORT_INTERVAL_1;
	transmitData[11] = IMU_REPORT_INTERVAL_2;
	transmitData[12] = IMU_REPORT_INTERVAL_3; //MSBs

	//Select CS by pulling low and write configuration to the IMU. Add delays to ensure good timing.
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_SPI_Transmit(hspi, transmitData, IMU_CONFIGURE_ROTATION_VECTOR_REPORT_LENGTH, HAL_MAX_DELAY);
	HAL_Delay(1);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	//Deassert the WAKE pin since the chip is now awake and we are done configuring
	HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef IMU_get_data(IMU_HandleTypeDef* imu){

	//receive data buffer
	uint8_t receiveData[256] = {0};

	//Poll for the data to be ready (observe interrupt line)
	if (IMU_poll_new_data(imu, IMU_NEW_DATA_TIMEOUT_MS) == HAL_TIMEOUT){
		IMU_init(imu->hspi, imu);
		reinit_counter++;
	}

	//Read the data into a buffer
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Receive(imu->hspi, receiveData, IMU_ROTATION_VECTOR_REPORT_LENGTH, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	//Ensure this is the correct channel we're receiving data on
	if (receiveData[2] == IMU_DATA_CHANNEL){
		//Ensure that this is the correct data (timestamp + rotation vector)
		if ((receiveData[4] == IMU_TIMESTAMP_REPORT_ID) && (receiveData[9] == IMU_ROTATION_VECTOR_REPORT_ID)){

			//Save data parameters to our struct. We need to combine the MSB and LSB to get the correct 16bit value.
			imu->quat_i = receiveData[14] << 8 | receiveData[13];
			imu->quat_j = receiveData[16] << 8 | receiveData[15];
			imu->quat_k = receiveData[18] << 8 | receiveData[17];
			imu->quat_r = receiveData[20] << 8 | receiveData[19];
			imu->accurary_rad = receiveData[22] << 8 | receiveData[21];

			good_counter++;
			return HAL_OK;
		}
	}else{
		bad_counter++;
	}


	return HAL_ERROR;
}

static void IMU_read_startup_data(IMU_HandleTypeDef* imu){

	bool reading = true;
	uint8_t receiveData[256] = {0};

	//Track start time
	uint32_t startTime = HAL_GetTick();

	while (reading){

		if (IMU_poll_new_data(imu, IMU_DUMMY_PACKET_TIMEOUT_MS) == HAL_TIMEOUT){
			reading = false;
		}

		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_SPI_Receive(imu->hspi, receiveData, 100, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);
	}
}

static HAL_StatusTypeDef IMU_poll_new_data(IMU_HandleTypeDef* imu, uint32_t timeout){

	uint32_t startTime = HAL_GetTick();

	while (HAL_GPIO_ReadPin(imu->int_port, imu->int_pin)){

		uint32_t currentTime = HAL_GetTick();

		if ((currentTime - startTime) > timeout){
			return HAL_TIMEOUT;
		}
	}

	return HAL_OK;
}
