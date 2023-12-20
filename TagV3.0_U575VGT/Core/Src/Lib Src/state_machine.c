/*
 * state_machine.c
 *
 *  Created on: Jul 24, 2023
 *      Author: Kaveet
 */

#include "Lib Inc/state_machine.h"
#include "Sensor Inc/audio.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/KellerDepth.h"
#include "Sensor Inc/ECG.h"
#include "Sensor Inc/BMS.h"
#include "Lib Inc/threads.h"
#include "app_usbx_device.h"
#include "ux_device_cdc_acm.h"
#include "main.h"
#include "app_filex.h"
#include <stdbool.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;

//External event flags
extern TX_EVENT_FLAGS_GROUP usb_cdc_event_flags_group;
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP usb_event_flags_group;
extern TX_EVENT_FLAGS_GROUP bms_event_flags_group;

extern uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitBuf[APP_RX_DATA_SIZE];
extern uint16_t usbTransmitLen;

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

//FileX variables
FX_FILE config_file = {};
extern FX_MEDIA sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//BMS handler
extern MAX17320_HandleTypeDef bms;

//USB status variable
extern bool inserted;

//Event flags for signaling changes in state
TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

//Status variable to indicate thread is writing to SD card
bool config_writing = false;

//State variable
State state = STATE_DATA_CAPTURE;
bool sleep_state[10] = {0};

//State machine flags
ULONG actual_state_flags;

//Callback for completed SD card write
void ConfigWriteComplete_Callback(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	config_writing = false;
}

