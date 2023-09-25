/*
 * KellerDepth.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 */

#include "KellerDepth.h"
#include "main.h"
#include "BNO08x.h"
#include "fx_api.h"
#include "app_filex.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include "stm32u5xx_hal_gpio.h"
#include "stm32u5xx_hal_spi.h"
#include <stdbool.h>
#include "Lib Inc/threads.h"
#include "Sensor Inc/GpsGeofencing.h"
#include "Sensor Inc/RTC.h"

extern I2C_HandleTypeDef hi2c2;

//ThreadX useful variables (defined globally because they're shared with the SD card writing thread)
TX_EVENT_FLAGS_GROUP depth_event_flags_group;
TX_MUTEX depth_first_half_mutex;
TX_MUTEX depth_second_half_mutex;

//Array for holding IMU data. The buffer is split in half and shared with the IMU thread.
DEPTH_Data depth_data[2][IMU_HALF_BUFFER_SIZE] = {0};

void keller_thread_entry(ULONG thread_input) {

	//Create Keller handler and initialize
	Keller_HandleTypedef depth;

	keller_init(&hi2c2, &depth);

	//Create the event flag
	tx_event_flags_create(&depth_event_flags_group, "DEPTH Event Flags");
	tx_mutex_create(&depth_first_half_mutex, "DEPTH First Half Mutex", TX_INHERIT);
	tx_mutex_create(&depth_second_half_mutex, "DEPTH Second Half Mutex", TX_INHERIT);

	//Allow the SD card writing thread to start
	tx_thread_resume(&threads[IMU_SD_THREAD].thread);

	while (1) {
		//Wait for first half of the buffer to be ready for data
		tx_mutex_get(&depth_first_half_mutex, TX_WAIT_FOREVER);

		//Fill up the first half of the buffer (this function call fills up the IMU buffer on its own)
		keller_get_data(&depth, 0);

		//Release the mutex to allow for SD card writing thread to run
		tx_mutex_put(&depth_first_half_mutex);

		//Acquire second half (so we can fill it up)
		tx_mutex_get(&depth_second_half_mutex, TX_WAIT_FOREVER);

		//Call to get data, this handles filling up the second half of the buffer completely
		keller_get_data(&depth, 1);

		//Release mutex
		tx_mutex_put(&depth_second_half_mutex);

		//Check to see if there was a stop flag raised
		ULONG actual_flags;
		tx_event_flags_get(&depth_event_flags_group, DEPTH_STOP_DATA_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);

		//If there was something set cleanup the thread
		if (actual_flags & ECG_STOP_DATA_THREAD_FLAG){

			//Signal SD card thread to stop, and terminate this thread
			tx_event_flags_set(&depth_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR);
			tx_thread_terminate(&threads[DEPTH_THREAD].thread);
		}
	}
}

void keller_init(Keller_HandleTypedef *keller_sensor, I2C_HandleTypeDef *hi2c_device) {
	keller_sensor->i2c_handler = hi2c_device;
}

HAL_StatusTypeDef keller_get_data(Keller_HandleTypedef* keller_sensor, uint8_t buffer_half) {

	//Receive data buffer
	uint8_t receiveData[256] = {0};

	//Index to keep track of number of samples per frame
	uint8_t sample_index = 0;

	for (uint16_t index = 0; index < DEPTH_HALF_BUFFER_SIZE; index++) {

		ULONG actual_events;

		//Wait for data to be ready (suspends so other threads can run)
		tx_event_flags_get(&depth_event_flags_group, DEPTH_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

		uint8_t data_buf[1] = {KELLER_REQ};

		HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(keller_sensor->i2c_handler, 0x40 << 1, data_buf, 1, 100);

		//If there is some issue, ignore the data
		if (ret != HAL_OK) {
			//Decrement index so we dont fill this part of the array
			index--;

			//Return to start of loop
			continue;
		}

		// Wait >=8ms or wait for EOC to go high (VDD) or check status byte for the busy flag
		// The easy solution is to wait for 8ms>, the faster solution is to read the Busy flag or (if MCU pin available) EOC pin
		HAL_Delay(8);

		ret = HAL_I2C_Master_Receive(keller_sensor->i2c_handler, KELLER_ADDR << 1, receiveData, KELLER_DLEN, 100);

		//If there is some issue, ignore the data
		if (ret != HAL_OK) {
			//Decrement index so we dont fill this part of the array
			index--;

			//Return to start of loop
			continue;
		}

		depth_data[buffer_half][index].status = receiveData[0];
		depth_data[buffer_half][index].raw_pressure = ((uint16_t) receiveData[1] << 8) | receiveData[2];
		depth_data[buffer_half][index].raw_temp = ((uint16_t) receiveData[3] << 8) | receiveData[4];

		depth_data[buffer_half][index].temperature = TO_DEGC(depth_data[buffer_half][index].raw_temp);
		depth_data[buffer_half][index].pressure = TO_BAR(depth_data[buffer_half][index].raw_pressure);

		//Increment sample count for data header, reset after max number of samples per frame reached
		if (sample_index < SAMPLES_PER_FRAME-1) {
			sample_index++;
		}
		else {
			sample_index = 0;
		}

		//If we filled the buffer, back out now (prevent overflow or hardfault)
		if (index >= IMU_HALF_BUFFER_SIZE){
			return HAL_OK;
		}
	}

	return HAL_OK;
}

