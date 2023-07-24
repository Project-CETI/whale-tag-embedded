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

TX_EVENT_FLAGS_GROUP imu_event_flags_group;

bool imu_running = false;

bool imu_writing = false;

uint8_t good_counter = 0;

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

	//Dummy write to show the beggining of the file
	imu.data.quat_r = 0x0201;
	imu.data.quat_i = 0x0403;
	imu.data.quat_j = 0x0605;
	imu.data.quat_k = 0x0807;
	imu.data.accurary_rad = 0x0A09;
	fx_file_write(&imu_file, &imu.data, sizeof(IMU_Data));

	//Enable our interrupt handler that signals data is ready
	HAL_NVIC_EnableIRQ(EXTI12_IRQn);


	while(1) {

		//Array to hold enough sets of IMU data for our SD card write
		IMU_Data imu_data[IMU_NUM_SAMPLES] = {0};

		//Collect 10 pieces of IMU data
		for (uint8_t index = 0; index < IMU_NUM_SAMPLES; index++){

			//variable that holds the results of the flag polling (returns which flags are set)
			ULONG actual_events;

			//Poll for data to be readyb or for a stop command. Flag is set by our interrupt handler.
			//Calling this function blocks and suspends the thread until the data becomes available (or we were told to stop the thread).
			tx_event_flags_get(&imu_event_flags_group, IMU_DATA_READY_FLAG | IMU_STOP_THREAD_FLAG, TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

			//if we are ready to read IMU data
			if (actual_events & IMU_DATA_READY_FLAG){
				//Debug
				imu_running = true;

				//Get the data and store in our handler
				IMU_get_data(&imu);
				imu_data[index] = imu.data;

				//Debug
				imu_running = false;
			}

			//We received a "stop" command from the state machine, so cleanup and stop the thread
			if (actual_events & IMU_STOP_THREAD_FLAG){

				//Close the file
				fx_file_close(&imu_file);

				//Terminate thread so it needs to be fully reset to start again
				tx_thread_terminate(&threads[IMU_THREAD].thread);
			}
		}

		imu_writing = true;
		fx_file_write(&imu_file, imu_data, sizeof(IMU_Data) * IMU_NUM_SAMPLES);

		//Wait for the writing to complete before giving up control to prevent unnecessary context switches
		while (imu_writing);

		good_counter = 0;
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

	//Wait for IMU to be ready (poll for falling edge)
	if (IMU_poll_new_data(imu, HAL_MAX_DELAY) == HAL_TIMEOUT){
		return;
	}

	//Need to setup the IMU to send the appropriate data to us. Transmit a "set feature" command to start receiving rotation data.
	//All non-populated bytes are left as default 0.
	uint8_t transmitData[256] = {0};

	//Configure SHTP header (first 4 bytes)
	transmitData[0] = IMU_CONFIGURE_ROTATION_VECTOR_REPORT_LENGTH; //LSB
	transmitData[1] = 0x00; //MSB
	transmitData[2] = IMU_CONTROL_CHANNEL;

	//Indicates we want to start receiving a report
	transmitData[4] = IMU_SET_FEATURE_REPORT_ID;

	//Indicates we want to receive rotation vector reports
	transmitData[5] = IMU_ROTATION_VECTOR_REPORT_ID;

	//Set how often we want to receive data -> 0x0007A120 results in data every 0.5 seconds
	transmitData[9] = IMU_REPORT_INTERVAL_0; //LSB
	transmitData[10] = IMU_REPORT_INTERVAL_1;
	transmitData[11] = IMU_REPORT_INTERVAL_2;
	transmitData[12] = IMU_REPORT_INTERVAL_3; //MSBs

	//Select CS by pulling low and write configuration to the IMU. Add delays to ensure good timing.
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_SPI_Transmit(hspi, transmitData, IMU_CONFIGURE_ROTATION_VECTOR_REPORT_LENGTH, HAL_MAX_DELAY);
	HAL_Delay(1);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	//Deassert the WAKE pin since the chip is now awake and we are done configuring
	HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef IMU_get_data(IMU_HandleTypeDef* imu){

	//receive data buffer
	uint8_t receiveData[256] = {0};

	//Read the header in to a buffer
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Receive(imu->hspi, receiveData, IMU_ROTATION_VECTOR_REPORT_LENGTH, 500);
	HAL_GPIO_WritePin(imu->cs_port, imu->cs_pin, GPIO_PIN_SET);

	//Extract length from first 2 bytes
	uint32_t dataLength = ((receiveData[1] << 8) | receiveData[0]) & IMU_LENGTH_BIT_MASK;

	//Ensure this is the correct channel we're receiving data on and it matches expected length
	if (receiveData[2] == IMU_DATA_CHANNEL && dataLength == IMU_ROTATION_VECTOR_REPORT_LENGTH){

		//Ensure that this is the correct data (timestamp + rotation vector)
	 	if ((receiveData[4] == IMU_TIMESTAMP_REPORT_ID) && (receiveData[9] == IMU_ROTATION_VECTOR_REPORT_ID)){

			//Save data parameters to our struct. We need to combine the MSB and LSB to get the correct 16bit value.
			imu->data.quat_i = receiveData[14] << 8 | receiveData[13];
			imu->data.quat_j = receiveData[16] << 8 | receiveData[15];
			imu->data.quat_k = receiveData[18] << 8 | receiveData[17];
			imu->data.quat_r = receiveData[20] << 8 | receiveData[19];
			imu->data.accurary_rad = receiveData[22] << 8 | receiveData[21];

			good_counter++;
			return HAL_OK;
		}
	}

	return HAL_ERROR;
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