void state_machine_thread_entry(ULONG thread_input){

	//Create event flags for triggering state changes
	tx_event_flags_create(&state_machine_event_flags_group, "State Machine Event Flags");

	//Create config file if not exist
	UINT fx_result = FX_SUCCESS;
	char *file_name = "config.bin";

	//Create config file
	fx_result = fx_file_create(&sdio_disk, file_name);
	if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
		Error_Handler();
	}

	//Initialize config file if file creation successful
	if (fx_result == FX_SUCCESS) {

		//Open config file
		fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_WRITE);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		//Ensure sleep state is zero (wake)
		sleep_state[0] = false;

		//Setup "write complete" callback function
		fx_result = fx_file_write_notify_set(&config_file, ConfigWriteComplete_Callback);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		//Tell BMS thread to write nonvolatile memory settings
		tx_event_flags_set(&bms_event_flags_group, BMS_NV_WRITE_FLAG, TX_OR);

		//Wait for operation to finish
		tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);
		if (actual_state_flags & BMS_OP_ERROR_FLAG) {
			Error_Handler();
		}

		//Write initial configuration to config file
		config_writing = true;
		fx_file_write(&config_file, &sleep_state[0], sizeof(sleep_state));
		while (config_writing);
	}
	//Read config file if already created and not simulating (debugging)
	else if ((fx_result == FX_ALREADY_CREATED) && (!IS_SIMULATING_DEBUG)) {

		//Declare tag is sleeping as default
		state = STATE_SLEEP;

		//Read config file to check if tag returning from sleep mode
		fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_READ);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		ULONG actual_bytes;
		fx_result = fx_file_read(&config_file, &sleep_state[0], 2, &actual_bytes);

		//Enter data capture if read error or if current state is not sleep state
		if ((fx_result != FX_SUCCESS) || (actual_bytes < 1) || (sleep_state[0] == 0)) {
			state = STATE_DATA_CAPTURE;
			enter_data_capture();
		}
	}

	//Check if simulating (debugging)
	if (IS_SIMULATING_DEBUG) {
		//Set the simulation state defined in the header file
		state = SIMULATING_STATE;

		//Check the initial state and start in the appropriate state
		if (state == STATE_DATA_CAPTURE) {
			enter_data_capture();
		}
		else if (state == STATE_RECOVERY) {
			enter_recovery();
		}
		else if (state == STATE_DATA_OFFLOAD) {
			enter_data_offload();
		}
	}

	//Wait for exit sleep command to return to main loop
	while (state == STATE_SLEEP) {

		// shut off power from battery (initialization automatically starts discharge)
		tx_event_flags_set(&bms_event_flags_group, BMS_CLOSE_MOSFET_FLAG, TX_OR);
		tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);

		ULONG actual_state_flags = 0;
		if (inserted) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_USB_MSB_ACTIVATED_FLAG, TX_OR);
			inserted = false;
		}
		tx_event_flags_get(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG | STATE_USB_MSB_ACTIVATED_FLAG | STATE_USB_MSB_DEACTIVATED_FLAG, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);

		//BMS exit sleep mode flag
		if (actual_state_flags & STATE_EXIT_SLEEP_MODE_FLAG) {

			//Tell BMS thread to start discharging from battery to power on tag
			tx_event_flags_set(&bms_event_flags_group, BMS_START_DISCHARGE_FLAG, TX_OR);
			tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);

			//Exit data offload
			exit_data_offload();

			//Enter data capture
			enter_data_capture();
			state = STATE_DATA_CAPTURE;

			//Open config file to update sleep state
			fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_WRITE);
			if (fx_result != FX_SUCCESS) {
				Error_Handler();
			}

			//Setup "write complete" callback function
			fx_result = fx_file_write_notify_set(&config_file, ConfigWriteComplete_Callback);
			if (fx_result != FX_SUCCESS) {
				Error_Handler();
			}

			//Update sleep state
			sleep_state[0] = false;

			//Go to beginning of file
			fx_result = fx_file_seek(&config_file, 0);
			if (fx_result != FX_SUCCESS) {
				Error_Handler();
			}

			//Write to config file
			config_writing = true;
			fx_file_write(&config_file, &sleep_state[0], sizeof(sleep_state[0]));
			while (config_writing);

			//Send status over uart and usb
			HAL_UART_Transmit_IT(&huart2, (uint8_t *) &fx_result, 1);

			if (usbTransmitLen == 0) {
				memcpy(&usbTransmitBuf[0], (uint8_t *) &fx_result, 1);
				usbTransmitLen = 1;
				tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
			}
		}

		//USB connected flag
		if (actual_state_flags & STATE_USB_MSB_ACTIVATED_FLAG) {

			//In USB MSB mode (turn off all threads except vital threads)
			if (!sleep_state[1]) {
				//Stop data capture and recovery threads
				if (state == STATE_DATA_CAPTURE) {
					soft_exit_data_capture();
				}
				else if (state == STATE_RECOVERY) {
					exit_recovery();
				}
				enter_data_offload();
			}
			//In USB CDC mode (turn on data capture threads for debugging)
			else {
				if (state == STATE_RECOVERY) {
					exit_recovery();
				}
				enter_data_capture();
			}
		}

		//USB disconnected flag
		if (actual_state_flags & STATE_USB_MSB_DEACTIVATED_FLAG) {
			exit_data_offload();
			enter_data_capture();
		}
	}

	//Enter main thread execution loop ONLY if we arent simulating
	if (!IS_SIMULATING_DEBUG) {
		while (1){

			ULONG actual_state_flags = 0;
			if (inserted) {
				tx_event_flags_set(&state_machine_event_flags_group, STATE_USB_MSB_ACTIVATED_FLAG, TX_OR);
				inserted = false;
			}
			tx_event_flags_get(&state_machine_event_flags_group, ALL_STATE_FLAGS, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);

			//RTC timeout flag or GPS geofencing flag (outside of Dominica)
			if ((actual_state_flags & STATE_TIMEOUT_FLAG) || (actual_state_flags & STATE_GPS_FENCE_FLAG)) {

				//Exit data capture and enter recovery
				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
					enter_recovery();
					state = STATE_RECOVERY;
				}
			}

			//BMS low battery flag or sd card full flag
			if ((actual_state_flags & STATE_LOW_BATT_FLAG) || (actual_state_flags & STATE_SD_CARD_FULL_FLAG)) {

				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
				}

				enter_recovery();
				state = STATE_RECOVERY;
			}

			//BMS critical battery flag
			if (actual_state_flags & STATE_CRIT_BATT_FLAG) {

				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
				}
				else if (state == STATE_RECOVERY) {
					exit_recovery();
				}

				state = STATE_FISHTRACKER;

				//Tell BMS thread to close BMS mosfets to power off tag
				tx_event_flags_set(&bms_event_flags_group, BMS_CLOSE_MOSFET_FLAG, TX_OR);
				tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);
			}

			//BMS sleep mode flag
			if (actual_state_flags & STATE_SLEEP_MODE_FLAG) {

				//Open config file to update sleep state
				fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_WRITE);
				if ((fx_result != FX_SUCCESS) && (fx_result != FX_ACCESS_ERROR)) {
					Error_Handler();
				}

				//Setup "write complete" callback function
				fx_result = fx_file_write_notify_set(&config_file, ConfigWriteComplete_Callback);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Update sleep state
				sleep_state[0] = true;

				//Go to beginning of config file
				fx_result = fx_file_seek(&config_file, 0);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Write prior state to beginning of config file
				config_writing = true;
				fx_file_write(&config_file, &sleep_state[0], sizeof(sleep_state[0]));
				while (config_writing);
				//fx_result = fx_file_close(&config_file);

				//Tell BMS thread to close BMS mosfets to power off tag
				tx_event_flags_set(&bms_event_flags_group, BMS_CLOSE_MOSFET_FLAG, TX_OR);
				tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_state_flags, TX_WAIT_FOREVER);
				if (actual_state_flags & BMS_OP_ERROR_FLAG) {

					//Reset sleep state
					sleep_state[0] = false;

					//Go to beginning of config file to redo write
					fx_result = fx_file_seek(&config_file, 0);
					if (fx_result != FX_SUCCESS) {
						Error_Handler();
					}

					//Write to config file
					config_writing = true;
					fx_file_write(&config_file, &sleep_state[0], sizeof(sleep_state[0]));
					while (config_writing);

					//Send error over uart and usb
					HAL_StatusTypeDef ret = HAL_ERROR;
					HAL_UART_Transmit_IT(&huart2, &ret, 1);

					if (usbTransmitLen == 0) {
						memcpy(&usbTransmitBuf[0], &ret, 1);
						usbTransmitLen = 1;
						tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
					}
				}
			}

			//USB mass storage device connected flag
			if (actual_state_flags & STATE_USB_MSB_ACTIVATED_FLAG) {

				//In USB MSB mode (turn off all threads except vital threads)
				if (!sleep_state[1]) {
					//Stop data capture and recovery threads
					if (state == STATE_DATA_CAPTURE) {
						soft_exit_data_capture();
					}
					else if (state == STATE_RECOVERY) {
						exit_recovery();
					}
					enter_data_offload();
				}
				//In USB CDC mode (turn on data capture threads for debugging)
				else {
					if (state == STATE_RECOVERY) {
						exit_recovery();
					}
					enter_data_capture();
				}
			}

			//USB mass storage device disconnected flag
			if (actual_state_flags & STATE_USB_MSB_DEACTIVATED_FLAG) {
				exit_data_offload();
				enter_data_capture();
			}

			if ((actual_state_flags & STATE_ENTER_SIM_FLAG) | (actual_state_flags & STATE_EXIT_SIM_FLAG)) {

				//Enter fishtracker
				if (state == STATE_DATA_CAPTURE) {
					soft_exit_data_capture();
				}
				else if (state == STATE_RECOVERY) {
					exit_recovery();
				}
				else if (state == STATE_DATA_OFFLOAD) {
					exit_data_offload();
				}

				if (actual_state_flags & STATE_EXIT_SIM_FLAG) {
					//Enter data capture if exiting simulation mode
					enter_data_capture();
					state = STATE_DATA_CAPTURE;

					//Send status over uart and usb
					HAL_StatusTypeDef ret = HAL_OK;
					HAL_UART_Transmit_IT(&huart2, &ret, 1);

					if (usbTransmitLen == 0) {
						memcpy(&usbTransmitBuf[0], &ret, 1);
						usbTransmitLen = 1;
						tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
					}
				}
				else {
					//Transition to desired state if entering simulation mode
					state = usbReceiveBuf[3];
				}

				switch (usbReceiveBuf[3]) {
					case STATE_DATA_CAPTURE:
						enter_data_capture();
						break;
					case STATE_RECOVERY:
						enter_recovery();
						break;
					case STATE_DATA_OFFLOAD:
						enter_data_offload();
						break;
					default:
						break;
				}

				//Send status over uart and usb
				HAL_StatusTypeDef ret = HAL_OK;
				HAL_UART_Transmit_IT(&huart2, &ret, 1);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					usbTransmitLen = 1;
					tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);
				}
			}
		}
	}
}


