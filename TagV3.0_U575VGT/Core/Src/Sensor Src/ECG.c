/*
 * ECG.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Kaveet
 */

#include "Sensor Inc/ECG.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/DataLogging.h"
#include "Lib Inc/threads.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include "ux_device_cdc_acm.h"
#include "app_filex.h"
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c4;
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
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

extern ECG_HandleTypeDef ecg;

//ThreadX useful variables (defined globally because they're shared with the SD card writing thread)
TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
TX_MUTEX ecg_first_half_mutex;
TX_MUTEX ecg_second_half_mutex;

//Array for holding ECG data. The buffer is split in half and shared with the ECG_SD thread.
ECG_Data ecg_data[2][ECG_HALF_BUFFER_SIZE] = {0};

// uart debugging
bool ecg_get_samples = false;
uint16_t ecg_sample_count = 0;
bool ecg_unit_test = false;
HAL_StatusTypeDef ecg_unit_test_ret = HAL_ERROR;

void ecg_thread_entry(ULONG thread_input){

	//Initialize ecg
	ecg_init(&hi2c4, &ecg);

	//Create the event flag
	tx_event_flags_create(&ecg_event_flags_group, "ECG Event Flags");
	tx_mutex_create(&ecg_first_half_mutex, "ECG First Half Mutex", TX_INHERIT);
	tx_mutex_create(&ecg_second_half_mutex, "ECG Second Half Mutex", TX_INHERIT);

	//Enable our interrupt handler (signals when data is ready)
	HAL_NVIC_EnableIRQ(EXTI14_IRQn);

	//Start data collection thread
	tx_thread_resume(&threads[DATA_LOG_THREAD].thread);

	while(1) {
		ULONG actual_flags = 0;
		HAL_StatusTypeDef ret = HAL_ERROR;

		// wait for any debugging flag
		tx_event_flags_get(&ecg_event_flags_group, ECG_UNIT_TEST_FLAG | ECG_READ_FLAG | ECG_WRITE_FLAG | ECG_CMD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		if (actual_flags & ECG_UNIT_TEST_FLAG) {
			ecg_unit_test = true;
			tx_event_flags_set(&ecg_event_flags_group, ECG_UNIT_TEST_DONE_FLAG, TX_OR);
		}
		else if (actual_flags & ECG_READ_FLAG) {

		}
		else if (actual_flags & ECG_WRITE_FLAG) {

		}
		else if (actual_flags & ECG_CMD_FLAG) {
			if (usbReceiveBuf[3] == ECG_GET_SAMPLES_CMD) {
				ecg_get_samples = true;
			}
		}

		//Acquire first half mutex to start collecting data
		tx_event_flags_get(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG | IMU_STOP_DATA_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);
		tx_event_flags_get(&depth_event_flags_group, DEPTH_HALF_BUFFER_FLAG | DEPTH_STOP_DATA_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);
		tx_mutex_get(&ecg_first_half_mutex, TX_WAIT_FOREVER);

		//Fill up the first half of the buffer (this function call fills up the IMU buffer on its own)
		ecg_get_data(&ecg, 0);

		//Release the first half mutex to signal the ECG_SD thread that it can start writing
		tx_mutex_put(&ecg_first_half_mutex);

		tx_event_flags_set(&ecg_event_flags_group, ECG_HALF_BUFFER_FLAG, TX_OR);
		tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

		//Acquire the second half mutex to fill it up
		tx_event_flags_get(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG | IMU_STOP_DATA_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);
		tx_event_flags_get(&depth_event_flags_group, DEPTH_HALF_BUFFER_FLAG | DEPTH_STOP_DATA_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);
		tx_mutex_get(&ecg_second_half_mutex, TX_WAIT_FOREVER);

		//Call to get data, this handles filling up the second half of the buffer completely
		ecg_get_data(&ecg, 1);

		//Release second half mutex so the ECG_SD thread can write to it
		tx_mutex_put(&ecg_second_half_mutex);

		tx_event_flags_set(&ecg_event_flags_group, ECG_HALF_BUFFER_FLAG, TX_OR);
		tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

		//Check to see if there was a stop thread request. Put very little wait time so its essentially an instant check
		tx_event_flags_get(&ecg_event_flags_group, ECG_STOP_DATA_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);

		//If there was something set cleanup the thread
		if (actual_flags & ECG_STOP_DATA_THREAD_FLAG){

			//Suspend our interrupt to stop new data from coming in
			HAL_NVIC_DisableIRQ(EXTI14_IRQn);

			//Signal SD card thread to stop, and terminate this thread
			tx_event_flags_set(&ecg_event_flags_group, ECG_STOP_SD_THREAD_FLAG, TX_OR);
			tx_thread_terminate(&threads[ECG_THREAD].thread);
		}
	}
}
HAL_StatusTypeDef ecg_init(I2C_HandleTypeDef* hi2c, ECG_HandleTypeDef* ecg){

	ecg->i2c_handler = hi2c;
	ecg->n_data_ready_port = ECG_NDRDY_GPIO_Port;
	ecg->n_data_ready_pin = ECG_NDRDY_Pin;

	//Reset the ADC
	ecg_reset_adc(ecg);

	//Configure the ecg adc
	ecg_write_configuration_register(ecg, ECG_ADC_DEFAULT_CONFIG_REGISTER);

	//Start the conversions
	uint8_t start_command = ECG_ADC_START;
	HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &start_command, 1, HAL_MAX_DELAY);

	return HAL_OK;
}

