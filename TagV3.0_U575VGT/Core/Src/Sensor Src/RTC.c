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
#include "ux_device_cdc_acm.h"
#include "main.h"

//External variables
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;

extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

extern State state;

// usb buffers
extern uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitLen;

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

// uart debugging
bool rtc_get_samples = false;
bool rtc_unit_test = false;
HAL_StatusTypeDef rtc_unit_test_ret = HAL_ERROR;

void rtc_thread_entry(ULONG thread_input) {

	bool first_pass = true;

	// create events flag
	tx_event_flags_create(&rtc_event_flags_group, "RTC Event Flags");

	// wait for gps lock
	while (!gps.is_pos_locked && sTime.Minutes < RTC_GPS_TIMEOUT_MINS) {
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
	}

	rtc_unit_test_ret = RTC_Init(&sTime, &sDate);
	rtc_unit_test_ret |= RTC_Init(&eTime, &eDate);

	tx_event_flags_set(&rtc_event_flags_group, RTC_INIT_FLAG, TX_OR);

	while (1) {
		ULONG actual_flags = 0;
		HAL_StatusTypeDef ret = HAL_ERROR;

		// wait for any debugging flag
		tx_event_flags_get(&rtc_event_flags_group, RTC_CMD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		if (actual_flags & RTC_UNIT_TEST_FLAG) {
			rtc_unit_test = true;
		}
		if (actual_flags & RTC_CMD_FLAG) {
			if (usbReceiveBuf[3] == RTC_GET_SAMPLES_CMD) {
				rtc_get_samples = true;
			}
			else if (usbReceiveBuf[3] == RTC_SYNC_GPS_CMD) {
				if (gps.is_pos_locked) {
					ret = RTC_Init(&sTime, &sDate);
					ret |= RTC_Init(&eTime, &eDate);
					if (ret != HAL_OK) {
						ret = RTC_SET_GPS_TIME_ERROR;
					}
				}
				else {
					ret = RTC_NO_GPS_LOCK;
				}

				// send result over uart and usb
				HAL_UART_Transmit_IT(&huart2, &ret, 1);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					usbTransmitLen = 1;
				}
			}
			else if (usbReceiveBuf[3] == RTC_SET_TIME_CMD) {

				//Manually initialize start time for RTC
				eTime.Hours = usbReceiveBuf[4];
				eTime.Minutes = usbReceiveBuf[5];
				eTime.Seconds = usbReceiveBuf[6];
				eTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
				eTime.StoreOperation = RTC_STOREOPERATION_RESET;

				//Set start time for RTC
				ret = HAL_RTC_SetTime(&hrtc, &eTime, RTC_FORMAT_BCD);

				//Initialize start date for RTC
				eDate.Year = usbReceiveBuf[7];
				eDate.Month = usbReceiveBuf[8];
				eDate.Date = usbReceiveBuf[9];

				//Set start date for RTC
				ret |= HAL_RTC_SetDate(&hrtc, &eDate, RTC_FORMAT_BCD);

				// send result over uart and usb
				HAL_UART_Transmit_IT(&huart2, &ret, 1);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					usbTransmitLen = 1;
				}
			}
		}

		// get time from RTC (must query time THEN date to update registers)
		ret = HAL_RTC_GetTime(&hrtc, &eTime, RTC_FORMAT_BIN);
		ret |= HAL_RTC_GetDate(&hrtc, &eDate, RTC_FORMAT_BIN);

		if (rtc_unit_test) {
			rtc_unit_test_ret = ret;
			rtc_unit_test = false;
			tx_event_flags_set(&rtc_event_flags_group, RTC_UNIT_TEST_DONE_FLAG, TX_OR);
		}

		if (rtc_get_samples) {

			// send result over uart and usb
			HAL_UART_Transmit_IT(&huart2, &ret, 1);
			HAL_UART_Transmit_IT(&huart2, &eTime.Hours, 1);
			HAL_UART_Transmit_IT(&huart2, &eTime.Minutes, 1);
			HAL_UART_Transmit_IT(&huart2, &eTime.Seconds, 1);
			HAL_UART_Transmit_IT(&huart2, &eDate.Year, 1);
			HAL_UART_Transmit_IT(&huart2, &eDate.Month, 1);
			HAL_UART_Transmit_IT(&huart2, &eDate.Date, 1);

			if (usbTransmitLen == 0) {
				memcpy(&usbTransmitBuf[0], &ret, 1);
				memcpy(&usbTransmitBuf[1], &eTime.Hours, 1);
				memcpy(&usbTransmitBuf[2], &eTime.Minutes, 1);
				memcpy(&usbTransmitBuf[3], &eTime.Seconds, 1);
				memcpy(&usbTransmitBuf[4], &eDate.Year, 1);
				memcpy(&usbTransmitBuf[5], &eDate.Month, 1);
				memcpy(&usbTransmitBuf[6], &eDate.Date, 1);
				usbTransmitLen = 7;
			}
			rtc_get_samples = false;
		}

		// check if max operating duration passed
		if (abs(eTime.Minutes - sTime.Minutes) > RTC_SHUTDOWN_LIMIT_MINS && abs(eTime.Hours - sTime.Hours) > RTC_SHUTDOWN_LIMIT_HOUR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_TIMEOUT_FLAG, TX_OR);
		}

		// wait for data logging to finish
		if (!first_pass && (state == STATE_DATA_CAPTURE || state == STATE_DATA_OFFLOAD)) {
			tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		}
		// rtc thread needs to run twice at startup
		else if (state == STATE_DATA_CAPTURE || state == STATE_DATA_OFFLOAD) {
			first_pass = false;
		}
	}
}

HAL_StatusTypeDef RTC_Init(RTC_TimeTypeDef *eTime, RTC_DateTypeDef *eDate) {

	//Initialize start time for RTC (if no gps lock, all values are zero)
	eTime->Hours = gps_data.timestamp[0];
	eTime->Minutes = gps_data.timestamp[1];
	eTime->Seconds = gps_data.timestamp[2];
	eTime->DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	eTime->StoreOperation = RTC_STOREOPERATION_RESET;

	//Set start time for RTC
	if (HAL_RTC_SetTime(&hrtc, eTime, RTC_FORMAT_BCD) != HAL_OK) {
		return HAL_ERROR;
	}

	//Initialize start date for RTC
	eDate->Year = gps_data.datestamp[0];
	eDate->Month = gps_data.datestamp[1];
	eDate->Date = gps_data.datestamp[2];

	//Set start date for RTC
	if (HAL_RTC_SetDate(&hrtc, eDate, RTC_FORMAT_BCD) != HAL_OK) {
		return HAL_ERROR;
	}

	return HAL_OK;
}
