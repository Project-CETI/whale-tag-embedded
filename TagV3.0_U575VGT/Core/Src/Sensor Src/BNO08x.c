/*
 * BNO08x.c
 *
 *  Created on: Feb 8, 2023
 *      Author: Kaveet Grewal
 *
 * 	Source file for the IMU drivers. See header file for more details
 */
#include "BNO08x.h"
#include "fx_api.h"
#include "app_filex.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include "stm32u5xx_hal_gpio.h"
#include "stm32u5xx_hal_spi.h"
#include <stdbool.h>
#include "Lib Inc/threads.h"

extern SPI_HandleTypeDef hspi1;

//FileX variables
extern FX_MEDIA        sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

static void IMU_read_startup_data(IMU_HandleTypeDef* imu);
static HAL_StatusTypeDef IMU_poll_new_data(IMU_HandleTypeDef* imu, uint32_t timeout);
static void IMU_configure_reports(IMU_HandleTypeDef * imu, uint8_t reportID, bool isLastReport);

TX_EVENT_FLAGS_GROUP imu_event_flags_group;

bool imu_running = false;

bool imu_writing = false;

//DEBUG
uint8_t good_counter = 0;
uint8_t bad = 0;

uint8_t accels = 0;
uint8_t gyros = 0;
uint8_t magnets = 0;
uint8_t quats = 0;

void imu_SDWriteComplete(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	imu_writing = false;
}

