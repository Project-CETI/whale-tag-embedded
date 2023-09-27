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

#define DATA_LOG_SLEEP_TIME_SEC 1
#define DATA_LOG_SLEEP_TIME_TICKS tx_s_to_ticks(DATA_LOG_SLEEP_TIME_SEC)

//ThreadX flag for when thread is done writing frame to SD
#define DATA_LOG_COMPLETE_FLAG 0x1

//Frame header parameters
#define KEY_VALUE 0x24
#define SAMPLES_PER_FRAME 15
#define BYTES_PER_FRAME (12*SAMPLES_PER_FRAME)

typedef struct __Header_Typedef {

	//Key value to identify start of frame
	uint8_t key_value;

	//Number of samples per frame
	uint8_t samples_count;

	//Number of bytes per frame
	uint8_t bytes_count;

	//Date of samples
	uint8_t datestamp[3];

	//Time of first IMU sample
	uint8_t timestamp[3];

} Header_Data;

void sd_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_DATALOGGING_H_ */
