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

#define DATA_LOG_SLEEP_TIME_SEC 1
#define DATA_LOG_SLEEP_TIME_TICKS tx_s_to_ticks(DATA_LOG_SLEEP_TIME_SEC)

//ThreadX flag for when thread is done writing frame to SD
#define DATA_LOG_COMPLETE_FLAG 0x8

//Frame header parameters
#define KEY_VALUE 0x24
#define SAMPLES_PER_FRAME (IMU_BUFFER_SIZE + DEPTH_BUFFER_SIZE + ECG_BUFFER_SIZE)
#define BYTES_PER_HEADER  (sizeof(Header_Data))
#define BYTES_PER_FRAME (BYTES_PER_HEADER + sizeof(IMU_Data) * IMU_HALF_BUFFER_SIZE + sizeof(DEPTH_Data) * DEPTH_HALF_BUFFER_SIZE + sizeof(ECG_Data) * ECG_HALF_BUFFER_SIZE)

typedef struct __Header_Typedef {

	//Key value to identify start of frame
	uint8_t key_value;

	//Number of bytes per frame
	uint16_t bytes_count;

	//Number of samples per frame
	uint16_t samples_count;

	//Current state of tag
	uint8_t state;

	//Date of samples
	uint8_t datestamp[3];

	//Time of first IMU sample
	uint8_t timestamp[3];

	float latitude;
	float longitude;
	uint8_t gps_lock;

	float state_of_charge;
	float time_to_empty;
	float cell_1_voltage;
	float cell_2_voltage;
	max17320_Reg_Faults bms_faults;


} Header_Data;

void sd_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_DATALOGGING_H_ */
