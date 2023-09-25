/*
 * threads.h
 *
 *  Created on: Jul 6, 2023
 *      Author: Kaveet
 *
 * This file contains useful structs and enums for defining new threads using threadX.
 *
 * To create a new thread, declare it in the __TX_THREAD_LIST enum, and then declare the configuration information in the threadConfigList.
 *
 * Avoid touching the other structures (e.g the thread array). The threadListInit() function and App_ThreadX_Init() (inside of app_threadx.h) handle all other creation and memory allocation.
 *
 * If creating a new thread causes memory allocation issues (tx_byte_allocate or tx_thread_create fail/hardfault), you may need to allocate more memory to ThreadX. This can be done inside of the IOC file.
 */

#ifndef INC_LIB_INC_THREADS_H_
#define INC_LIB_INC_THREADS_H_

#include "tx_api.h"
#include "app_threadx.h"
#include "Sensor Inc/audio.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/BNO08x_SD.h"
#include "Sensor Inc/ECG.h"
#include "Sensor Inc/GpsGeofencing.h"
#include "Sensor Inc/ECG_SD.h"
#include "Sensor Inc/RTC.h"
#include "Lib Inc/state_machine.h"
#include "Recovery Inc/Aprs.h"
#include "Recovery Inc/Burnwire.h"

//Enum for all threads so we can easily keep track of the list + total number of threads.
// If adding a new thread to the list, put it before the "NUM_THREADS" element, as it must always be the last element in the enum.
typedef enum __TX_THREAD_LIST {
	STATE_MACHINE_THREAD,
	AUDIO_THREAD,
	IMU_THREAD,
	DEPTH_THREAD,
	IMU_SD_THREAD,
	ECG_THREAD,
	ECG_SD_THREAD,
	GPS_THREAD,
	APRS_THREAD,
	BURNWIRE_THREAD,
	RTC_THREAD,
	NUM_THREADS //DO NOT ADD THREAD ENUMS BELOW THIS
}Thread;

//TX Thread configuration members that can be defined at runtime (constants).
//We put these in a separate struct so they can be defined in the header file when adding new threads.
typedef struct __TX_THREAD_Config_TypeDef{

	char * thread_name;

	VOID (*thread_entry_function)(ULONG);
	ULONG thread_input;

	ULONG thread_stack_size;

	UINT priority;
	UINT preempt_threshold;

	ULONG timeslice;
	UINT start;

}Thread_ConfigTypeDef;

//The main TX Thread handler. The "thread" and "thread_stack_start" element need to be defined during runtime, so we keep them in this struct and not the config struct.
typedef struct __TX_THREAD_TypeDef {

	TX_THREAD thread;

	VOID * thread_stack_start;

	Thread_ConfigTypeDef config;

}Thread_HandleTypeDef;

//Define the config for each struct here, in the same order the are listed in the Thread Enum above.
static Thread_ConfigTypeDef threadConfigList[NUM_THREADS] = {
		[STATE_MACHINE_THREAD] = {
			//State Machine
			.thread_name = "State Machine Thread",
			.thread_entry_function = state_machine_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 1024,
			.priority = 2,
			.preempt_threshold = 2,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[AUDIO_THREAD] = {
			//Audio Thread
			.thread_name = "Audio Thread",
			.thread_entry_function = audio_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 2048,
			.priority = 3,
			.preempt_threshold = 3,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[IMU_THREAD] = {
			//IMU Thread
			.thread_name = "IMU Thread",
			.thread_entry_function = imu_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 800,
			.priority = 4,
			.preempt_threshold = 4,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[DEPTH_THREAD] = {
			//IMU Thread
			.thread_name = "Depth Thread",
			.thread_entry_function = keller_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 800,
			.priority = 4,
			.preempt_threshold = 4,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[IMU_SD_THREAD] = {
			//IMU SD Thread
			.thread_name = "IMU SD Thread",
			.thread_entry_function = imu_sd_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 2048,
			.priority = 5,
			.preempt_threshold = 5,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[ECG_THREAD] = {
			//ECG Thread
			.thread_name = "ECG Thread",
			.thread_entry_function = ecg_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 1024,
			.priority = 6,
			.preempt_threshold = 6,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[ECG_SD_THREAD] = {
			//ECG_SD Thread
			.thread_name = "ECG SD Thread",
			.thread_entry_function = ecg_sd_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 2048,
			.priority = 7,
			.preempt_threshold = 7,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[GPS_THREAD] = {
			//GPS Geofencing
			.thread_name = "GPS Thread",
			.thread_entry_function = gps_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 1024,
			.priority = 12,
			.preempt_threshold = 12,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[APRS_THREAD] = {
			//APRS Thread
			.thread_name = "APRS Thread",
			.thread_entry_function = aprs_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 2048,
			.priority = 8,
			.preempt_threshold = 8,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[BURNWIRE_THREAD] = {
			//Burnwire Thread
			.thread_name = "Burnwire Thread",
			.thread_entry_function = burnwire_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 1024,
			.priority = 10,
			.preempt_threshold = 10,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_DONT_START
		},
		[RTC_THREAD] = {
			// RTC Thread
			.thread_name = "RTC Thread",
			.thread_entry_function = RTC_thread_entry,
			.thread_input = 0x1234,
			.thread_stack_size = 2048,
			.priority = 8,
			.preempt_threshold = 8,
			.timeslice = TX_NO_TIME_SLICE,
			.start = TX_AUTO_START
		}
};

//An array to hold all the threads. We do NOT need to touch this at all the add new threads, only edit the config list (above).
extern Thread_HandleTypeDef threads[NUM_THREADS];

//Function to be called BEFORE creating any TX threads. It initializes the stack pointers and populates the thread handler array.
void threadListInit();

#endif /* INC_LIB_INC_THREADS_H_ */
