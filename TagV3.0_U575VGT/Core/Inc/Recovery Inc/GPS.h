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
 *
 */

#ifndef INC_RECOVERY_INC_GPS_H_
#define INC_RECOVERY_INC_GPS_H_

#include "main.h"
#include "stm32u5xx_hal_uart.h"
#include <stdbool.h>

#define PACKET_START_CHAR 0x24 //"$"
#define PACKET_END_CHAR 0x0D //"\r" or <CR>

#define UART_TIMEOUT 5000

typedef struct __GPS_TypeDef {

	//UART handler for communication
	UART_HandleTypeDef* huart;

	bool isPosLocked;

	bool isDominica;

	float latitude;
	float longitude;

	uint32_t quality;

	uint16_t timestamp[3]; //0 is hour, 1 is minute, 2 is second

}GPS_HandleTypeDef;

//Initialized and configures the GPS
HAL_StatusTypeDef initialize_gps(UART_HandleTypeDef* huart, GPS_HandleTypeDef* gps);

//Polls for new data from the GPS, parses it and stores it in the GPS struct. Returns true if it has successfully locked onto a positon.
bool read_gps_data(GPS_HandleTypeDef* gps);

//Repeatedly tries to read the GPS data to get a lock. User should call this function if they want a position lock.
void get_gps_lock(GPS_HandleTypeDef* gps);

#endif /* INC_RECOVERY_INC_GPS_H_ */