HAL_StatusTypeDef ecg_get_data(ECG_HandleTypeDef* ecg, uint8_t buffer_half) {

	ULONG actual_events;
	uint8_t ecg_error_count = 0;

	for (uint16_t index=0; index<ECG_HALF_BUFFER_SIZE; index++){

		//We've initialized the ECG to be in continuous conversion mode, and we have an interrupt line signaling when data is ready.
		//Thus, wait for our flag to be set in the interrupt handler. This function call will block the entire task until we receive the data.
		tx_event_flags_get(&ecg_event_flags_group, ECG_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, ECG_FLAG_TIMEOUT);

		HAL_StatusTypeDef ret = ecg_read_adc(ecg);

		if (ecg_unit_test) {
			ecg_unit_test_ret = ret;
			ecg_unit_test = false;
			tx_event_flags_set(&ecg_event_flags_group, ECG_UNIT_TEST_DONE_FLAG, TX_OR);
		}

		if (ecg_get_samples) {
			// send ecg data over uart and usb
			HAL_UART_Transmit_IT(&huart2, &ret, 1);
			HAL_UART_Transmit_IT(&huart2, (uint8_t *) &ecg->data, ECG_DATA_LEN);

			if (usbTransmitLen == 0) {
				memcpy(&usbTransmitBuf[0], &ret, 1);
				memcpy(&usbTransmitBuf[1], &ecg->data, ECG_DATA_LEN);
				usbTransmitLen = ECG_DATA_LEN + 1;
				tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
			}

			ecg_sample_count++;
			if (ecg_sample_count < ECG_NUM_SAMPLES) {
				ecg_get_samples = false;
				ecg_sample_count = 0;
			}
		}

		if (ret != HAL_OK) {
			index--;
			ecg_error_count++;

			if (ecg_error_count > ECG_MAX_ERROR_COUNT) {
				tx_event_flags_set(&ecg_event_flags_group, ECG_STOP_DATA_THREAD_FLAG, TX_OR);
				tx_event_flags_set(&ecg_event_flags_group, ECG_STOP_SD_THREAD_FLAG, TX_OR);
				tx_thread_suspend(&threads[ECG_THREAD].thread);
			}

			//Return to start of loop
			continue;
		}

		//Fill up buffer
		ecg_data[buffer_half][index] = ecg->data;

		//If we filled the buffer, back out now (prevent overflow or hardfault)
		if (index >= ECG_HALF_BUFFER_SIZE) {
			return HAL_OK;
		}
	}

	return HAL_OK;
}

HAL_StatusTypeDef ecg_read_adc(ECG_HandleTypeDef* ecg){

	//Issue read command
	uint8_t read_command = ECG_ADC_READ_DATA;
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &read_command, 1, HAL_MAX_DELAY);

	//return if failed
	if (ret != HAL_OK) {
		return ret;
	}

	//Read the data (no need to convert it, since we just write it to the buffer as raw data
	ret = HAL_I2C_Master_Receive(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), ecg->data.raw_data, ECG_DATA_LEN, HAL_MAX_DELAY);

	return ret;
}

HAL_StatusTypeDef ecg_write_configuration_register(ECG_HandleTypeDef* ecg, uint8_t data){

	uint8_t configure_command[2] = {0b01000000, data};

	return HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), configure_command, 2, HAL_MAX_DELAY);
}

HAL_StatusTypeDef ecg_read_configuration_register(ECG_HandleTypeDef* ecg, uint8_t * data){

	//Send read command
	uint8_t read_command = ECG_ADC_READ_CONFIG_REG;
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &read_command, 1, HAL_MAX_DELAY);

	//return if failed
	if (ret != HAL_OK) {
		return ret;
	}

	//Read the config register into the data buffer
	ret = HAL_I2C_Master_Receive(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), data, 1, HAL_MAX_DELAY);

	return ret;
}

HAL_StatusTypeDef ecg_configure_electrodes(ECG_HandleTypeDef* ecg, uint8_t electrode_config){

	//The ADC has 8 possible configurations for the Positive and negative channel. See datasheet for a list of them.
	//They are MUXED to allow for 3 bits to control the 8 combinations. These 3 bits are bits 7-5 in the config register.

	//Read the config register
	uint8_t config_register = 0;
	ecg_read_configuration_register(ecg, &config_register);

	//We want to only change the first 3 bits, so we should first unset them then set them appropriately.
	//This prevents the other settings from being changed
	//
	//Unset the first 3 bits
	config_register &= ~(111 << 5);

	//Now set them with our passed in values
	config_register |= (electrode_config << 5);

	//Now, call the function to write to the config register with our new values
	return ecg_write_configuration_register(ecg, config_register);
}

HAL_StatusTypeDef ecg_poll_data_ready(ECG_HandleTypeDef* ecg){

	uint32_t startTime = HAL_GetTick();

	//Poll for a falling edge (data ready is low)
	while (HAL_GPIO_ReadPin(ecg->n_data_ready_port, ecg->n_data_ready_pin)){

		//Track current time and check if we timeout
		uint32_t currentTime = HAL_GetTick();

		if ((currentTime - startTime) > ECG_ADC_DATA_TIMEOUT){
			return HAL_TIMEOUT;
		}
	}

	return HAL_OK;
}

HAL_StatusTypeDef ecg_reset_adc(ECG_HandleTypeDef* ecg){

	uint8_t reset_command = ECG_ADC_RESET;

	return HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &reset_command, 1, HAL_MAX_DELAY);
}
