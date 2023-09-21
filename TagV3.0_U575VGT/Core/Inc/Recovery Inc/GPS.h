/*
 * GPS.h
 *
 *  Created on: Jun 22, 2023
 *      Author: Kaveet
 *
 * This file contains the driver for the GPS module used to find the position (lat/long) for the APRS recovery mode.
 *
 * GPS is the NEO M9N. See datasheet: https://cdn.sparkfun.com/assets/learn_tutorials/1/1/0/2/NEO-M9N-00B_DataSheet_UBX-19014285.pdf
 *
 * More info is in the Integration Manual and Interface Description Documents.
 *
 * Integration Manual: https://content.u-blox.com/sites/default/files/NEO-M9N_Integrationmanual_UBX-19014286.pdf
 * Interface Description: https://content.u-blox.com/sites/default/files/u-blox-M9-SPG-4.04_InterfaceDescription_UBX-21022436.pdf
 */

#ifndef INC_RECOVERY_INC_GPS_H_
#define INC_RECOVERY_INC_GPS_H_

#include "main.h"
#include "stm32u5xx_hal_uart.h"
#include <stdbool.h>

#define GPS_PACKET_START_CHAR 0x24 //"$"
#define GPS_PACKET_END_CHAR 0x0D //"\r" or <CR>

#define GPS_UART_TIMEOUT 5000
#define GPS_TRY_LOCK_TIMEOUT 5000

#define GPS_SIMULATION true
#define GPS_SIM_LAT 42.36326
#define GPS_SIM_LON -71.12650

#define DEFAULT_LAT 15.31383
#define DEFAULT_LON -61.30075

#define DOMINICA_LAT_BOUNDARY 17.71468

typedef enum __GPS_MESSAGE_TYPES {
	GPS_SIM = 0,
	GPS_GLL = 1,
	GPS_GGA = 2,
	GPS_RMC = 3,
	GPS_NUM_MSG_TYPES //Should always be the last element in the enum. Represents the number of message types. If you need to add a new type, put it before this element.
}GPS_MsgTypes;

typedef struct __GPS_Data {

	float latitude;
	float longitude;

	uint32_t quality;

	uint16_t timestamp[3]; //0 is hour, 1 is minute, 2 is second
	uint16_t datestamp[3]; //0 is year, 1 is month, 2 is date

	bool is_dominica;

	bool is_valid_data;

	GPS_MsgTypes msg_type;

}GPS_Data;

typedef struct __GPS_TypeDef {

	//UART handler for communication
	UART_HandleTypeDef* huart;

	bool is_pos_locked;

	GPS_Data data[GPS_NUM_MSG_TYPES];

}GPS_HandleTypeDef;

//Initialized and configures the GPS
HAL_StatusTypeDef initialize_gps(UART_HandleTypeDef* huart, GPS_HandleTypeDef* gps);

//Polls for new data from the GPS, parses it and stores it in the GPS struct. Returns true if it has successfully locked onto a positon.
bool read_gps_data(GPS_HandleTypeDef* gps);

//Repeatedly tries to read the GPS data to get a lock. User should call this function if they want a position lock.
bool get_gps_lock(GPS_HandleTypeDef* gps, GPS_Data* gps_data);

//Checks if a GPS location is in dominica based on the latitude and longitude
bool is_in_dominica(float latitude, float longitude);

#endif /* INC_RECOVERY_INC_GPS_H_ */
