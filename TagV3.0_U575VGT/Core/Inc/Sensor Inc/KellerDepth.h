/*
 * KellerDepth.h
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 *      Sensor: Keller Pressure Transmitter series 4LD
 *      Datasheet: keller-druck.com/en/products/pressure-transmitters/oem-pressure-transmitters/series-4ld
 */

#ifndef KELLERDEPTH_H
#define KELLERDEPTH_H

#include "main.h"
#include "stm32u575xx.h"
#include "stm32u5xx_hal.h"
#include "tx_api.h"
#include <stdint.h>
#include <stdbool.h>

// Keller sensor I2C address
#define KELLER_ADDR 	0x40

// Request data address
#define KELLER_REQ		0xAC
#define KELLER_DLEN		5

#define P_MIN			0.0f
#define P_MAX			200.0f

#define TO_DEGC(RAW_IN)	(float)(((RAW_IN >> 4) - 24.0f) * 0.05 - 50.0f)
#define TO_BAR(RAW_IN)	(float)((RAW_IN - 16384) * (P_MAX - P_MIN) / 32768 + P_MIN)

//ThreadX flag bit for when DEPTH data is ready
//#define DEPTH_DATA_READY_FLAG 0x1

//ThreadX flag for stopping the DEPTH (exit data capture)
#define DEPTH_STOP_DATA_THREAD_FLAG 0x2
#define DEPTH_STOP_SD_THREAD_FLAG 0x4

//The number of DEPTH Samples to collect before writing to the SD card. This MUST be an even number.
#define DEPTH_BUFFER_SIZE 250
#define DEPTH_HALF_BUFFER_SIZE (DEPTH_BUFFER_SIZE / 2)

//Frame parameters
#define SAMPLE_DATA_SIZE 10
#define SAMPLES_PER_FRAME 15
#define HEADER_ID 0x24

typedef struct __attribute__ ((packed, scalar_storage_order("little-endian")))
keller_raw_data{
	uint8_t status;
	uint16_t pressure;
}KellerRawData;

typedef struct __Depth_Data_Typedef {

	//Data buffer for I2C data
	uint8_t raw_data[5];

	//raw values read from sensor
	uint8_t status;
	uint16_t raw_pressure;
	uint16_t raw_temp;

	//Pressure in Bar
	float pressure;

	//Temperature in celsius
	float temperature;
} DEPTH_Data;

typedef struct __Keller_Depth_TypeDef
{

	//I2C handler for communication
	I2C_HandleTypeDef *i2c_handler;

} Keller_HandleTypedef;

//Function prototypes
void depth_init(I2C_HandleTypeDef *hi2c_device, Keller_HandleTypedef *keller_sensor);
HAL_StatusTypeDef depth_get_data(Keller_HandleTypedef *keller_sensor, uint8_t buffer_half);

//Main Depth thread
void depth_thread_entry(ULONG thread_input);

#endif /* KELLERDEPTH_H */