void enter_data_capture(){
	HAL_GPIO_WritePin(GPIOB, DIAG_LED1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, DIAG_LED2_Pin, GPIO_PIN_RESET);

	//Resume data capture threads
	tx_thread_resume(&threads[BMS_THREAD].thread);
	tx_thread_resume(&threads[RTC_THREAD].thread);
	tx_thread_resume(&threads[LIGHT_THREAD].thread);
	tx_thread_resume(&threads[AUDIO_THREAD].thread);
	tx_thread_resume(&threads[IMU_THREAD].thread);
	tx_thread_resume(&threads[COMMS_RX_THREAD].thread);
}


void soft_exit_data_capture(){

	//Suspend the data collection threads so we can just resume them later if needed
	tx_thread_suspend(&threads[COMMS_RX_THREAD].thread);
	tx_thread_suspend(&threads[AUDIO_THREAD].thread);
	tx_thread_suspend(&threads[IMU_THREAD].thread);
	tx_thread_suspend(&threads[ECG_THREAD].thread);
	tx_thread_suspend(&threads[DEPTH_THREAD].thread);
	tx_thread_suspend(&threads[DATA_LOG_THREAD].thread);
}

void hard_exit_data_capture(){

	//Signal data collection threads to stop running (and they should never run again)
	tx_event_flags_set(&audio_event_flags_group, AUDIO_STOP_THREAD_FLAG, TX_OR);
	tx_event_flags_set(&imu_event_flags_group, IMU_STOP_DATA_THREAD_FLAG, TX_OR);
	tx_event_flags_set(&depth_event_flags_group, DEPTH_STOP_DATA_THREAD_FLAG, TX_OR);
	tx_event_flags_set(&ecg_event_flags_group, ECG_STOP_DATA_THREAD_FLAG, TX_OR);
}

