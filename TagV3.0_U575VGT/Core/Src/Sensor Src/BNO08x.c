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
uint32_t good_counter = 0;
uint32_t bad_counter = 0;
void IMU_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu){

	imu->hspi = hspi;

	//Need to setup the IMU to send the appropriate data to us. Transmit a "set feature" command to start receiving rotation data.
	//All non-populated bytes are left as default 0.
	uint8_t transmitData[256] = {0};
	//uint8_t receiveData[256] = {0};

	while (HAL_GPIO_ReadPin(IMU_INT_GPIO_Port, IMU_INT_Pin)){};

	HAL_Delay(1000);

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
	transmitData[12] = IMU_REPORT_INTERVAL_3; //MSB

	//Select CS by pulling low and write configuration to the IMU
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, transmitData, IMU_CONFIGURE_ROTATION_VECTOR_REPORT_LENGTH, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	uint8_t testTransmit[256] = {0};
	uint8_t testReceive[256] = {0};


	//TEST - get feature
	/*while (HAL_GPIO_ReadPin(IMU_INT_GPIO_Port, IMU_INT_Pin)){};

	//Configure SHTP header (first 4 bytes)
	testTransmit[0] = 6; //LSB
	testTransmit[1] = 0x00; //MSB
	testTransmit[2] = IMU_CONTROL_CHANNEL;

	testTransmit[4] = 0xFE;
	testTransmit[5] = IMU_ROTATION_VECTOR_REPORT_ID;

	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(hspi, testTransmit, testReceive, 6, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	while(1){
		//Poll for falling edge on interrupt pin
	while (HAL_GPIO_ReadPin(IMU_INT_GPIO_Port, IMU_INT_Pin)){};
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Receive(hspi, testReceive, IMU_ROTATION_VECTOR_REPORT_LENGTH, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);
	if ((testReceive[4] == 0xFB) && (testReceive[9] == 0x05)){
		HAL_Delay(1);
		good_counter++;
	}else {
		bad_counter++;
	}
	}*/
}

void IMU_get_data(IMU_HandleTypeDef* imu){

	uint8_t receiveData[256] = {0};

	//loop of reading data
	while(1){

		//Poll for the data to be ready (observe interrupt line)
		while (HAL_GPIO_ReadPin(IMU_INT_GPIO_Port, IMU_INT_Pin)){};
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

			}
		}

	}
}
