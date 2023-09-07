/*
 * GpsGeofencing.h
 *
 *  Created on: Aug 9, 2023
 *      Author: Kaveet
 *
 * The thread which occasionally reads the GPS data to try and get a GPS lock. If the GPS location is outside of dominica, we signal this to the state machine and enter recovery mode.
 *
 * This thread re-uses the functions inside of Recovery Inc/GPS.h for simplicity of code.
 */

#ifndef INC_SENSOR_INC_GPSGEOFENCING_H_
#define INC_SENSOR_INC_GPSGEOFENCING_H_

#include "Lib Inc/timing.h"
#include "Recovery Inc/GPS.h"
#include "tx_api.h"

#define GPS_GEOFENCING_SLEEP_PERIOD_SECONDS 10
#define GPS_GEOFENCING_SLEEP_PERIOD_TICKS (tx_s_to_ticks(GPS_GEOFENCING_SLEEP_PERIOD_SECONDS))

void gps_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_GPSGEOFENCING_H_ */
