/*
 * GpsGeofencing.c
 *
 *  Created on: Aug 9, 2023
 *      Author: Kaveet
 */

#include "Sensor Inc/GpsGeofencing.h"
#include "main.h"
#include "Lib Inc/state_machine.h"

//External variables defined in other C files (uart handler and state machine event flag)
extern UART_HandleTypeDef huart3;
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

void gps_thread_entry(ULONG thread_input){

	//Initialize GPS struct
	GPS_HandleTypeDef gps;
	initialize_gps(&huart3, &gps);

	//Thread infinite loop entry
	while (1){

		//Create struct to hold gps data
		GPS_Data gps_data;

		//Try to get a GPS lock
		if (get_gps_lock(&gps, &gps_data)){

			//If the location is NOT in dominica
			if (!gps_data.is_dominica){

				//Signal the state machine to enter recovery mode
				tx_event_flags_set(&state_machine_event_flags_group, STATE_GPS_FLAG, TX_OR);
			}
		}

		//Go to sleep and try again later
		tx_thread_sleep(GPS_GEOFENCING_SLEEP_PERIOD_TICKS);
	}

}
