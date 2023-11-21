/*
 * IMU.c
 *
 *  Created on: Nov 16, 2023
 *      Author: kevinma
 */

#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/audio.h"
#include "fx_api.h"
#include "app_filex.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include "stm32u5xx_hal_gpio.h"
#include "stm32u5xx_hal_spi.h"
#include "Lib Inc/threads.h"
#include <stdbool.h>

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart4;

extern Thread_HandleTypeDef threads[NUM_THREADS];

// threadX mutex and event flags
TX_EVENT_FLAGS_GROUP imu_event_flags_group;
TX_MUTEX imu_first_half_mutex;
TX_MUTEX imu_second_half_mutex;

// array split in half for holding IMU data
IMU_Data imu_data[2][IMU_HALF_BUFFER_SIZE] = {0};

// for debugging
bool debug = true;

void imu_thread_entry(ULONG thread_input) {

	//Create the events flag.
	tx_event_flags_create(&imu_event_flags_group, "IMU Event Flags");
	tx_mutex_create(&imu_first_half_mutex, "IMU First Half Mutex", TX_INHERIT);
	tx_mutex_create(&imu_second_half_mutex, "IMU Second Half Mutex", TX_INHERIT);

	//Enable our interrupt handler that signals data is ready
	HAL_NVIC_EnableIRQ(EXTI12_IRQn);

	IMU_HandleTypeDef imu;
	imu_init(&hspi1, &imu);

	tx_thread_resume(&threads[DEPTH_THREAD].thread);

	while (1) {
		ULONG actual_flags = 0;

		/*
		tx_event_flags_get(&imu_event_flags_group, IMU_UNIT_TEST_FLAG | IMU_GET_SAMPLE_FLAG | IMU_RESET_FLAG, TX_OR_CLEAR, &actual_flags, IMU_CMD_WAIT_TIMEOUT);
		if (actual_flags & IMU_UNIT_TEST_FLAG) {

			// get product id
			HAL_StatusTypeDef ret = imu_get_product_id(&imu);

			//send result over UART
			HAL_UART_Transmit_IT(&huart4, ret, 1);
		}
		else if (actual_flags & IMU_GET_SAMPLES_FLAG) {

			// get samples from imu
			HAL_StatusTypeDef ret = imu_get_samples(&imu, 10);

			//send result over UART
			HAL_UART_Transmit_IT(&huart4, ret, 1);
		}
		else if (actual_flags & IMU_RESET_FLAG) {

			ULONG actual_events = 0;

			// reset imu
			imu_reset(&imu);

			// wait for reset to complete
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			if (actual_events & IMU_DATA_READY_FLAG) {
				HAL_UART_Transmit_IT(&huart4, HAL_OK, 1);
			}
			else {
				HAL_UART_Transmit_IT(&huart4, HAL_TIMEOUT, 1);
			}
		}
		*/

		// acquire first half of buffer
		tx_mutex_get(&imu_first_half_mutex, TX_WAIT_FOREVER);

		// fill first half of buffer
		imu_get_data(&imu, 0);

		// release mutex to allow sd thread to run
		tx_mutex_put(&imu_first_half_mutex);
		tx_event_flags_set(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG, TX_OR);

		// acquire second half of buffer
		tx_mutex_get(&imu_second_half_mutex, TX_WAIT_FOREVER);

		// fill second half of buffer
		imu_get_data(&imu, 1);

		// release mutex to allow sd thread to run
		tx_mutex_put(&imu_second_half_mutex);
		tx_event_flags_set(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG, TX_OR);

		// wait for stop thread flag
		tx_event_flags_get(&imu_event_flags_group, IMU_STOP_DATA_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);

		// check for stop thread flag
		if (actual_flags & IMU_STOP_DATA_THREAD_FLAG) {

			// suspend imu interrupt
			HAL_NVIC_DisableIRQ(EXTI12_IRQn);

			// signal sd thread to stop saving imu data and terminate thread
			tx_event_flags_set(&imu_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR);
			tx_thread_terminate(&threads[IMU_THREAD].thread);
		}
	}

}

void imu_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu) {

	ULONG actual_events = 0;

	// setup gpio pins
	imu->hspi = hspi;
	imu->cs_port = IMU_CS_GPIO_Port;
	imu->cs_pin = IMU_CS_Pin;
	imu->int_port = IMU_INT_GPIO_Port;
	imu->int_pin = IMU_INT_Pin;
	imu->wake_port = IMU_WAKE_GPIO_Port;
	imu->wake_pin = IMU_WAKE_Pin;
	imu->reset_port = IMU_RESET_GPIO_Port;
	imu->reset_pin = IMU_RESET_Pin;

	// reset imu
	imu_reset(imu);

	// wait for reset to complete
	tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

	// read startup data
	imu_configure_startup(imu);
}

