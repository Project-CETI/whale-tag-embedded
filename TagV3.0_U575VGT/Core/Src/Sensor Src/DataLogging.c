/*
 * DataLogging.c
 *
 *  Created on: Feb 9, 2023
 *      Author: amjad
 */

#include "Sensor Inc/DataLogging.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/ECG.h"
#include "Sensor Inc/KellerDepth.h"
#include "Sensor Inc/RTC.h"
#include "Sensor Inc/LightSensor.h"
#include "Sensor Inc/BMS.h"
#include "Lib Inc/threads.h"
#include "Lib Inc/state_machine.h"
#include "app_filex.h"
#include <stdio.h>
#include <stdbool.h>

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

//State variable
extern State state;

//RTC date and time
extern RTC_TimeTypeDef eTime;
extern RTC_DateTypeDef eDate;
extern RTC_TimeTypeDef sTime;

//BMS data
extern MAX17320_HandleTypeDef bms;

//GPS data
extern GPS_HandleTypeDef gps;
extern GPS_Data gps_data;

//Light sensor data
extern LightSensorHandleTypedef light_sensor;

//FileX variables
FX_FILE data_file = {};
extern FX_MEDIA sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//ThreadX flags for data logging
TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

//ThreadX flags for RTC
extern TX_EVENT_FLAGS_GROUP rtc_event_flags_group;

//ThreadX flags for light sensor
extern TX_EVENT_FLAGS_GROUP light_event_flags_group;

//ThreadX flags for IMU sensor
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_MUTEX imu_first_half_mutex;
extern TX_MUTEX imu_second_half_mutex;

//ThreadX flags for DEPTH sensor
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_MUTEX depth_first_half_mutex;
extern TX_MUTEX depth_second_half_mutex;

//ThreadX flags for ECG sensor
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_MUTEX ecg_first_half_mutex;
extern TX_MUTEX ecg_second_half_mutex;

//ThreadX flags for state machine
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

//State machine flags
extern ULONG actual_state_flags;

//Arrays for holding sensor data. The buffer is split in half and shared with the IMU thread.
extern IMU_Data imu_data[2][IMU_HALF_BUFFER_SIZE];
extern DEPTH_Data depth_data[2][DEPTH_HALF_BUFFER_SIZE];
extern ECG_Data ecg_data[2][ECG_HALF_BUFFER_SIZE];

//RTC old time for tracking file names
RTC_TimeTypeDef old_eTime = {0};

//Preamble bytes to indicate end of sensor data in each frame
uint8_t preamble_bytes[SENSOR_PREAMBLE_LEN] = {SENSOR_PREAMBLE, SENSOR_PREAMBLE, SENSOR_PREAMBLE, SENSOR_PREAMBLE};

//Status variable to indicate thread is writing to SD card
bool sd_writing = false;

//Callback for completed SD card write
void SDWriteComplete_Callback(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	sd_writing = false;
}

