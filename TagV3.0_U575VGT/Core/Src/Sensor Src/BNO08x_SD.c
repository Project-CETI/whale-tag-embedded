/*
 * BNO08x_SD.c
 *
 *  Created on: Aug 8, 2023
 *      Author: Kaveet
 *
 * See header file for details
 */

#include "Sensor Inc/BNO08x_SD.h"
#include "app_filex.h"

//FileX variables
extern FX_MEDIA        sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//ThreadX useful variables (external because theyre shared with the data collection thread)
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_MUTEX imu_first_half_mutex;
extern TX_MUTEX imu_second_half_mutex;

//Array for holding IMU data. The buffer is split in half and shared with the IMU thread.
extern IMU_Data imu_data[2][IMU_HALF_BUFFER_SIZE];

uint8_t imu_writing = 0;

//Callback for completed SD card write
void imu_SDWriteComplete(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	imu_writing = 0;
}


void imu_sd_thread_entry(ULONG thread_input){

	FX_FILE imu_file = {};

	//Create our binary file for dumping imu data
	UINT fx_result = FX_SUCCESS;
	fx_result = fx_file_create(&sdio_disk, "imu_test.bin");
	if((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)){
	  Error_Handler();
	}

	//Open the file
	fx_result = fx_file_open(&sdio_disk, &imu_file, "imu_test.bin", FX_OPEN_FOR_WRITE);
	if(fx_result != FX_SUCCESS){
	  Error_Handler();
	}

	//Set our "write complete" callback function
	fx_result = fx_file_write_notify_set(&imu_file, imu_SDWriteComplete);
	if(fx_result != FX_SUCCESS){
	  Error_Handler();
	}

	//Dummy write at the beggining (first write is usually slow)
	imu_writing = 125; //DEBUG TO BE MORE VISIBLE ON CUBE MONITOR
	fx_file_write(&imu_file, imu_data[0], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);
	while (imu_writing);

	while (1){

		//Wait for the Data collection thread to be done filling the first half of the buffer
		tx_mutex_get(&imu_first_half_mutex, TX_WAIT_FOREVER);

		//Write the first half to the SD card
		imu_writing = 125; //DEBUG TO BE MORE VISIBLE ON CUBE MONITOR
		fx_file_write(&imu_file, imu_data[0], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);

		//Wait for the writing to complete before continuing
		while (imu_writing);

		//Release mutex (allow for data thread to write to buffer)
		tx_mutex_put(&imu_first_half_mutex);

		//Wait for second half buffer
		tx_mutex_get(&imu_second_half_mutex, TX_WAIT_FOREVER);

		//Write second half to the SD card
		imu_writing = 125; //DEBUG TO BE MORE VISIBLE ON CUBE MONITOR
		fx_file_write(&imu_file, imu_data[1], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);

		//Wait for the writing to complete before continuing
		while (imu_writing);

		//Release second half mutex
		tx_mutex_put(&imu_second_half_mutex);
	}
}
