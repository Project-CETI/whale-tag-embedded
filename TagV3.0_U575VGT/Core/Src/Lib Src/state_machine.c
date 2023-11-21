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
#include "main.h"
#include "app_filex.h"
#include <stdbool.h>
#include <stdio.h>

//Event flags for signaling changes in state
TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

//External event flags for the other sensors. Use these to signal them to whenever they get the chance
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP usb_event_flags_group;
extern TX_EVENT_FLAGS_GROUP bms_event_flags_group;

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

//FileX variables
extern FX_MEDIA sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//BMS handler
extern MAX17320_HandleTypeDef bms;

//Status variable to indicate thread is writing to SD card
bool config_writing = false;

//State variable
State state = STATE_DATA_CAPTURE;
State prior_state[10] = {STATE_SLEEP};

//State machine flags
ULONG actual_flags;

//Callback for completed SD card write
void ConfigWriteComplete_Callback(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	config_writing = false;
}

void state_machine_thread_entry(ULONG thread_input){

	//Create event flags for triggering state changes
	tx_event_flags_create(&state_machine_event_flags_group, "State Machine Event Flags");

	//Create config file if not exist
	FX_FILE config_file = {};
	UINT fx_result = FX_SUCCESS;
	char *file_name = "config.bin";

	//Create config file
	fx_result = fx_file_create(&sdio_disk, file_name);
	if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
		Error_Handler();
	}

	//Initialize config file if file creation successful
	if (fx_result == FX_SUCCESS) {

		//Write first byte to config file
		fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_WRITE);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		//Setup "write complete" callback function
		fx_result = fx_file_write_notify_set(&config_file, ConfigWriteComplete_Callback);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		//Tell BMS thread to write nonvolatile memory settings
		tx_event_flags_set(&bms_event_flags_group, BMS_NV_SETUP_FLAG, TX_OR);

		//Wait for operation to finish
		tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		if (actual_flags & BMS_OP_ERROR_FLAG) {
			state = STATE_BMS_ERROR;
		}

		//Set number of remaining writes for BMS
		prior_state[1] = bms.remaining_writes;

		//Write initial configuration to config file
		config_writing = true;
		fx_file_write(&config_file, &prior_state[0], sizeof(prior_state));
		while (config_writing);
	}
	//Read config file if already created and not simulating
	else if ((fx_result == FX_ALREADY_CREATED) && (!IS_SIMULATING)) {

		//Declare tag is sleeping
		state = STATE_SLEEP;

		//Read config file to check if tag returning from sleep mode
		fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_READ);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		ULONG actual_bytes;
		fx_result = fx_file_read(&config_file, &prior_state[0], 3, &actual_bytes);

		//Immediately change states if file error or if prior state is sleep state
		if ((fx_result != FX_SUCCESS) || (prior_state[0] == STATE_SLEEP)) {
			state = STATE_DATA_CAPTURE;
			enter_data_capture();
		}
	}

	if (IS_SIMULATING) {
		//If simulating, set the simulation state defined in the header file, else, enter data capture as a default
		//state = (IS_SIMULATING) ? SIMULATING_STATE : STATE_DATA_CAPTURE;

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

		ULONG actual_flags = 0;
		tx_event_flags_get(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG | STATE_USB_CONNECTED_FLAG | STATE_USB_DISCONNECTED_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

		//BMS exit sleep mode flag
		if (actual_flags & STATE_EXIT_SLEEP_MODE_FLAG) {

			//Tell BMS thread to open BMS mosfets to power tag
			tx_event_flags_set(&bms_event_flags_group, BMS_OPEN_MOSFET_FLAG, TX_OR);
			tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

			//Restore prior state
			enter_data_capture();
			state = STATE_DATA_CAPTURE;

			//Write prior state as sleep state to beginning of config file
			fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_WRITE);
			if (fx_result != FX_SUCCESS) {
				Error_Handler();
			}

			//Setup "write complete" callback function
			fx_result = fx_file_write_notify_set(&config_file, ConfigWriteComplete_Callback);
			if (fx_result != FX_SUCCESS) {
				Error_Handler();
			}

			prior_state[0] = STATE_SLEEP;
			fx_result = fx_file_seek(&config_file, 0);
			if (fx_result != FX_SUCCESS) {
				Error_Handler();
			}

			config_writing = true;
			fx_file_write(&config_file, &prior_state[0], sizeof(prior_state[0]));
			while (config_writing);
		}

		//USB connected flag
		if (actual_flags & STATE_USB_CONNECTED_FLAG) {
			enter_data_offload();
		}

		//USB disconnected flag
		if (actual_flags & STATE_USB_DISCONNECTED_FLAG) {
			exit_data_offload();
			enter_data_capture();
		}
	}


	//Enter main thread execution loop ONLY if we arent simulating
	if (!IS_SIMULATING) {
		while (1){

			ULONG actual_flags = 0;
			tx_event_flags_get(&state_machine_event_flags_group, ALL_STATE_FLAGS, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

			//RTC timeout flag or GPS geofencing flag (outside of Dominica)
			if ((actual_flags & STATE_TIMEOUT_FLAG) || (actual_flags & STATE_GPS_FENCE_FLAG)) {

				//Exit data capture and enter recovery
				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
					enter_recovery();
					state = STATE_RECOVERY;
				}
			}

			//BMS low battery flag
			if ((actual_flags & STATE_LOW_BATT_FLAG) || (actual_flags & STATE_SD_CARD_FULL_FLAG)) {

				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
					enter_recovery();
					state = STATE_RECOVERY;
				}
			}

			//BMS critical battery flag
			if (actual_flags & STATE_CRIT_BATT_FLAG) {

				if (state == STATE_RECOVERY) {
					exit_recovery();
					state = STATE_FISHTRACKER;
				}
			}

			//BMS sleep mode flag
			if (actual_flags & STATE_SLEEP_MODE_FLAG) {

				//Open config file
				fx_result = fx_file_open(&sdio_disk, &config_file, file_name, FX_OPEN_FOR_WRITE);
				if ((fx_result != FX_SUCCESS) && (fx_result != FX_ACCESS_ERROR)) {
					Error_Handler();
				}

				//Setup "write complete" callback function
				fx_result = fx_file_write_notify_set(&config_file, ConfigWriteComplete_Callback);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Go to beginning of config file
				prior_state[0] = STATE_DATA_CAPTURE;
				fx_result = fx_file_seek(&config_file, 0);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Write prior state to beginning of config file
				config_writing = true;
				fx_file_write(&config_file, &prior_state[0], sizeof(prior_state[0]));
				while (config_writing);
				fx_result = fx_file_close(&config_file);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Tell BMS thread to close BMS mosfets to power off tag
				tx_event_flags_set(&bms_event_flags_group, BMS_CLOSE_MOSFET_FLAG, TX_OR);
				tx_event_flags_get(&bms_event_flags_group, BMS_OP_DONE_FLAG | BMS_OP_ERROR_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
				if (actual_flags & BMS_OP_ERROR_FLAG) {
					state = STATE_BMS_ERROR;
				}
			}

			//USB connected flag
			if (actual_flags & STATE_USB_CONNECTED_FLAG) {

				if (state == STATE_DATA_CAPTURE) {
					soft_exit_data_capture();
				}
				else if (state == STATE_RECOVERY) {
					exit_recovery();
				}

				enter_data_offload();
				state = STATE_DATA_OFFLOAD;
			}

			//USB disconnected flag
			if (actual_flags & STATE_USB_DISCONNECTED_FLAG) {
				exit_data_offload();
				enter_data_capture();
				state = STATE_DATA_CAPTURE;
			}
		}
	}
}


