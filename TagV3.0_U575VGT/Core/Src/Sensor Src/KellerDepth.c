/*
 * KellerDepth.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 */

#include "Sensor Inc/KellerDepth.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/LightSensor.h"
#include "Sensor Inc/DataLogging.h"
#include "Lib Inc/threads.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include "ux_device_cdc_acm.h"
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart2;

extern Thread_HandleTypeDef threads[NUM_THREADS];

// usb flags
extern TX_EVENT_FLAGS_GROUP usb_cdc_event_flags_group;

// usb buffers
extern uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitLen;

// sensor event flags
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP light_event_flags_group;
extern TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

//ThreadX useful variables (defined globally because they're shared with the SD card writing thread)
TX_EVENT_FLAGS_GROUP depth_event_flags_group;
TX_MUTEX depth_first_half_mutex;
TX_MUTEX depth_second_half_mutex;

//Array for holding IMU data. The buffer is split in half and shared with the IMU thread.
DEPTH_Data depth_data[2][DEPTH_HALF_BUFFER_SIZE] = {0};

// uart debugging
bool depth_get_samples = false;
uint16_t depth_sample_count = 0;
bool depth_unit_test = false;
HAL_StatusTypeDef depth_unit_test_ret = HAL_ERROR;

void depth_thread_entry(ULONG thread_input) {

	//Declare Keller handler and initialize chip
	Keller_HandleTypedef depth;
	depth_init(&hi2c2, &depth);

	//Create the event flag
	tx_event_flags_create(&depth_event_flags_group, "DEPTH Event Flags");
	tx_mutex_create(&depth_first_half_mutex, "DEPTH First Half Mutex", TX_INHERIT);
	tx_mutex_create(&depth_second_half_mutex, "DEPTH Second Half Mutex", TX_INHERIT);

	//Start ECG sensor thread
	tx_thread_resume(&threads[ECG_THREAD].thread);

	while (1) {
		ULONG actual_flags = 0;

		// wait for any debugging flag
		tx_event_flags_get(&depth_event_flags_group, DEPTH_UNIT_TEST_FLAG | DEPTH_CMD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		if (actual_flags & DEPTH_UNIT_TEST_FLAG) {
			depth_unit_test = true;
		}
		else if (actual_flags & DEPTH_CMD_FLAG) {
			if (usbReceiveBuf[3] == DEPTH_GET_SAMPLES_CMD) {
				depth_get_samples = true;
			}
		}

		//Wait for first half of the buffer to be ready for data
		tx_event_flags_get(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG | IMU_STOP_DATA_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);
		tx_mutex_get(&depth_first_half_mutex, TX_WAIT_FOREVER);

		//Fill up the first half of the buffer (this function call fills up the IMU buffer on its own)
		depth_get_data(&depth, 0);
		tx_event_flags_set(&depth_event_flags_group, DEPTH_COMPLETE_FLAG, TX_OR);

		//Release the mutex to allow for SD card writing thread to run
		tx_mutex_put(&depth_first_half_mutex);

		tx_event_flags_set(&depth_event_flags_group, DEPTH_HALF_BUFFER_FLAG, TX_OR);
		tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

		//Acquire second half (so we can fill it up)
		tx_event_flags_get(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG | IMU_STOP_DATA_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);
		tx_mutex_get(&depth_second_half_mutex, TX_WAIT_FOREVER);

		//Call to get data, this handles filling up the second half of the buffer completely
		depth_get_data(&depth, 1);
		tx_event_flags_set(&depth_event_flags_group, DEPTH_COMPLETE_FLAG, TX_OR);

		//Release mutex
		tx_mutex_put(&depth_second_half_mutex);

		tx_event_flags_set(&depth_event_flags_group, DEPTH_HALF_BUFFER_FLAG, TX_OR);
		tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

		//Check to see if there was a stop flag raised
		tx_event_flags_get(&depth_event_flags_group, DEPTH_STOP_DATA_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);

		//If there was something set cleanup the thread
		if (actual_flags & DEPTH_STOP_DATA_THREAD_FLAG){

			//Signal SD card thread to stop, and terminate this thread
			tx_event_flags_set(&depth_event_flags_group, DEPTH_STOP_SD_THREAD_FLAG, TX_OR);
			tx_thread_terminate(&threads[DEPTH_THREAD].thread);
		}
	}
}

void depth_init(I2C_HandleTypeDef *hi2c_device, Keller_HandleTypedef *keller_sensor) {
	keller_sensor->i2c_handler = hi2c_device;
}

HAL_StatusTypeDef depth_get_data(Keller_HandleTypedef* keller_sensor, uint8_t buffer_half) {

	ULONG actual_flags = 0;
	uint8_t data_buf[DEPTH_RECEIVE_BUFFER_MAX_LEN] = {0};
	uint8_t depth_error_count = 0;

	tx_event_flags_get(&light_event_flags_group, LIGHT_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

	for (uint16_t index=0; index<DEPTH_HALF_BUFFER_SIZE; index++) {

		// request depth data
		data_buf[0] = DEPTH_REQ_DATA_ADDR;
		HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(keller_sensor->i2c_handler, DEPTH_ADDR << 1, &data_buf[0], 1, DEPTH_TIMEOUT);

		if (depth_unit_test) {
			depth_unit_test_ret = ret;
			depth_unit_test = false;
			tx_event_flags_set(&depth_event_flags_group, DEPTH_UNIT_TEST_DONE_FLAG, TX_OR);
		}

		if (ret != HAL_OK) {
			index--;
			depth_error_count++;

			if (depth_error_count > DEPTH_MAX_ERROR_COUNT) {
				// stop collecting data instead of terminating thread
				//memset(depth_data[buffer_half], 0, sizeof(depth_data[buffer_half]));
				break;
			}

			//Return to start of loop
			continue;
		}

		// await end of conversion (wait 8ms or check busy flag in status byte or wait for EOC to be high)
		HAL_Delay(DEPTH_BUSY_WAIT_TIME_MS);
		tx_event_flags_get(&depth_event_flags_group, DEPTH_DATA_READY_FLAG, TX_OR_CLEAR, &actual_flags, DEPTH_BUSY_WAIT_TIME_MS);

		ret = HAL_I2C_Master_Receive(keller_sensor->i2c_handler, DEPTH_ADDR << 1, &data_buf[0], DEPTH_DATA_LEN, DEPTH_TIMEOUT);

		if (depth_get_samples) {
			// send depth data over uart and usb
			HAL_UART_Transmit_IT(&huart2, &ret, 1);
			HAL_UART_Transmit_IT(&huart2, &data_buf[0], DEPTH_DATA_LEN);

			if (usbTransmitLen == 0) {
				memcpy(&usbTransmitBuf[0], &ret, 1);
				memcpy(&usbTransmitBuf[1], &data_buf[0], DEPTH_DATA_LEN);
				usbTransmitLen = DEPTH_DATA_LEN + 1;
				tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
			}

			depth_sample_count++;
			if (depth_sample_count < DEPTH_NUM_SAMPLES) {
				depth_get_samples = false;
				depth_sample_count = 0;
			}
		}

		if (ret != HAL_OK) {
			index--;
			depth_error_count++;

			if (depth_error_count > DEPTH_MAX_ERROR_COUNT) {
				tx_event_flags_set(&depth_event_flags_group, DEPTH_STOP_DATA_THREAD_FLAG, TX_OR);
				tx_event_flags_set(&depth_event_flags_group, DEPTH_STOP_SD_THREAD_FLAG, TX_OR);
				tx_thread_suspend(&threads[DEPTH_THREAD].thread);
			}

			//Return to start of loop
			continue;
		}

		depth_data[buffer_half][index].status = data_buf[0];
		depth_data[buffer_half][index].raw_pressure = ((uint16_t) data_buf[1] << 8) | data_buf[2];
		depth_data[buffer_half][index].raw_temp = ((uint16_t) data_buf[3] << 8) | data_buf[4];

		// check index to prevent buffer overflow
		if (index >= IMU_HALF_BUFFER_SIZE){
			return HAL_OK;
		}
	}

	return HAL_OK;
}
