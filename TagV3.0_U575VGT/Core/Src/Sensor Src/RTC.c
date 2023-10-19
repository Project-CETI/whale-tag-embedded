/*
 * RTC.c
 *
 *  Created on: Sep 18, 2023
 *      Author: kevinma
 */

#include "Sensor Inc/RTC.h"
#include "Sensor Inc/GpsGeofencing.h"
#include "Sensor Inc/DataLogging.h"
#include "Lib Inc/state_machine.h"

//External variables
extern RTC_HandleTypeDef hrtc;
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

extern State state;

//GPS structs to check for GPS lock
extern GPS_HandleTypeDef gps;
extern GPS_Data gps_data;

//Create RTC structs to track date and time
RTC_TimeTypeDef sTime = {0};
RTC_DateTypeDef sDate = {0};
RTC_TimeTypeDef eTime = {0};
RTC_DateTypeDef eDate = {0};

//ThreadX flags for rtc
TX_EVENT_FLAGS_GROUP rtc_event_flags_group;

//Time interval variable
uint8_t new_audio_file;

void rtc_thread_entry(ULONG thread_input) {

	bool first_pass = true;

	// create events flag
	tx_event_flags_create(&rtc_event_flags_group, "RTC Event Flags");

	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	// wait for gps lock
	while (!gps.is_pos_locked && sTime.Minutes < RTC_GPS_TIMEOUT_MINS) {
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
	}

	RTC_Init(&sTime, &sDate);
	RTC_Init(&eTime, &eDate);

	tx_event_flags_set(&rtc_event_flags_group, RTC_INIT_FLAG, TX_OR);

	while (1) {

		ULONG actual_flags;

		//Get time from RTC (must query time THEN date to update registers)
		HAL_RTC_GetTime(&hrtc, &eTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &eDate, RTC_FORMAT_BIN);

		//Check if max operating duration passed
		if (abs(eTime.Minutes - sTime.Minutes) > RTC_SHUTDOWN_LIMIT_MINS && abs(eTime.Hours - sTime.Hours) > RTC_SHUTDOWN_LIMIT_HOUR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_TIMEOUT_FLAG, TX_OR);
		}

		if (abs(eTime.Minutes - sTime.Minutes) % RTC_AUDIO_REFRESH_MINS <= RTC_AUDIO_REFRESH_TOL) {
			new_audio_file = 1;
		}
		else {
			new_audio_file = 0;
		}

		//Sleep and repeat the process once woken up
		if (!first_pass && state == STATE_DATA_CAPTURE) {
			tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		}
		else if (state == STATE_DATA_CAPTURE){
			first_pass = false;
		}
	}
}

void RTC_Init(RTC_TimeTypeDef *eTime, RTC_DateTypeDef *eDate) {

	//Initialize start time for RTC
	eTime->Hours = gps_data.timestamp[0];
	eTime->Minutes = gps_data.timestamp[1];
	eTime->Seconds = gps_data.timestamp[2];
	eTime->DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	eTime->StoreOperation = RTC_STOREOPERATION_RESET;

	//Set start time for RTC
	if (HAL_RTC_SetTime(&hrtc, eTime, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}

	//Initialize start date for RTC
	eDate->Year = gps_data.datestamp[0];
	eDate->Month = gps_data.datestamp[1];
	eDate->Date = gps_data.datestamp[2];

	//Set start date for RTC
	if (HAL_RTC_SetDate(&hrtc, eDate, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}
}
