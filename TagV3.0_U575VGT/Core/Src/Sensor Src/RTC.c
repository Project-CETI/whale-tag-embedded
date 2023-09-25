/*
 * RTC.c
 *
 *  Created on: Sep 18, 2023
 *      Author: kevinma
 */

#include "Sensor Inc/RTC.h"
#include "Sensor Inc/GpsGeofencing.h"
#include "Lib Inc/state_machine.h"
#include "main.h"

//External variables
extern RTC_HandleTypeDef hrtc;
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern GPS_HandleTypeDef gps;
extern GPS_Data gps_data;

//Create RTC structs to track date and time
RTC_TimeTypeDef sTime = {0};
RTC_DateTypeDef sDate = {0};
RTC_TimeTypeDef eTime = {0};
RTC_DateTypeDef eDate = {0};

//Status for RTC initialization from GPS time
bool rtc_setup = false;

void RTC_thread_entry(ULONG thread_input) {

	//Initialize start date and time for RTC only if there is GPS lock
	if (gps.is_pos_locked) {
		RTC_Init(&sTime, &sDate);
		RTC_Init(&eTime, &eDate);
		rtc_setup = true;
	}

	while (1 && rtc_setup) {

		//Calibrate RTC with new GPS date and time every time interval
		if (gps.is_pos_locked && eTime.Minutes % RTC_INIT_REFRESH_MINS == 0) {
			RTC_Init(&eTime, &eDate);
		}

		//Get time from RTC (must query time THEN date to update registers)
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

		//Check if max operating duration passed
		if (abs(eTime.Seconds - sTime.Seconds) > RTC_SHUTDOWN_LIMIT_SEC && abs(eTime.Hours - sTime.Hours) > RTC_SHUTDOWN_LIMIT_HOUR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_TIMEOUT_FLAG, TX_OR);
		}

		//Sleep and repeat the process once woken up
		tx_thread_sleep(RTC_SLEEP_TIME_TICKS);
	}
}

void RTC_Init(RTC_TimeTypeDef *eTime, RTC_DateTypeDef *eDate) {

	eTime->Hours = gps_data.timestamp[0];
	eTime->Minutes = gps_data.timestamp[1];
	eTime->Seconds = gps_data.timestamp[2];
	eTime->DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	eTime->StoreOperation = RTC_STOREOPERATION_RESET;

	//Set start time for RTC
	if (HAL_RTC_SetTime(&hrtc, &eTime, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}

	//Initialize start date for RTC
	eDate->Year = gps_data.datestamp[0];
	eDate->Month = gps_data.datestamp[1];
	eDate->Date = gps_data.datestamp[2];

	//Set start date for RTC
	if (HAL_RTC_SetDate(&hrtc, &eDate, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}
}

