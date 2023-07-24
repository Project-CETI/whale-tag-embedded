/*
 * BNO08x.h
 *
 *  Created on: Feb 8, 2023
 *      Author: Kaveet Grewal
 *      Sensor: BNO08x
 *      Datasheet(s): https://www.ceva-dsp.com/wp-content/uploads/2019/10/BNO080_085-Datasheet.pdf
 *      			  https://www.ceva-dsp.com/wp-content/uploads/2019/10/Sensor-Hub-Transport-Protocol.pdf
 *      			  https://www.ceva-dsp.com/wp-content/uploads/2019/10/SH-2-Reference-Manual.pdf
 *
 * 	This file handles the SPI interface between the STM board and the IMU (BNO08x).
 *
 * 	The STM configures the IMU by requesting it to send rotation reports every interval. The IMU fires and interrupt when it has data ready.
 *
 * 	The IMU follows the Sensor Hub Transfer Protocol (SHTP) on top of SPI. Documentation on it can be found at the links above.
 */
#ifndef INC_BNO08X_H_
#define INC_BNO08X_H_

//Includes
#include "main.h"
#include "stm32u575xx.h"
#include "tx_api.h"
#include <stdint.h>
#include <stdbool.h>

//Useful defines

//Channels
#define IMU_CONTROL_CHANNEL 2
#define IMU_DATA_CHANNEL 3

//REPORT IDs
#define IMU_SET_FEATURE_REPORT_ID 0XFD //report ID for requesting sensor records/data
#define IMU_ROTATION_VECTOR_REPORT_ID 0x05 //report ID for the quaternion rotation components
#define IMU_TIMESTAMP_REPORT_ID 0xFB //report ID for timestamps that are put infront of the data

//Report interval from MSByte to LSByte
#define IMU_REPORT_INTERVAL_3 0x00
#define IMU_REPORT_INTERVAL_2 0x07
#define IMU_REPORT_INTERVAL_1 0xA1
#define IMU_REPORT_INTERVAL_0 0x20

//Bit mask for extracting length from the first two bytes of the header
#define IMU_LENGTH_BIT_MASK 0x7FFF //neglects MSB

//The length of data (including header) of the message to configure the rotation vector reports
#define IMU_CONFIGURE_ROTATION_VECTOR_REPORT_LENGTH 21

//The length of the rotation vector data received from the IMU
#define IMU_ROTATION_VECTOR_REPORT_LENGTH 23

//the length of the SHTP header
#define IMU_SHTP_HEADER_LENGTH 4

//Timeout values
#define IMU_NEW_DATA_TIMEOUT_MS 2000
#define IMU_DUMMY_PACKET_TIMEOUT_MS 500

//ThreadX flag bit for when IMU data is ready
#define IMU_DATA_READY_FLAG 0x1

//The number of IMU Samples to collect before writing to the SD card
#define IMU_NUM_SAMPLES 10

//IMU typedef definition for useful data variables
typedef struct __IMU_Data_Typedef {

	//Data variables - rotation unit quaternions
	// real, i, j and k components
	uint16_t quat_r;
	uint16_t quat_i;
	uint16_t quat_j;
	uint16_t quat_k;
	uint16_t accurary_rad;

} IMU_Data;

//IMU typedef definition for useful variables
typedef struct __IMU_Typedef{

	//SPI handler for communication
	SPI_HandleTypeDef* hspi;

	//GPIO info for chip select
	GPIO_TypeDef* cs_port;
	uint16_t cs_pin;

	//GPIO info for the interrupt line
	GPIO_TypeDef* int_port;
	uint16_t int_pin;

	//GPIO info for the wakeup line
	GPIO_TypeDef* wake_port;
	uint16_t wake_pin;

	//Data struct
	IMU_Data data;

} IMU_HandleTypeDef;


//Function prototypes
void IMU_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu);
HAL_StatusTypeDef IMU_get_data(IMU_HandleTypeDef* imu);

//Main IMU thread to run on RTOS
void IMU_thread_entry(ULONG thread_input);

#endif /* INC_BNO08X_H_ */
