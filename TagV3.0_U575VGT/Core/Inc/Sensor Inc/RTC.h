/*
 * RTC.h
 *
 *  Created on: Sep 18, 2023
 *      Author: kevinma
 */

#ifndef INC_SENSOR_INC_RTC_H_
#define INC_SENSOR_INC_RTC_H_

#include "Lib Inc/timing.h"
#include "tx_api.h"
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define RTC_SHUTDOWN_LIMIT_SEC 20
#define RTC_SHUTDOWN_LIMIT_HOUR 1

#define RTC_SLEEP_TIME_SEC 5
#define RTC_SLEEP_TIME_TICKS tx_s_to_ticks(RTC_SLEEP_TIME_SEC)

#define RTC_GPS_TIMEOUT_MINS 1 // timeout in minutes before initializing rtc without gps timestamp

void rtc_thread_entry(ULONG thread_input);
void RTC_Init(RTC_TimeTypeDef *eTime, RTC_DateTypeDef *eDate);

#endif /* INC_SENSOR_INC_RTC_H_ */
