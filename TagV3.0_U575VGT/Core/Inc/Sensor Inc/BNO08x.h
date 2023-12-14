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
#include "Lib Inc/timing.h"
#include <stdint.h>
#include <stdbool.h>

//Channels
#define IMU_CONTROL_CHANNEL 2
#define IMU_DATA_CHANNEL 3

//REPORT IDs
#define IMU_SET_FEATURE_REPORT_ID		0XFD //report ID for requesting sensor records/data
#define IMU_TIMESTAMP_REPORT_ID			0xFB //report ID for timestamps that are put infront of the data
#define IMU_GET_PRODUCT_REPORT_ID		0xF9 //report ID for product ID
#define IMU_G_ROTATION_VECTOR_REPORT_ID	0x08 //report ID for the quaternion rotation components
#define IMU_ROTATION_VECTOR_REPORT_ID	0x05 //report ID for the quaternion rotation components with accuracy
#define IMU_ACCELEROMETER_REPORT_ID		0x01 //report ID for the accelerometer data
#define IMU_GYROSCOPE_REPORT_ID			0x02 //report ID for the gyroscope data
#define IMU_MAGNETOMETER_REPORT_ID		0x03 //report ID for the magnetometer data

//Report interval from MSByte to LSByte
#define IMU_REPORT_INTERVAL_3			0x00
#define IMU_REPORT_INTERVAL_2			0x00
#define IMU_REPORT_INTERVAL_1			0x4E
#define IMU_REPORT_INTERVAL_0			0x20

//Bit mask for extracting length from the first two bytes of the header
#define IMU_LENGTH_BIT_MASK				0x7FFF //neglects MSB

#define IMU_CONFIG_REPORT_LEN			0x15
#define IMU_PRODUCT_CONFIG_REPORT_LEN	0x06
#define IMU_ROTATION_VECTOR_REPORT_LEN	23
#define IMU_SHTP_HEADER_LEN				4
#define IMU_REPORT_HEADER_LEN			4

//Timeout values
#define IMU_NEW_DATA_TIMEOUT_MS 		2000
#define IMU_DUMMY_PACKET_TIMEOUT_MS		500
#define IMU_FLAG_WAIT_TIMEOUT			tx_s_to_ticks(10)
#define IMU_MAX_ERROR_COUNT				50

//ThreadX status flags
#define IMU_DATA_READY_FLAG				0x1
#define IMU_STOP_SD_THREAD_FLAG			0x2
#define IMU_STOP_DATA_THREAD_FLAG		0x4
#define IMU_HALF_BUFFER_FLAG			0x8

//UART debugging flags
#define IMU_UNIT_TEST_FLAG				0x10
#define IMU_UNIT_TEST_DONE_FLAG			0x12
#define IMU_CMD_FLAG					0x14

//Commands for IMU
#define IMU_GET_SAMPLES_CMD				0x1
#define IMU_NUM_SAMPLES					10
#define IMU_RESET_CMD					0x2
#define IMU_CONFIG_CMD					0x3

//Buffer constants
#define IMU_BUFFER_SIZE					250 // number of samples before writing to SD card (must be even number)
#define IMU_HALF_BUFFER_SIZE			(IMU_BUFFER_SIZE / 2)
#define IMU_RECEIVE_BUFFER_MAX_LEN		300

//The useful number of data bytes for each kind of report (quaternion vs 3 axis measurement)
#define IMU_QUAT_USEFUL_BYTES			10
#define IMU_Q_QUAT_USEFUL_BYTES			8
#define IMU_3_AXIS_USEFUL_BYTES			6

//MS timeout for SPI reads
#define IMU_SPI_READ_TIMEOUT_MS			10
#define IMU_SPI_STARTUP_TIMEOUT_MS		5000

// 8-bit to 16-bit conversion
#define TO_16_BIT(b1, b2)				((uint16_t)(b2 << 8) | (uint16_t)b1)

// struct for imu data
typedef struct __IMU_Data_Typedef {

	// report id
	uint8_t report_id;

	// report data
	uint8_t raw_data[IMU_QUAT_USEFUL_BYTES];

} IMU_Data;

// struct for imu shtp header
typedef struct __IMU_Header_Typedef {
	uint8_t report_len_lsb;
	uint8_t report_len_msb;
	uint8_t channel;
	uint8_t seq;
} IMU_Header;

// struct for imu configuration
typedef struct __IMU_SensorConfig_Typedef {

	// sensor header config
	IMU_Header header;

	// sensor feature config
	uint8_t feature_report_id;
	uint8_t report_id;
	uint8_t feature_flags;
	uint8_t sens_lsb;
	uint8_t sens_msb;
	uint8_t report_interval_lsb;
	uint8_t report_interval_1;
	uint8_t report_interval_2;
	uint8_t report_interval_msb;
	uint8_t batch_interval_lsb;
	uint8_t batch_interval_1;
	uint8_t batch_interval_2;
	uint8_t batch_interval_msb;

	// sensor specific configuration
	uint32_t sensor_config;

} IMU_SensorConfig;

// struct for imu product id request
typedef struct __IMU_ProductIDConfig_Typedef {
	// sensor header config
	IMU_Header header;

	// sensor product id request
	uint8_t feature_report_id;
	uint8_t reserved;

} IMU_ProductIDConfig;

// struct for imu
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

	//GPIO info for the reset line
	GPIO_TypeDef* reset_port;
	uint16_t reset_pin;

} IMU_HandleTypeDef;

void imu_init(SPI_HandleTypeDef* hspi, IMU_HandleTypeDef* imu);
void imu_reset(IMU_HandleTypeDef* imu);
HAL_StatusTypeDef imu_configure_reports(IMU_HandleTypeDef* imu, uint8_t report_id);
HAL_StatusTypeDef imu_get_product_id(IMU_HandleTypeDef* imu);
void imu_configure_startup(IMU_HandleTypeDef* imu);
HAL_StatusTypeDef imu_get_data(IMU_HandleTypeDef* imu, uint8_t buffer_half);

//Main IMU thread to run on RTOS
void imu_thread_entry(ULONG thread_input);

#endif /* INC_BNO08X_H_ */