void imu_reset(IMU_HandleTypeDef* imu) {

	// assert falling and rising edge on reset pin
	HAL_GPIO_WritePin(imu->reset_port, imu->reset_pin, GPIO_PIN_SET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(imu->reset_port, imu->reset_pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(imu->reset_port, imu->reset_pin, GPIO_PIN_SET);

}

void imu_configure_reports(IMU_HandleTypeDef* imu, uint8_t report_id) {

	uint8_t seq = 1;

	// create config frame
	IMU_SensorConfig config = {0};
	config.header.report_len_lsb = IMU_CONFIG_REPORT_LEN;
	config.header.channel = IMU_CONTROL_CHANNEL;
	config.header.seq = seq;
	config.feature_report_id = IMU_SET_FEATURE_REPORT_ID;
	config.report_id = report_id;
	config.report_interval_lsb = 0x10;
	config.report_interval_1 = 0x27;
	config.report_interval_2 = IMU_REPORT_INTERVAL_2;
	config.report_interval_msb = IMU_REPORT_INTERVAL_3;

	// transmit config frame
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(imu->hspi, (uint8_t *) &config, IMU_CONFIG_REPORT_LEN, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef imu_get_product_id(IMU_HandleTypeDef* imu) {

	uint8_t seq = 1;

	// create config frame
	IMU_ProductIDConfig config = {0};
	config.header.report_len_lsb = IMU_PRODUCT_CONFIG_REPORT_LEN;
	config.header.channel = IMU_CONTROL_CHANNEL;
	config.header.seq = seq;
	config.feature_report_id = IMU_GET_PRODUCT_REPORT_ID;

	// transmit config frame
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(imu->hspi, (uint8_t *) &config, IMU_PRODUCT_CONFIG_REPORT_LEN, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);
}

void imu_configure_startup(IMU_HandleTypeDef* imu) {

	ULONG actual_events = IMU_DATA_READY_FLAG;
	uint8_t read_startup = 0;
	uint16_t dataLen = 0;
	uint8_t receiveData[IMU_RECEIVE_BUFFER_MAX_LEN] = {0};

	while (read_startup < 2) {
		// read startup header
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_SPI_Receive(imu->hspi, &receiveData[0], IMU_SHTP_HEADER_LEN, IMU_SPI_STARTUP_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		// calculate data length
		dataLen = TO_16_BIT(receiveData[0], receiveData[1]);

		if (dataLen > IMU_RECEIVE_BUFFER_MAX_LEN) {
			dataLen = IMU_RECEIVE_BUFFER_MAX_LEN;
		}

		// get interrupt for data
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		// read startup data
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_SPI_Receive(imu->hspi, &receiveData[0], dataLen, IMU_SPI_STARTUP_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		// check interrupt for more startup data
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		// send config frame during last startup message
		if (read_startup == 1) {
			// configure all reports
			imu_configure_reports(imu, IMU_ROTATION_VECTOR_REPORT_ID);
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			imu_configure_reports(imu, IMU_ACCELEROMETER_REPORT_ID);
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			imu_configure_reports(imu, IMU_GYROSCOPE_REPORT_ID);
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			imu_configure_reports(imu, IMU_MAGNETOMETER_REPORT_ID);
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
		}
		read_startup++;
	}
}

HAL_StatusTypeDef imu_get_data(IMU_HandleTypeDef* imu, uint8_t buffer_half) {

	ULONG actual_events = 0;
	uint8_t receiveData[IMU_RECEIVE_BUFFER_MAX_LEN] = {0};

	for (uint16_t index=0; index<IMU_HALF_BUFFER_SIZE; index++) {
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		//Read the header in to buffer
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_StatusTypeDef ret = HAL_SPI_Receive(imu->hspi, &receiveData[0], IMU_SHTP_HEADER_LEN, IMU_SPI_READ_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		// extract data length from header
		uint16_t dataLen = TO_16_BIT(receiveData[0], receiveData[1]);

		if (ret == HAL_TIMEOUT || dataLen <= 0) {
			index--;

			continue;
		}
		else if (dataLen > IMU_RECEIVE_BUFFER_MAX_LEN) {
			dataLen = IMU_RECEIVE_BUFFER_MAX_LEN;
		}

		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		ret = HAL_SPI_Receive(imu->hspi, &receiveData[0], dataLen, IMU_SPI_READ_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		if (ret == HAL_TIMEOUT) {
			index--;

			continue;
		}

		if (debug) {
			HAL_UART_Transmit_IT(&huart4, &receiveData[0], dataLen);
		}

		if ((receiveData[2] == IMU_DATA_CHANNEL) && (receiveData[4] == IMU_TIMESTAMP_REPORT_ID)) {
			uint16_t parsedDataIndex = 9;
			while (parsedDataIndex < dataLen) {
				if ((receiveData[parsedDataIndex] == IMU_G_ROTATION_VECTOR_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_ROTATION_VECTOR_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_ACCELEROMETER_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_GYROSCOPE_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_MAGNETOMETER_REPORT_ID)) {

					// store report id
					imu_data[buffer_half][index].report_id = receiveData[parsedDataIndex];

					// store report data
					memcpy(imu_data[buffer_half][index].raw_data, &receiveData[parsedDataIndex+4], IMU_QUAT_USEFUL_BYTES);

					// increment index to next report
					parsedDataIndex += 4 + IMU_QUAT_USEFUL_BYTES;

					// check for buffer overflow
					if (index >= IMU_HALF_BUFFER_SIZE) {
						return HAL_OK;
					}

					if (parsedDataIndex < dataLen) {
						index++;
					}
				}
				else {
					index--;
					parsedDataIndex = dataLen;
				}
			}
		}
		else {
			index--;
			continue;
		}
	}

	return HAL_OK;
}


