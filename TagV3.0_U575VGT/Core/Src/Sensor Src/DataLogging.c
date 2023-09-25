/*
 * DataLogging.c
 *
 *  Created on: Feb 9, 2023
 *      Author: amjad
 */

#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/ECG.h"
#include "Sensor Inc/KellerDepth.h"
#include "Sensor Inc/RTC.h"
#include "Lib Inc/threads.h"
#include "app_filex.h"
#include <stdio.h>
#include <stdbool.h>

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

//RTC date and time
extern RTC_TimeTypeDef eTime;
extern RTC_DateTypeDef eDate;
extern bool rtc_init;

//FileX variables
extern FX_MEDIA        sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//ThreadX useful variables (external because theyre shared with the data collection thread)
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_MUTEX imu_first_half_mutex;
extern TX_MUTEX imu_second_half_mutex;

//Arrays for holding sensor data. The buffer is split in half and shared with the IMU thread.
extern IMU_Data imu_data[2][IMU_HALF_BUFFER_SIZE];

//Status variable to indicate thread is writing to SD card
uint8_t sd_writing = false;

//Callback for completed SD card write
void SDWriteComplete_Callback(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	sd_writing = false;
}

void sd_thread_entry(ULONG thread_input) {

	FX_FILE imu_file = {};
	FX_FILE depth_file = {};
	UINT fx_result = FX_SUCCESS;

	char file_name = "data.bin";

	//Create our binary file for dumping sensor data
	fx_result = fx_file_create(&sdio_disk, file_name);
	if((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)){
	  Error_Handler();
	}

	//Open binary file
	fx_result = fx_file_open(&sdio_disk, &imu_file, file_name, FX_OPEN_FOR_WRITE);
	if(fx_result != FX_SUCCESS){
	  Error_Handler();
	}

	//Setup "write complete" callback function
	fx_result = fx_file_write_notify_set(&imu_file, SDWriteComplete_Callback);
	if(fx_result != FX_SUCCESS){
	  Error_Handler();
	}

	//Dummy write at start since first write is usually slow
	sd_writing = true;
	fx_file_write(&imu_file, imu_data[0], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);
	while (sd_writing);

	//Sequentially save buffers for all sensors
	while (1) {

		//Wait for the data collection threads to be done filling the first half of the buffer
		tx_mutex_get(&imu_first_half_mutex, TX_WAIT_FOREVER);

		//Write the first half to the SD card
		sd_writing = true;
		fx_file_write(&imu_file, imu_data[0], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);

		//Wait for the writing to complete before continuing
		while (sd_writing);

		//Release mutex (allow for data thread to write to buffer)
		tx_mutex_put(&imu_first_half_mutex);

		//Wait for the data collection threads to be done filling the second half of the buffer
		tx_mutex_get(&imu_second_half_mutex, TX_WAIT_FOREVER);

		//Write second half to the SD card
		sd_writing = true;
		fx_file_write(&imu_file, imu_data[1], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);

		//Wait for the writing to complete before continuing
		while (sd_writing);

		//Release second half mutex
		tx_mutex_put(&imu_second_half_mutex);



		//Check to see if a stop flag was raised
		ULONG actual_flags;
		tx_event_flags_get(&imu_event_flags_group, IMU_STOP_SD_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		//tx_event_flags_get(&imu_event_flags_group, DEPTH_STOP_SD_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		tx_event_flags_get(&imu_event_flags_group, ECG_STOP_SD_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);

		//If the stop flag was raised
		if (actual_flags & IMU_STOP_SD_THREAD_FLAG){

			//close the file
			fx_file_close(&imu_file);

			//Delete the event flag group
			tx_event_flags_delete(&imu_event_flags_group);

			//Terminate the thread
			tx_thread_terminate(&threads[DATA_LOG_THREAD].thread);
		}
	}
}
