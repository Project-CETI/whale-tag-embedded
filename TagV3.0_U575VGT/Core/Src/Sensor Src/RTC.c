/*
 * RTC.c
 *
 *  Created on: Sep 18, 2023
 *      Author: kevinma
 */

#include "Sensor Inc/RTC.h"
#include "Lib Inc/state_machine.h"
#include "main.h"

//External variables
extern RTC_HandleTypeDef hrtc;
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern UART_HandleTypeDef huart2;

void RTC_thread_entry(ULONG thread_input) {

	//Create variable to track time
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	//Initialize start time for RTC
	sTime.Hours = 0;
	sTime.Minutes = 0;
    sTime.Seconds = 0;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    //Set start time for RTC
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) {
    	Error_Handler();
    }

    //Initialize start date for RTC
    sDate.WeekDay = RTC_WEEKDAY_SUNDAY;
    sDate.Month = RTC_MONTH_SEPTEMBER;
    sDate.Date = 0x15;
    sDate.Year = 0x23;

    //Set start date for RTC
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) {
    	Error_Handler();
    }

	while (1) {

		//Get time from RTC
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

		if (sTime.Seconds > RTC_SHUTDOWN_LIMIT_SEC) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_CRITICAL_LOW_BATTERY_FLAG, TX_OR);
		}

		uint8_t sec = sTime.Seconds;

		HAL_UART_Transmit(&huart2, &sec, 1, HAL_MAX_DELAY);

		//Sleep and repeat the process once woken up
		tx_thread_sleep(RTC_SLEEP_TIME_TICKS);
	}
}

