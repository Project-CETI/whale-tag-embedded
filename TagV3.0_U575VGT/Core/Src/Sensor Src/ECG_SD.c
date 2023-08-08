/*
 * ECG_SD.c
 *
 *  Created on: Aug 8, 2023
 *      Author: Kaveet
 *
 * See Header file (ECG_SD.H) for details.
 */

#include "Sensor Inc/ECG_SD.h"
#include "app_filex.h"
#include <stdbool.h>

//FileX variables
extern FX_MEDIA        sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//External variables to share with the ECG data thread
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_MUTEX ecg_first_half_mutex;
extern TX_MUTEX ecg_second_half_mutex;

//Array for holding ECG data. The buffer is split in half and shared with the ECG thread.
extern ECG_Data ecg_data[2][ECG_HALF_BUFFER_SIZE];

//DEBUG
uint8_t ecg_writing = 0;

void ecg_SDWriteComplete(FX_FILE *file){
	//Indicate that we are no longer writing to the SD card
	ecg_writing = 0;
}

void ecg_sd_thread_entry(ULONG thread_input){

	FX_FILE ecg_file = {};

	//Create our binary file for dumping ecg data
	UINT fx_result = FX_SUCCESS;
	fx_result = fx_file_create(&sdio_disk, "ecg_test.bin");
	if((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)){
	  Error_Handler();
	}

	//Open the file
	fx_result = fx_file_open(&sdio_disk, &ecg_file, "ecg_test.bin", FX_OPEN_FOR_WRITE);
	if(fx_result != FX_SUCCESS){
	  Error_Handler();
	}

	//Set our "write complete" callback function
	fx_result = fx_file_write_notify_set(&ecg_file, ecg_SDWriteComplete);
	if(fx_result != FX_SUCCESS){
	  Error_Handler();
	}

	//Do a dummy write for the beginning of the file
	ecg_writing = 250; //Set to 250 for easier validation in cube monitor (eventually make this true and false)
	fx_file_write(&ecg_file, ecg_data, sizeof(float) * ECG_BUFFER_SIZE);
	while (ecg_writing);

	while (1){

		//Wait for first half buffer to fill up
		tx_mutex_get(&ecg_first_half_mutex, TX_WAIT_FOREVER);

		//First half buffer is full, write to the SD card
		ecg_writing = 250; //Set to 250 for easier validation in cube monitor (eventually make this true and false)
		fx_file_write(&ecg_file, ecg_data[0], sizeof(ECG_Data) * ECG_HALF_BUFFER_SIZE);

		//Poll for completion (other threads will still run)
		while (ecg_writing);

		//Release first half mutex (done writing)
		tx_mutex_put(&ecg_first_half_mutex);

		//Wait for second half buffer to fill up
		tx_mutex_get(&ecg_second_half_mutex, TX_WAIT_FOREVER);

		//Second half of the buffer is full, write to SD card
		ecg_writing = 250; //Set to 250 for easier validation in cube monitor (eventually make this true and false)
		fx_file_write(&ecg_file, ecg_data[1], sizeof(ECG_Data) * ECG_HALF_BUFFER_SIZE);

		//Poll for completion
		while (ecg_writing);

		//Release first half mutex (done writing)
		tx_mutex_put(&ecg_second_half_mutex);
	}
}


