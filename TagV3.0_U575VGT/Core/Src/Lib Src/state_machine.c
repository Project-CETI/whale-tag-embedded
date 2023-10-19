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
#include "Lib Inc/threads.h"
#include "app_usbx_device.h"
#include "main.h"
#include "app_filex.h"

//Event flags for signaling changes in state
TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

//External event flags for the other sensors. Use these to signal them to whenever they get the chance
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP usb_event_flags_group;

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

//FileX variables
extern FX_MEDIA sdio_disk;

//State variable
State state = 0;
State prior_state = 0;
uint8_t slept_status = 0;

//State machine flags
ULONG actual_flags;

void state_machine_thread_entry(ULONG thread_input){

	//Create event flags for triggering state changes
	tx_event_flags_create(&state_machine_event_flags_group, "State Machine Event Flags");

	//Create config file if not exist
	FX_FILE config_file;
	UINT fx_result = fx_file_create(&sdio_disk, "config_file.txt");
	if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
		Error_Handler();
	}

	//Read config file to check if tag returning from sleep mode
	if (fx_result == FX_ALREADY_CREATED) {
		fx_result = fx_file_open(&sdio_disk, &config_file, "config_file.txt", FX_OPEN_FOR_READ);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}

		ULONG actual_bytes = 0;
		fx_result = fx_file_read(&config_file, &slept_status, 1, &actual_bytes);
		if (fx_result != FX_SUCCESS) {
			Error_Handler();
		}
		if (slept_status) {
			prior_state = slept_status;
			fx_file_delete(&sdio_disk, "config_file.txt");
		}
	}

	//If simulating, set the simulation state defined in the header file, else, enter data capture as a default
	if (!slept_status) {
		state = (IS_SIMULATING) ? SIMULATING_STATE : STATE_DATA_CAPTURE;
	}
	else {
		state = STATE_FISHTRACKER;
	}

	//Check the initial state and start in the appropriate state
	if (state == STATE_DATA_CAPTURE){
		enter_data_capture();
	}
	else if (state == STATE_RECOVERY){
		enter_recovery();
	}
	else if (state == STATE_DATA_OFFLOAD){
		enter_data_offload();
	}

	//Enter main thread execution loop ONLY if we arent simulating
	if (!IS_SIMULATING){
		while (1){

			ULONG actual_flags = 0;
			tx_event_flags_get(&state_machine_event_flags_group, ALL_STATE_FLAGS, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

			//RTC timeout flag or GPS geofencing flag (outside of Dominica)
			if ((actual_flags & STATE_TIMEOUT_FLAG) || (actual_flags & STATE_GPS_FENCE_FLAG)){

				//Exit data capture and enter recovery
				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
					enter_recovery();
					state = STATE_RECOVERY;
				}
			}

			//BMS low battery flag
			if (actual_flags & STATE_LOW_BATT_FLAG){

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
				fx_result = fx_file_open(&sdio_disk, &config_file, "config_file.txt", FX_OPEN_FOR_WRITE);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Enter fishtracker mode and save prior state
				if (state == STATE_DATA_CAPTURE) {
					hard_exit_data_capture();
					state = STATE_FISHTRACKER;
					prior_state = 1;
				}
				else if (state == STATE_RECOVERY) {
					exit_recovery();
					state = STATE_FISHTRACKER;
					prior_state = 2;
				}

				//Write prior state to config file
				fx_result = fx_file_write(&config_file, &prior_state, sizeof(prior_state));
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}
			}

			//BMS exit sleep mode flag
			if (actual_flags & STATE_EXIT_SLEEP_MODE_FLAG) {

				//Restore prior state
				if (prior_state == STATE_DATA_CAPTURE) {
					enter_data_capture();
					state = STATE_DATA_CAPTURE;
				}
				else if (prior_state == STATE_RECOVERY) {
					enter_recovery();
					state = STATE_RECOVERY;
				}
			}

			//USB connected flag
			if (actual_flags & STATE_USB_CONNECTED_FLAG){

				if (state == STATE_DATA_CAPTURE){
					soft_exit_data_capture();
				}
				else if (state == STATE_RECOVERY){
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

	//Resume data capture threads (they will no longer be in a suspended state)
	//tx_thread_resume(&threads[AUDIO_THREAD].thread);
	tx_thread_resume(&threads[IMU_THREAD].thread);
}


void soft_exit_data_capture(){

	//Suspend the data collection threads so we can just resume them later if needed
	tx_thread_suspend(&threads[AUDIO_THREAD].thread);
	tx_thread_suspend(&threads[IMU_THREAD].thread);
	tx_thread_suspend(&threads[ECG_THREAD].thread);
	tx_thread_suspend(&threads[DEPTH_THREAD].thread);
	tx_thread_suspend(&threads[GPS_THREAD].thread);
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
	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card clock divison to be a little slower
	MX_SDMMC1_SD_Fake_Init(DATA_OFFLOADING_SD_CLK_DIV);
}

void exit_data_offload(){
	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card back to the original clock divider
	MX_SDMMC1_SD_Fake_Init(NORMAL_SD_CLK_DIV);
}

void enter_recovery(){
	//Start APRS thread
	tx_thread_reset(&threads[APRS_THREAD].thread);
	tx_thread_resume(&threads[APRS_THREAD].thread);

	//Start Burnwire thread to release tag
	tx_thread_resume(&threads[BURNWIRE_THREAD].thread);
}

void exit_recovery(){
	//Stop APRS, BMS, GPS thread
	//tx_thread_termiante(&threads[COMMS_RX_THREAD].thread);
	tx_thread_terminate(&threads[APRS_THREAD].thread);
	tx_thread_terminate(&threads[GPS_THREAD].thread);
	//tx_thread_terminate(&threads[BMS_THREAD].thread);
}


