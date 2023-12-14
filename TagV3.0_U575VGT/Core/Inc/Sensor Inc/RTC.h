/*
 * RTC.h
 *
 *  Created on: Sep 18, 2023
 *      Author: kevin ma
 */

#ifndef INC_SENSOR_INC_RTC_H_
#define INC_SENSOR_INC_RTC_H_

#include "Lib Inc/timing.h"
#include "tx_api.h"
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

// rtc timeout settings
#define RTC_SHUTDOWN_LIMIT_MINS		20
#define RTC_SHUTDOWN_LIMIT_HOUR		1

// rtc initialization timeout
#define RTC_GPS_TIMEOUT_MINS		1 // timeout in minutes before initializing rtc without gps timestamp

// rtc event flags
#define RTC_INIT_FLAG				0x1

// rtc event flags for uart debugging
#define RTC_UNIT_TEST_FLAG			0x2
#define RTC_UNIT_TEST_DONE_FLAG		0x4
#define RTC_CMD_FLAG				0x8

// commands for rtc
#define RTC_GET_SAMPLES_CMD			0x1
#define RTC_SYNC_GPS_CMD			0x2
#define RTC_SET_TIME_CMD			0x3

// status constants for rtc set gps time command
#define RTC_NO_GPS_LOCK				0x1
#define RTC_SET_GPS_TIME_ERROR		0x2

HAL_StatusTypeDef RTC_Init(RTC_TimeTypeDef *eTime, RTC_DateTypeDef *eDate);

//Main RTC thread to run on RTOS
void rtc_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_RTC_H_ */
