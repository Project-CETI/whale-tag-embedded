/*
 * IMU.c
 *
 *  Created on: Nov 16, 2023
 *      Author: kevinma
 */

#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/audio.h"
#include "Lib Inc/threads.h"
#include "ux_device_cdc_acm.h"
#include "fx_api.h"
#include "app_filex.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include "stm32u5xx_hal_gpio.h"
#include "stm32u5xx_hal_spi.h"
#include <stdbool.h>

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;

extern Thread_HandleTypeDef threads[NUM_THREADS];

// usb flags
extern TX_EVENT_FLAGS_GROUP usb_cdc_event_flags_group;

// usb buffers
extern uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitLen;

extern IMU_HandleTypeDef imu;

// threadX mutex and flags
TX_EVENT_FLAGS_GROUP imu_event_flags_group;
TX_MUTEX imu_first_half_mutex;
TX_MUTEX imu_second_half_mutex;

// array split in half for holding IMU data
IMU_Data imu_data[2][IMU_HALF_BUFFER_SIZE] = {0};

// uart debug status
bool imu_get_samples = false;
uint16_t imu_sample_count = 0;
bool imu_unit_test = false;
HAL_StatusTypeDef imu_unit_test_ret = HAL_ERROR;

uint8_t debug = 1;

void imu_thread_entry(ULONG thread_input) {

	//Create the events flag.
	tx_event_flags_create(&imu_event_flags_group, "IMU Event Flags");
	tx_mutex_create(&imu_first_half_mutex, "IMU First Half Mutex", TX_INHERIT);
	tx_mutex_create(&imu_second_half_mutex, "IMU Second Half Mutex", TX_INHERIT);

	//Enable our interrupt handler that signals data is ready
	HAL_NVIC_EnableIRQ(EXTI12_IRQn);

	imu_init(&hspi1, &imu);

	tx_thread_resume(&threads[DEPTH_THREAD].thread);

	while (1) {
		ULONG actual_flags = 0;
		HAL_StatusTypeDef ret = HAL_ERROR;

		// wait for any debugging flag
		tx_event_flags_get(&imu_event_flags_group, IMU_UNIT_TEST_FLAG | IMU_CMD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		if (actual_flags & IMU_UNIT_TEST_FLAG) {
			imu_unit_test = true;
		}
		else if (actual_flags & IMU_CMD_FLAG) {
			ULONG actual_events = 0;

			if (usbReceiveBuf[3] == IMU_GET_SAMPLES_CMD) {

				// continue normal data collection but also transmit over uart and usb
				imu_get_samples = true;
			}
			else if (usbReceiveBuf[3] == IMU_RESET_CMD) {

				// reset imu
				imu_reset(&imu);

				// wait for reset to complete
				tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
				if (actual_events & IMU_DATA_READY_FLAG) {
					ret = HAL_OK;
				}
				else {
					ret = HAL_TIMEOUT;
				}

				// send result over uart and usb
				HAL_UART_Transmit_IT(&huart2, &ret, 1);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					usbTransmitLen = 1;
					tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
				}
			}
			else if (usbReceiveBuf[3] == IMU_CONFIG_CMD) {

				// enable new report id
				tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
				if (actual_events & IMU_DATA_READY_FLAG) {
					ret = imu_configure_reports(&imu, usbReceiveBuf[4]);
				}
				else {
					ret = HAL_TIMEOUT;
				}
				// send result over uart and usb
				HAL_UART_Transmit_IT(&huart2, &ret, 1);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					usbTransmitLen = 1;
					tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
				}
			}
		}

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

HAL_StatusTypeDef imu_configure_reports(IMU_HandleTypeDef* imu, uint8_t report_id) {

	uint8_t seq = 1;

	// create config frame
	IMU_SensorConfig config = {0};
	config.header.report_len_lsb = IMU_CONFIG_REPORT_LEN;
	config.header.channel = IMU_CONTROL_CHANNEL;
	config.header.seq = seq;
	config.feature_report_id = IMU_SET_FEATURE_REPORT_ID;
	config.report_id = report_id;
	config.report_interval_lsb = 0x10;//IMU_REPORT_INTERVAL_0;
	config.report_interval_1 = 0x27;//IMU_REPORT_INTERVAL_1;
	config.report_interval_2 = IMU_REPORT_INTERVAL_2;
	config.report_interval_msb = IMU_REPORT_INTERVAL_3;

	// transmit config frame
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_StatusTypeDef ret = HAL_SPI_Transmit(imu->hspi, (uint8_t *) &config, IMU_CONFIG_REPORT_LEN, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	return ret;
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
	HAL_StatusTypeDef ret = HAL_SPI_Transmit(imu->hspi, (uint8_t *) &config, IMU_PRODUCT_CONFIG_REPORT_LEN, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	return ret;
}

void imu_configure_startup(IMU_HandleTypeDef* imu) {

	ULONG actual_events = 0;
	uint8_t read_startup = 0;
	uint16_t dataLen = 0;
	uint8_t receiveData[IMU_RECEIVE_BUFFER_MAX_LEN] = {0};

	while (read_startup < 3) {
		// read startup header
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_SPI_Receive(imu->hspi, &receiveData[0], IMU_SHTP_HEADER_LEN, IMU_SPI_STARTUP_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		//HAL_UART_Transmit(&huart2, &receiveData[0], IMU_SHTP_HEADER_LEN, HAL_MAX_DELAY);

		// calculate data length
		dataLen = TO_16_BIT(receiveData[0], receiveData[1]);

		if (dataLen > IMU_RECEIVE_BUFFER_MAX_LEN) {
			dataLen = IMU_RECEIVE_BUFFER_MAX_LEN;
		}

		// get interrupt for data
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		// reset first element for checking report id
		receiveData[0] = 0;

		// read startup data and keep requesting data until valid report id received
		while (receiveData[0] == 0 && dataLen != 0) {
			HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
			HAL_SPI_Receive(imu->hspi, &receiveData[0], dataLen, IMU_SPI_STARTUP_TIMEOUT_MS);
			HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

			//HAL_UART_Transmit(&huart2, &receiveData[0], dataLen, HAL_MAX_DELAY);

			// check interrupt for more startup data
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
		}

		// send config frame during last startup message
		if (read_startup == 1 && dataLen != 0 && receiveData[0] != 0) {
			// configure all reports
			imu_configure_reports(imu, IMU_ROTATION_VECTOR_REPORT_ID);
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			//imu_configure_reports(imu, IMU_ACCELEROMETER_REPORT_ID);
			//tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			//imu_configure_reports(imu, IMU_GYROSCOPE_REPORT_ID);
			//tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);
			//imu_configure_reports(imu, IMU_MAGNETOMETER_REPORT_ID);
			//tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		}

		if (dataLen != 0 && receiveData[0] != 0) {
			read_startup++;
			receiveData[0] = 0;
		}
	}
}

HAL_StatusTypeDef imu_get_data(IMU_HandleTypeDef* imu, uint8_t buffer_half) {

	ULONG actual_events = 0;
	uint8_t receiveData[IMU_RECEIVE_BUFFER_MAX_LEN] = {0};
	uint8_t imu_error_count = 0;

	for (uint16_t index=0; index<IMU_HALF_BUFFER_SIZE; index++) {
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		//Read the header in to buffer
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_StatusTypeDef ret = HAL_SPI_Receive(imu->hspi, &receiveData[0], IMU_SHTP_HEADER_LEN, IMU_SPI_READ_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		// extract data length from header
		uint16_t dataLen = TO_16_BIT(receiveData[0], receiveData[1]);

		if (imu_unit_test) {
			imu_unit_test_ret = ret;
			imu_unit_test = false;
			tx_event_flags_set(&imu_event_flags_group, IMU_UNIT_TEST_DONE_FLAG, TX_OR);
		}

		if (imu_get_samples) {
			HAL_UART_Transmit_IT(&huart2, &ret, 1);
			HAL_UART_Transmit_IT(&huart2, (uint8_t *) &receiveData[0], IMU_SHTP_HEADER_LEN);

			if (usbTransmitLen == 0) {
				memcpy(&usbTransmitBuf[0], &ret, 1);
				memcpy(&usbTransmitBuf[1], &receiveData[0], IMU_SHTP_HEADER_LEN);
				usbTransmitLen = IMU_SHTP_HEADER_LEN + 1;
				tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
			}
		}

		if (ret == HAL_TIMEOUT || dataLen <= 0) {
			index--;
			imu_error_count++;

			// suspend imu if too many errors
			if (imu_error_count > IMU_MAX_ERROR_COUNT) {
				tx_event_flags_set(&imu_event_flags_group, IMU_STOP_DATA_THREAD_FLAG, TX_OR);
				tx_event_flags_set(&imu_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR);
				tx_thread_suspend(&threads[IMU_THREAD].thread);
				break;
			}
			continue;
		}

		else if (dataLen > IMU_RECEIVE_BUFFER_MAX_LEN) {
			dataLen = IMU_RECEIVE_BUFFER_MAX_LEN;
		}

		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, IMU_FLAG_WAIT_TIMEOUT);

		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		ret = HAL_SPI_Receive(imu->hspi, &receiveData[0], dataLen, IMU_SPI_READ_TIMEOUT_MS);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		if (ret == HAL_TIMEOUT) {
			index--;
			imu_error_count++;

			// suspend imu if too many errors
			if (imu_error_count > IMU_MAX_ERROR_COUNT) {
				tx_event_flags_set(&imu_event_flags_group, IMU_STOP_DATA_THREAD_FLAG, TX_OR);
				tx_event_flags_set(&imu_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR);
				tx_thread_suspend(&threads[IMU_THREAD].thread);
				break;
			}
			continue;
		}

		if (imu_get_samples) {
			// send imu data over uart and usb
			HAL_UART_Transmit_IT(&huart2, &receiveData[0], dataLen);

			if (usbTransmitLen == 0) {
				memcpy(&usbTransmitBuf[0], &receiveData[0], dataLen);
				usbTransmitLen = dataLen;
				tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
			}

			imu_sample_count++;
			if (imu_sample_count < IMU_NUM_SAMPLES) {
				imu_get_samples = false;
				imu_sample_count = 0;
			}
		}

		//HAL_UART_Transmit(&huart2, &receiveData[0], dataLen+IMU_SHTP_HEADER_LEN, HAL_MAX_DELAY);

		if ((receiveData[2] == IMU_DATA_CHANNEL) && (receiveData[4] == IMU_TIMESTAMP_REPORT_ID)) {
			uint16_t parsedDataIndex = 9;
			while (parsedDataIndex < dataLen) {
				if ((receiveData[parsedDataIndex] == IMU_G_ROTATION_VECTOR_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_ROTATION_VECTOR_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_ACCELEROMETER_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_GYROSCOPE_REPORT_ID) || (receiveData[parsedDataIndex] == IMU_MAGNETOMETER_REPORT_ID)) {

					uint8_t bytesLen = IMU_REPORT_HEADER_LEN + ((receiveData[parsedDataIndex] == IMU_G_ROTATION_VECTOR_REPORT_ID) ? IMU_Q_QUAT_USEFUL_BYTES : (receiveData[parsedDataIndex] == IMU_ROTATION_VECTOR_REPORT_ID) ? IMU_QUAT_USEFUL_BYTES : IMU_3_AXIS_USEFUL_BYTES);

					// store report id
					imu_data[buffer_half][index].report_id = receiveData[parsedDataIndex];

					// store report data
					memcpy(imu_data[buffer_half][index].raw_data, &receiveData[parsedDataIndex+4], bytesLen);

					// add padding to data buffer
					if (bytesLen < IMU_QUAT_USEFUL_BYTES) {
						imu_data[buffer_half][index].raw_data[6] = 0;
						imu_data[buffer_half][index].raw_data[7] = 0;
						imu_data[buffer_half][index].raw_data[8] = 0;
						imu_data[buffer_half][index].raw_data[9] = 0;
					}

					// increment index to next report
					parsedDataIndex += bytesLen;

					if (parsedDataIndex < dataLen) {
						index++;
					}

					// check for buffer overflow
					if (index >= IMU_HALF_BUFFER_SIZE) {
						return HAL_OK;
					}
				}
				// skip over rest of data if not correct report id
				else {
					index--;
					imu_error_count++;

					// suspend imu if too many errors
					if (imu_error_count > IMU_MAX_ERROR_COUNT) {
						tx_event_flags_set(&imu_event_flags_group, IMU_STOP_DATA_THREAD_FLAG, TX_OR);
						tx_event_flags_set(&imu_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR);
						tx_thread_suspend(&threads[IMU_THREAD].thread);
						break;
					}
					parsedDataIndex = dataLen;
				}
			}
		}
		else {
			index--;
			imu_error_count++;

			// suspend imu if too many errors
			if (imu_error_count > IMU_MAX_ERROR_COUNT) {
				tx_event_flags_set(&imu_event_flags_group, IMU_STOP_DATA_THREAD_FLAG, TX_OR);
				tx_event_flags_set(&imu_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR);
				tx_thread_suspend(&threads[IMU_THREAD].thread);
				break;
			}
			continue;
		}
	}

	return HAL_OK;
}