void enter_data_offload(){
	HAL_GPIO_WritePin(GPIOB, DIAG_LED1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, DIAG_LED2_Pin, GPIO_PIN_SET);

	tx_thread_suspend(&threads[BMS_THREAD].thread);
	tx_thread_suspend(&threads[LIGHT_THREAD].thread);

	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card clock division to be a little slower
	//MX_SDMMC1_SD_Fake_Init(DATA_OFFLOADING_SD_CLK_DIV);
}

void exit_data_offload(){
	tx_thread_resume(&threads[BMS_THREAD].thread);
	tx_thread_resume(&threads[LIGHT_THREAD].thread);

	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card back to the original clock divider
	//MX_SDMMC1_SD_Fake_Init(NORMAL_SD_CLK_DIV);
}

void enter_recovery(){
	HAL_GPIO_WritePin(GPIOB, DIAG_LED1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, DIAG_LED2_Pin, GPIO_PIN_SET);

	//Start Burnwire thread to release tag
	tx_thread_resume(&threads[BURNWIRE_THREAD].thread);

	tx_thread_terminate(&threads[LIGHT_THREAD].thread);
	tx_thread_terminate(&threads[RTC_THREAD].thread);

	//Start APRS and GPS thread
	//tx_thread_reset(&threads[APRS_THREAD].thread);
	//tx_thread_resume(&threads[APRS_THREAD].thread);
	//tx_thread_resume(&threads[GPS_THREAD].thread);
}

void exit_recovery(){
	HAL_GPIO_WritePin(GPIOB, DIAG_LED1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, DIAG_LED2_Pin, GPIO_PIN_RESET);

	//Stop all remaining threads in recovery mode
	tx_thread_terminate(&threads[DATA_LOG_THREAD].thread);
	tx_thread_terminate(&threads[COMMS_RX_THREAD].thread);
	tx_thread_terminate(&threads[APRS_THREAD].thread);
	tx_thread_terminate(&threads[GPS_THREAD].thread);
}


