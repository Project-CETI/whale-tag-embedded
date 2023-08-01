/*
 * state_machine.c
 *
 *  Created on: Jul 24, 2023
 *      Author: Kaveet
 */


#include "Lib Inc/state_machine.h"
#include "Sensor Inc/audio.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/ECG.h"
#include "Lib Inc/threads.h"
#include "app_usbx_device.h"
#include "main.h"

//Event flags for signaling changes in state
TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

//External event flags for the other sensors. Use these to signal them to whenever they get the chance
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP usb_event_flags_group;

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

void state_machine_thread_entry(ULONG thread_input){

	//Event flags for triggering state changes
	tx_event_flags_create(&state_machine_event_flags_group, "State Machine Event Flags");

	//If simulating, set the simulation state defined in the header file, else, enter data capture as a default
	State state = (IS_SIMULATING) ? SIMULATING_STATE : STATE_DATA_CAPTURE;

	//Check the initial state and start in the appropriate state
	if (state == STATE_DATA_CAPTURE){
		enter_data_capture();
	} else if (state == STATE_RECOVERY){
		enter_recovery();
	} else if (state == STATE_DATA_OFFLOAD){
		enter_data_offload();
	}

	//Enter main thread execution loop ONLY if we arent simulating
	if (!IS_SIMULATING){
		while (1){

			ULONG actual_flags;
			tx_event_flags_get(&state_machine_event_flags_group, ALL_STATE_FLAGS, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

			//Timeout event
			if (actual_flags & STATE_TIMEOUT_FLAG){

			}

			//GPS geofencing flag
			if (actual_flags & STATE_GPS_FLAG){

			}

			//BMS low battery flag
			if (actual_flags & STATE_LOW_BATT_FLAG){

			}

			//USB detected flag
			if (actual_flags & STATE_USB_CONNECTED_FLAG){
				if (state == STATE_DATA_CAPTURE){
					exit_data_capture();
				}
				else if (state == STATE_RECOVERY){
					exit_recovery();
				}

				enter_data_offload();
				state = STATE_DATA_OFFLOAD;
			}

			if (actual_flags & STATE_USB_DISCONNECTED_FLAG){

				//USB was disconnected. Exit data offload and enter data capture mode.
				exit_data_offload();

				enter_data_capture();

				state = STATE_DATA_CAPTURE;
			}
		}
	}
}


void enter_data_capture(){

	//Start data capture threads
	tx_thread_resume(&threads[AUDIO_THREAD].thread);
	tx_thread_resume(&threads[IMU_THREAD].thread);
	tx_thread_resume(&threads[ECG_THREAD].thread);
}


void exit_data_capture(){

	//Signal data collection threads to stop running
	tx_thread_suspend(&threads[AUDIO_THREAD].thread);
	tx_thread_suspend(&threads[IMU_THREAD].thread);
	tx_thread_suspend(&threads[ECG_THREAD].thread);
}


void enter_recovery(){
	//Start APRS thread
	tx_thread_reset(&threads[APRS_THREAD].thread);
}


void exit_recovery(){
	//Stop APRS thread
	tx_thread_terminate(&threads[APRS_THREAD].thread);
}


void enter_data_offload(){
	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card clock divison to be a little slower
	MX_SDMMC1_SD_Fake_Init(DATA_OFFLOADING_SD_CLK_DIV);
}

void exit_data_offload(){
	//Data offloading is always running, so we dont need to stop or start any threads, just adjust our SD card back to the original clock divider
	MX_SDMMC1_SD_Fake_Init(NORMAL_SD_CLK_DIV);
}