void IMU_thread_entry(ULONG thread_input){

	//Create IMU handler and initialize
	IMU_HandleTypeDef imu;

	IMU_init(&hspi1, &imu);

	//Create the events flag.
	tx_event_flags_create(&imu_event_flags_group, "IMU Event Flags");

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

	fx_file_write(&imu_file, &imu.data, sizeof(IMU_Data));
	while (imu_writing);

	//Enable our interrupt handler that signals data is ready
	HAL_NVIC_EnableIRQ(EXTI12_IRQn);

	while(1) {

		//Call to get data, this handles filling up our IMU data buffer completely
		IMU_get_data(&imu);
		imu_writing = true;
		fx_file_write(&imu_file, imu.data, sizeof(IMU_Data) * IMU_NUM_SAMPLES);

		//Wait for the writing to complete before giving up control to prevent unnecessary context switches
		while (imu_writing);

		good_counter = 0;

		//Check to see if there was a stop thread request. Put very little wait time so its essentially an instant check
		ULONG actual_flags = 0;
		tx_event_flags_get(&imu_event_flags_group, IMU_STOP_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, 1);

		//If there was something set cleanup the thread
		if (actual_flags & IMU_STOP_THREAD_FLAG){

			//Close the file and suspend our interrupt
			fx_file_close(&imu_file);
			HAL_NVIC_DisableIRQ(EXTI12_IRQn);

			//Delete threadx event flags and terminate the thread
			tx_event_flags_delete(&imu_event_flags_group);
			tx_thread_terminate(&threads[IMU_THREAD].thread);
		}
	}
}
void IMU_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu){

	imu->hspi = hspi;

	//GPIO info for important pins
	imu->cs_port = IMU_CS_GPIO_Port;
	imu->cs_pin = IMU_CS_Pin;

	imu->int_port = IMU_INT_GPIO_Port;
	imu->int_pin = IMU_INT_Pin;

	imu->wake_port = IMU_WAKE_GPIO_Port;
	imu->wake_pin = IMU_WAKE_Pin;

	//Delay to allow IMU to fully initialize
	HAL_Delay(1000);

	//The IMU outputs "advertisement" packets at startup. These aren't important for us, so we read through them.
	IMU_read_startup_data(imu);

	//Assert WAKE by causing a falling edge on the WAKE pin
	HAL_GPIO_WritePin(imu->wake_port, imu->wake_pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(imu->wake_port, imu->wake_pin, GPIO_PIN_RESET);

	IMU_configure_reports(imu, IMU_ROTATION_VECTOR_REPORT_ID, false);
	IMU_configure_reports(imu, IMU_ACCELEROMETER_REPORT_ID, false);
	IMU_configure_reports(imu, IMU_GYROSCOPE_REPORT_ID, false);
	IMU_configure_reports(imu, IMU_MAGNETOMETER_REPORT_ID, true);

	//Deassert wait to signal the end of configuration
	HAL_GPIO_WritePin(imu->wake_port, imu->wake_pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef IMU_get_data(IMU_HandleTypeDef* imu){

	//receive data buffer
	uint8_t receiveData[256] = {0};

	for (uint16_t index; index < IMU_NUM_SAMPLES; index++){

		ULONG actual_events;

		//Wait for data to be ready (suspends so other threads can run)
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

		//Read the header in to a buffer
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_StatusTypeDef ret = HAL_SPI_Receive(imu->hspi, receiveData, IMU_SHTP_HEADER_LENGTH, IMU_SPI_READ_TIMEOUT);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		//Extract length from first 2 bytes
		uint32_t dataLength = ((receiveData[1] << 8) | receiveData[0]) & IMU_LENGTH_BIT_MASK;

		//If there is some issue, ignore the data
		if (ret == HAL_TIMEOUT || dataLength <= 0){
			//Decrement index so we dont fill this part of the array
			index--;

			//Return to start of loop
			continue;
		}

		if (dataLength > 256){
			dataLength = 256;
		}

		//Now that we know the length, wait for the payload of data to be ready
		tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

		//Read the data (note that the SHTP header is resent with each read, so we still need to read the full length
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		ret = HAL_SPI_Receive(imu->hspi, receiveData, dataLength, IMU_SPI_READ_TIMEOUT);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

		//If there is some issue, ignore the data
		if (ret == HAL_TIMEOUT){

			//Same logic as above, reset our index
			index--;

			//Return to start of loop
			continue;
		}

		//Ensure this is the correct channel we're receiving data on and it has a timestamp
		if (receiveData[2] == IMU_DATA_CHANNEL && receiveData[4] == IMU_TIMESTAMP_REPORT_ID){

			//We may get more than one report at once, so use this to keep track of where we are in the data buffer.
			//So far we've parsed the header and timestamp data, so start at index 9 (first report ID)
			uint32_t parsedDataIndex = 9;
			while (parsedDataIndex < dataLength){

				//Ensure the data is somewhat valid (matches one of the report IDs)
				if (receiveData[parsedDataIndex] == IMU_ROTATION_VECTOR_REPORT_ID || receiveData[parsedDataIndex] == IMU_ACCELEROMETER_REPORT_ID || receiveData[parsedDataIndex] == IMU_GYROSCOPE_REPORT_ID || receiveData[parsedDataIndex] == IMU_MAGNETOMETER_REPORT_ID){

					//if its a quaternion, copy over all the bytes (10), if not just copy the 3 axis bytes (6)
					uint8_t bytesToCopy = (receiveData[parsedDataIndex] == IMU_ROTATION_VECTOR_REPORT_ID) ? IMU_QUAT_USEFUL_BYTES : IMU_3_AXIS_USEFUL_BYTES;

					//Copy the data header and then the useful data to our data struct
					imu->data[index].data_header = receiveData[parsedDataIndex];

					//Useful data starts 4 indexes after the report ID (skip over un-needed data)
					memcpy(imu->data[index].raw_data, &receiveData[parsedDataIndex + 4], bytesToCopy);

					//If it was a 3-axis report, fill the last 4 bytes with 0's
					if (bytesToCopy < IMU_QUAT_USEFUL_BYTES){
						imu->data[index].raw_data[6] = 0;
						imu->data[index].raw_data[7] = 0;
						imu->data[index].raw_data[8] = 0;
						imu->data[index].raw_data[9] = 0;
					}

					//DEBUG
					good_counter++;

					//increment forward to the next report
					parsedDataIndex += bytesToCopy + 4;

					//Increment index so we can store the next spot if this read has another report after it
					if (parsedDataIndex < dataLength){
						index++;
					}

					//If we filled the buffer, back out now (prevent overflow or hardfault)
					if (index >= IMU_NUM_SAMPLES){
						return HAL_OK;
					}
				}
				//If it doesnt match the header, we assume the rest of the packet is bad, so return to the top of the for loop (exit the while loop)
				else {
					index--;
					parsedDataIndex = dataLength;
				}
			}
		}
		//If its bad data
		else {
			//Dont increment index
			index--;
		}
	}


	return HAL_OK;
}

static void IMU_read_startup_data(IMU_HandleTypeDef* imu){

	//Helper variables
	bool reading = true;
	uint8_t receiveData[256] = {0};

	//Loop until done reading
	while (reading){

		//Poll for INT edge. If it never happens, then the IMU is done dumping data.
		if (IMU_poll_new_data(imu, IMU_DUMMY_PACKET_TIMEOUT_MS) == HAL_TIMEOUT){
			reading = false;
			break;
		}

		//Read through dummy data
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
		HAL_SPI_Receive(imu->hspi, receiveData, 100, 5000);
		HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);
	}
}

static HAL_StatusTypeDef IMU_poll_new_data(IMU_HandleTypeDef* imu, uint32_t timeout){

	//Get start time
	uint32_t startTime = HAL_GetTick();

	//Poll for RESET (asserted INT)
	while (HAL_GPIO_ReadPin(imu->int_port, imu->int_pin)){

		uint32_t currentTime = HAL_GetTick();
		if ((currentTime - startTime) > IMU_NEW_DATA_TIMEOUT_MS){
			return HAL_TIMEOUT;
		}
	}

	return HAL_OK;
}

static void IMU_configure_reports(IMU_HandleTypeDef * imu, uint8_t reportID, bool isLastReport){

	//Need to setup the IMU to send the appropriate data to us. Transmit a "set feature" command to start receiving rotation data.
	//All non-populated bytes are left as default 0.
	uint8_t transmitData[IMU_CONFIGURE_REPORT_LENGTH] = {0};

	//Configure SHTP header (first 4 bytes)
	transmitData[0] = IMU_CONFIGURE_REPORT_LENGTH; //LSB
	transmitData[1] = 0x00; //MSB
	transmitData[2] = IMU_CONTROL_CHANNEL;

	//Indicates we want to start receiving a report
	transmitData[4] = IMU_SET_FEATURE_REPORT_ID;

	//Indicates we want to receive rotation vector reports
	transmitData[5] = reportID;

	//Set how often we want to receive data
	transmitData[9] = IMU_REPORT_INTERVAL_0; //LSB
	transmitData[10] = IMU_REPORT_INTERVAL_1;
	transmitData[11] = IMU_REPORT_INTERVAL_2;
	transmitData[12] = IMU_REPORT_INTERVAL_3; //MSBs

	//Wait for IMU to be ready (poll for falling edge)
	if (IMU_poll_new_data(imu, HAL_MAX_DELAY) == HAL_TIMEOUT){
		return;
	}

	//Prep wake pin for another falling edge (if theres another report coming after this one)
	if (!isLastReport){
		HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_SET);
	}

	//Select CS by pulling low and write configuration to the IMU. Add delays to ensure good timing.
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_Delay(1);

	//Create another falling edge on the wake pin during our transfer to signal another transfer is coming
	if (!isLastReport){
		HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_RESET);
		HAL_Delay(1);
	}

	//Transmit data and pull CS back up to high
	HAL_SPI_Transmit(&hspi1, transmitData, IMU_CONFIGURE_REPORT_LENGTH, HAL_MAX_DELAY);
	HAL_Delay(1);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);
}