void sd_thread_entry(ULONG thread_input) {
	ULONG actual_flags = 0;
	char data_file_name[30] = {0};
	sprintf(data_file_name, "data_%d_%d_%d_%d_%d.bin", eDate.Month, eDate.Date, eTime.Hours, eTime.Minutes, eTime.Seconds);

	//Initialize header
	Header_Data header = {0};
	header.key_value = KEY_VALUE;
	header.bytes_count = BYTES_PER_FRAME;
	header.samples_count = SAMPLES_PER_FRAME;

	//Create our binary file for dumping sensor data
	UINT fx_result = FX_ACCESS_ERROR;
	fx_result = fx_file_create(&sdio_disk, data_file_name);
	if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
		Error_Handler();
	}

	//Close file in case already opened
	fx_result = fx_file_close(&data_file);

	//Open binary file
	fx_result = fx_file_open(&sdio_disk, &data_file, data_file_name, FX_OPEN_FOR_WRITE);
	if (fx_result != FX_SUCCESS) {
		Error_Handler();
	}

	//Setup "write complete" callback function
	fx_result = fx_file_write_notify_set(&data_file, SDWriteComplete_Callback);
	if (fx_result != FX_SUCCESS) {
		Error_Handler();
	}

	//Create the events flag
	tx_event_flags_create(&data_log_event_flags_group, "Data Logging Event Flags");

	//Dummy write at start since first write is usually slow
	sd_writing = true;
	fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
	fx_result |= fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
	if (fx_result == FX_NO_MORE_SPACE) {
		HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
		tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
	}
	while (sd_writing);

	//Wait for RTC thread to acquire GPS timestamp and initialize
	tx_event_flags_get(&rtc_event_flags_group, RTC_INIT_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

	//Sequentially save buffers for all sensors
	while (1) {
		actual_flags = 0;

		// check time to create new file
		if (abs(eTime.Minutes - sTime.Minutes) % RTC_REFRESH_MINS == 0) {

			//Only create new file if minutes is different or minutes is same and hours is different
			if (old_eTime.Minutes != eTime.Minutes) {

				//Create new data file name
				sprintf(data_file_name, "data_%d_%d_%d_%d_%d.bin", eDate.Month, eDate.Date, eTime.Hours, eTime.Minutes, eTime.Seconds);
				old_eTime = eTime;

				//Close previous data file
				fx_result = fx_file_close(&data_file);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Create new data file
				fx_result = fx_file_create(&sdio_disk, data_file_name);
				if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
					Error_Handler();
				}

				//Open new data file for writing
				fx_result = fx_file_open(&sdio_disk, &data_file, data_file_name, FX_OPEN_FOR_WRITE);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Set our "write complete" callback function
				fx_result = fx_file_write_notify_set(&data_file, SDWriteComplete_Callback);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}
			}
		}

		// write preamble before header
		sd_writing = true;
		fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
		if (fx_result == FX_NO_MORE_SPACE) {
			HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
			tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
		}
		while (sd_writing);

		//Save current state of tag
		header.state = state;
		header.state_flags = actual_flags;

		//Update header date and time
		header.datestamp[0] = eDate.Year;
		header.datestamp[1] = eDate.Month;
		header.datestamp[2] = eDate.Date;
		header.timestamp[0] = eTime.Hours;
		header.timestamp[1] = eTime.Minutes;
		header.timestamp[2] = eTime.Seconds;

		//Save GPS coordinates of tag if GPS locked
		if (gps.is_pos_locked) {
			header.latitude = gps_data.latitude;
			header.longitude = gps_data.longitude;
			header.gps_lock = true;
		}
		else {
			header.latitude = 0;
			header.longitude = 0;
			header.gps_lock = false;
		}

		header.state_of_charge = bms.state_of_charge;
		header.time_to_empty = bms.time_to_empty;
		header.cell_1_voltage = bms.cell_1_voltage;
		header.cell_2_voltage = bms.cell_2_voltage;
		header.bms_alerts = bms.alerts;

		header.infrared = light_sensor.data.infrared;
		header.visible = light_sensor.data.visible;

		sd_writing = true;
		fx_result = fx_file_write(&data_file, &header, sizeof(Header_Data));
		fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
		if (fx_result == FX_NO_MORE_SPACE) {
			HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
			tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
		}
		while (sd_writing);

		//Wait for the sensor threads to be done filling the first half of buffer
		tx_event_flags_get(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG | IMU_STOP_DATA_THREAD_FLAG | IMU_STOP_SD_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);

		if (actual_flags & IMU_STOP_SD_THREAD_FLAG) {
			//Terminate the IMU thread
			tx_thread_terminate(&threads[IMU_THREAD].thread);
			tx_event_flags_delete(&imu_event_flags_group);
		}
		if (!(actual_flags & IMU_STOP_DATA_THREAD_FLAG)) {
			tx_mutex_get(&imu_first_half_mutex, TX_WAIT_FOREVER);
			sd_writing = true;
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			fx_result = fx_file_write(&data_file, &imu_data[0], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			if (fx_result == FX_NO_MORE_SPACE) {
				HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
				tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
			}
			while (sd_writing);
			tx_mutex_put(&imu_first_half_mutex);
		}

		tx_event_flags_get(&depth_event_flags_group, DEPTH_HALF_BUFFER_FLAG | DEPTH_STOP_DATA_THREAD_FLAG | DEPTH_STOP_SD_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);

		if (actual_flags & DEPTH_STOP_SD_THREAD_FLAG) {
			//Terminate the keller depth thread
			tx_thread_terminate(&threads[DEPTH_THREAD].thread);
			tx_event_flags_delete(&depth_event_flags_group);
		}
		if (!(actual_flags & DEPTH_STOP_DATA_THREAD_FLAG)) {
			tx_mutex_get(&depth_first_half_mutex, TX_WAIT_FOREVER);
			sd_writing = true;
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			fx_result = fx_file_write(&data_file, &depth_data[0], sizeof(DEPTH_Data) * DEPTH_HALF_BUFFER_SIZE);
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			if (fx_result == FX_NO_MORE_SPACE) {
				HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
				tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
			}
			while (sd_writing);
			tx_mutex_put(&depth_first_half_mutex);
		}

		tx_event_flags_get(&ecg_event_flags_group, ECG_HALF_BUFFER_FLAG | ECG_STOP_DATA_THREAD_FLAG | ECG_STOP_SD_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);

		if (actual_flags & ECG_STOP_SD_THREAD_FLAG) {
			//Terminate the ECG thread
			tx_thread_terminate(&threads[ECG_THREAD].thread);
			tx_event_flags_delete(&ecg_event_flags_group);
		}
		if (!(actual_flags & ECG_STOP_DATA_THREAD_FLAG)) {
			tx_mutex_get(&ecg_first_half_mutex, TX_WAIT_FOREVER);
			sd_writing = true;
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			fx_result = fx_file_write(&data_file, &ecg_data[0], sizeof(ECG_Data) * ECG_HALF_BUFFER_SIZE);
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			if (fx_result == FX_NO_MORE_SPACE) {
				HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
				tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
			}
			while (sd_writing);
			tx_mutex_put(&ecg_first_half_mutex);
		}

		tx_event_flags_set(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR);

		//Update header date and time
		header.state = state;
		header.state_flags = actual_flags;

		//Update header date and time
		header.datestamp[0] = eDate.Year;
		header.datestamp[1] = eDate.Month;
		header.datestamp[2] = eDate.Date;
		header.timestamp[0] = eTime.Hours;
		header.timestamp[1] = eTime.Minutes;
		header.timestamp[2] = eTime.Seconds;

		//Save GPS coordinates of tag if GPS locked
		if (gps.is_pos_locked) {
			header.latitude = gps_data.latitude;
			header.longitude = gps_data.longitude;
			header.gps_lock = true;
		}
		else {
			header.latitude = 0;
			header.longitude = 0;
			header.gps_lock = false;
		}

		header.state_of_charge = bms.state_of_charge;
		header.time_to_empty = bms.time_to_empty;
		header.cell_1_voltage = bms.cell_1_voltage;
		header.cell_2_voltage = bms.cell_2_voltage;
		header.bms_alerts = bms.alerts;

		header.infrared = light_sensor.data.infrared;
		header.visible = light_sensor.data.visible;

		sd_writing = true;
		fx_result = fx_file_write(&data_file, &header, sizeof(Header_Data));
		fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
		if (fx_result == FX_NO_MORE_SPACE) {
			HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
			tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
		}
		while (sd_writing);

		tx_event_flags_get(&imu_event_flags_group, IMU_HALF_BUFFER_FLAG | IMU_STOP_DATA_THREAD_FLAG | IMU_STOP_SD_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);

		if (actual_flags & IMU_STOP_SD_THREAD_FLAG) {
			//Terminate the IMU thread
			tx_thread_terminate(&threads[IMU_THREAD].thread);
			tx_event_flags_delete(&imu_event_flags_group);
		}
		if (!(actual_flags & IMU_STOP_DATA_THREAD_FLAG)) {
			tx_mutex_get(&imu_second_half_mutex, TX_WAIT_FOREVER);
			sd_writing = true;
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			fx_result = fx_file_write(&data_file, &imu_data[1], sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE);
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			if (fx_result == FX_NO_MORE_SPACE) {
				HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
				tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
			}
			while (sd_writing);
			tx_mutex_put(&imu_second_half_mutex);
		}

		tx_event_flags_get(&depth_event_flags_group, DEPTH_HALF_BUFFER_FLAG | DEPTH_STOP_DATA_THREAD_FLAG | DEPTH_STOP_SD_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);

		if (actual_flags & DEPTH_STOP_SD_THREAD_FLAG) {
			//Terminate the keller depth thread
			tx_thread_terminate(&threads[DEPTH_THREAD].thread);
			tx_event_flags_delete(&depth_event_flags_group);
		}
		if (!(actual_flags & DEPTH_STOP_DATA_THREAD_FLAG)) {
			tx_mutex_get(&depth_second_half_mutex, TX_WAIT_FOREVER);
			sd_writing = true;
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			fx_result = fx_file_write(&data_file, &depth_data[1], sizeof(DEPTH_Data) * DEPTH_HALF_BUFFER_SIZE);
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			if (fx_result == FX_NO_MORE_SPACE) {
				HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
				tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
			}
			while (sd_writing);
			tx_mutex_put(&depth_second_half_mutex);
		}

		tx_event_flags_get(&ecg_event_flags_group, ECG_HALF_BUFFER_FLAG | ECG_STOP_DATA_THREAD_FLAG | ECG_STOP_SD_THREAD_FLAG, TX_OR, &actual_flags, TX_WAIT_FOREVER);

		if (actual_flags & ECG_STOP_SD_THREAD_FLAG) {
			//Terminate the ECG thread
			tx_thread_terminate(&threads[ECG_THREAD].thread);
			tx_event_flags_delete(&ecg_event_flags_group);
		}
		if (!(actual_flags & ECG_STOP_DATA_THREAD_FLAG)) {
			tx_mutex_get(&ecg_second_half_mutex, TX_WAIT_FOREVER);
			sd_writing = true;
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			fx_result = fx_file_write(&data_file, &ecg_data[1], sizeof(ECG_Data) * ECG_HALF_BUFFER_SIZE);
			fx_result = fx_file_write(&data_file, &preamble_bytes[0], SENSOR_PREAMBLE_LEN);
			if (fx_result == FX_NO_MORE_SPACE) {
				HAL_GPIO_WritePin(GPIOB, DIAG_LED4_Pin, GPIO_PIN_SET);
				tx_event_flags_set(&state_machine_event_flags_group, STATE_SD_CARD_FULL_FLAG, TX_OR);
			}
			while (sd_writing);
			tx_mutex_put(&ecg_second_half_mutex);
		}

		tx_event_flags_set(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR);
	}
}