void enter_data_capture(){

	//Resume data capture threads
	//tx_thread_resume(&threads[AUDIO_THREAD].thread);
	tx_thread_resume(&threads[IMU_THREAD].thread);
}


void soft_exit_data_capture(){

	//Suspend the data collection threads so we can just resume them later if needed
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
	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card clock division to be a little slower
	MX_SDMMC1_SD_Fake_Init(DATA_OFFLOADING_SD_CLK_DIV);
	HAL_GPIO_WritePin(GPIOB, DIAG_LED1_Pin, GPIO_PIN_SET);
}

void exit_data_offload(){
	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card back to the original clock divider
	MX_SDMMC1_SD_Fake_Init(NORMAL_SD_CLK_DIV);
	HAL_GPIO_WritePin(GPIOB, DIAG_LED1_Pin, GPIO_PIN_RESET);
}

void enter_recovery(){
	//Start APRS thread
	tx_thread_reset(&threads[APRS_THREAD].thread);
	tx_thread_resume(&threads[APRS_THREAD].thread);

	//Start Burnwire thread to release tag
	tx_thread_resume(&threads[BURNWIRE_THREAD].thread);
}

void exit_recovery(){
	//Stop all remaining threads in recovery mode: APRS, BMS, GPS
	//tx_thread_termiante(&threads[COMMS_RX_THREAD].thread);
	tx_thread_terminate(&threads[APRS_THREAD].thread);
	tx_thread_terminate(&threads[GPS_THREAD].thread);
	//tx_thread_terminate(&threads[BMS_THREAD].thread);
}


