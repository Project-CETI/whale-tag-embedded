/*
 * DataLogging.h
 *
 *  Created on: Feb 9, 2023
 *      Author: amjad
 */

#ifndef INC_SENSOR_INC_DATALOGGING_H_
#define INC_SENSOR_INC_DATALOGGING_H_

#include "tx_api.h"
#include <stdint.h>
#include "Sensor Inc/BMS.h"

//Data logging flags
#define DATA_LOG_COMPLETE_FLAG			0x8

//Frame header parameters
#define KEY_VALUE						0x24
#define SENSOR_PREAMBLE					0xff
#define SENSOR_PREAMBLE_LEN				4
#define SAMPLES_PER_FRAME				(IMU_BUFFER_SIZE + DEPTH_BUFFER_SIZE + ECG_BUFFER_SIZE)
#define BYTES_PER_HEADER				(sizeof(Header_Data))
#define BYTES_PER_FRAME					(BYTES_PER_HEADER + sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE + sizeof(DEPTH_Data) * DEPTH_HALF_BUFFER_SIZE + sizeof(ECG_Data) * ECG_HALF_BUFFER_SIZE)

//Time interval to create new logging file in minutes
#define RTC_AUDIO_REFRESH_MINS			5
#define RTC_AUDIO_REFRESH_TOL			1

//String indices for file name
#define DATA_FILENAME_HOURS_INDEX		10
#define DATA_FILENAME_MINS_INDEX		12

//Timeout constants
#define LIGHT_THREAD_TIMEOUT			1000

typedef struct __Header_Typedef {

	//Key value to identify start of frame
	uint8_t key_value;

	//Number of bytes per frame
	uint16_t bytes_count;

	//Number of samples per frame
	uint16_t samples_count;

	//State machine state and flag
	uint8_t state;
	ULONG state_flags;

	//Date of samples
	uint8_t datestamp[3];

	//Time of first sample
	uint8_t timestamp[3];

	//GPS data
	float latitude;
	float longitude;
	bool gps_lock;

	//BMS data
	float state_of_charge;
	float time_to_empty;
	float cell_1_voltage;
	float cell_2_voltage;
	max17320_Reg_Alerts bms_alerts;

	//Light sensor data
	uint16_t infrared;
	uint16_t visible;

} Header_Data;

//Main data logging thread to run on RTOS
void sd_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_DATALOGGING_H_ */
