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
#define IMU_TIMESTAMP_REPORT_ID 0xFB //report ID for timestamps that are put infront of the data
#define IMU_ROTATION_VECTOR_REPORT_ID 0x05 //report ID for the quaternion rotation components
#define IMU_ACCELEROMETER_REPORT_ID 0x01 //report ID for the accelerometer data
#define IMU_GYROSCOPE_REPORT_ID 0x02 //report ID for the gyroscope data
#define IMU_MAGNETOMETER_REPORT_ID 0x03 //report ID for the magnetometer data

//Report interval from MSByte to LSByte
#define IMU_REPORT_INTERVAL_3 0x00
#define IMU_REPORT_INTERVAL_2 0x00
#define IMU_REPORT_INTERVAL_1 0x4E
#define IMU_REPORT_INTERVAL_0 0x20

//Bit mask for extracting length from the first two bytes of the header
#define IMU_LENGTH_BIT_MASK 0x7FFF //neglects MSB

//The length of data (including header) of the message to configure the rotation vector reports
#define IMU_CONFIGURE_REPORT_LENGTH 21

//The length of the rotation vector data received from the IMU
#define IMU_ROTATION_VECTOR_REPORT_LENGTH 23

//the length of the SHTP header
#define IMU_SHTP_HEADER_LENGTH 4

//Timeout values
#define IMU_NEW_DATA_TIMEOUT_MS 2000
#define IMU_DUMMY_PACKET_TIMEOUT_MS 500

//ThreadX flag bit for when IMU data is ready
#define IMU_DATA_READY_FLAG 0x1

//ThreadX flag for stopping the IMU (exit data capture)
#define IMU_STOP_DATA_THREAD_FLAG 0x2
#define IMU_STOP_SD_THREAD_FLAG 0x4

//The number of IMU Samples to collect before writing to the SD card. This MUST be an even number.
#define IMU_BUFFER_SIZE 250
#define IMU_HALF_BUFFER_SIZE (IMU_BUFFER_SIZE / 2)

//The useful number of data bytes for each kind of report (quaternion vs 3 axis measurement)
#define IMU_QUAT_USEFUL_BYTES 10
#define IMU_3_AXIS_USEFUL_BYTES 6

//Number of bytes for
#define SAMPLE_DATA_SIZE 10
#define SAMPLES_PER_FRAME 15
#define BYTES_PER_FRAME 165

//MS timeout for SPI reads
#define IMU_SPI_READ_TIMEOUT 10

typedef struct __IMU_Header_Typedef {
	//Date and time of data
	uint16_t timestamp[3];
	uint16_t datestamp[3];

	//Samples in frame
	uint8_t num_samples;

	//Bytes in frame
	uint8_t num_bytes;

} IMU_Header;

//IMU typedef definition for useful data holding (of various types, like quaternion, accel, gyro, magnetometer
typedef struct __IMU_Data_Typedef {

	//Data header for keeping metadata (date, time, samples in frame, bytes in frame)
	IMU_Header data_header;

	//A header to signify which type of data this is (Matches the macro defined report IDs above)
	uint8_t data_id;

	/*
	 * The data for the IMU is only stored and written to the SD card as raw data.
	 *
	 * This is because the IMU is not useful inside of the system/tag (we dont do anything inside the tag with it, compared to say the BMS data.)
	 *
	 * So, to save processing power, we just store the raw bytes and handle everything else is post processing.
	 *
	 * The data is stored as follows (from index 0 to 9):
	 *
	 * X_lsb, X_msb, Y_lsb, Y_msb, Z_lsb, Z_msb, Real_lsb, Real_msb, Accuracy_lsb, Accuracy_msb
	 *
	 * Where X Y and Z represent the axes for any of the given reports, and "Real" and "Accuracy" are used only by the quaternion report.
	 *
	 * E.g., X_lsb and X_msb represent the acceleration in the X direction if it is an accelerometer report.
	 */
	uint8_t raw_data[SAMPLES_PER_FRAME][SAMPLE_DATA_SIZE];
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

} IMU_HandleTypeDef;


//Function prototypes
void IMU_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu);
HAL_StatusTypeDef IMU_get_data(IMU_HandleTypeDef* imu, uint8_t buffer_half);

//Main IMU thread to run on RTOS
void imu_thread_entry(ULONG thread_input);

#endif /* INC_BNO08X_H_ */
