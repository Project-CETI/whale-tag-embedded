/*
 * Comms_rx.c
 *
 *  Created on: Oct 18, 2023
 *      Author: kevinma
 */

#include "Comms Inc/Comms_rx.h"
#include "Lib Inc/state_machine.h"
#include "Lib Inc/threads.h"
#include "Sensor Inc/BMS.h"
#include "Sensor Inc/BNO08x.h"
#include "main.h"
#include "stm32u5xx_hal_uart.h"

extern UART_HandleTypeDef huart2;

extern Thread_HandleTypeDef threads[NUM_THREADS];

// thread flags
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP bms_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP light_event_flags_group;
extern TX_EVENT_FLAGS_GROUP rtc_event_flags_group;

// unit test status variables
extern HAL_StatusTypeDef imu_unit_test_ret;
extern HAL_StatusTypeDef bms_unit_test_ret;
extern HAL_StatusTypeDef audio_unit_test_ret;
extern HAL_StatusTypeDef ecg_unit_test_ret;
extern HAL_StatusTypeDef depth_unit_test_ret;
extern HAL_StatusTypeDef light_unit_test_ret;
extern HAL_StatusTypeDef rtc_unit_test_ret;

// comms rx flags
TX_EVENT_FLAGS_GROUP comms_event_flags_group;

// uart buffer for receiving commands
uint8_t uartReceiveBuf[16] = {0};

void comms_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (uartReceiveBuf[0] == UART_START_CHAR){
		tx_event_flags_set(&comms_event_flags_group, UART_VALID_START_FLAG, TX_OR);
	}
}

void comms_rx_thread_entry(ULONG thread_input) {

	tx_event_flags_create(&comms_event_flags_group, "Comms RX Event Flags");
	HAL_UART_RegisterCallback(&huart2, HAL_UART_RX_COMPLETE_CB_ID, comms_UART_RxCpltCallback);

	while (1) {
		ULONG actual_flags = 0;

		while(!(actual_flags & UART_VALID_START_FLAG)){
			HAL_UART_Receive_IT(&huart2, &uartReceiveBuf[0], 7);
			tx_event_flags_get(&comms_event_flags_group, UART_VALID_START_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		}

		// receive command
		//HAL_UART_Receive(&huart2, &uartReceiveBuf[1], 1, HAL_MAX_DELAY);
		comms_parse_cmd(uartReceiveBuf[1]);
	}
}

void comms_parse_cmd(uint8_t cmd_id) {

	switch (cmd_id) {
		case UART_SLEEP_CMD:
			tx_event_flags_set(&state_machine_event_flags_group, STATE_SLEEP_MODE_FLAG, TX_OR);
			break;
		case UART_WAKE_CMD:
			tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG, TX_OR);
			break;
		case UART_TEST_SENSORS_CMD:
			ULONG actual_flags = 0;

			// send unit test flag for all sensors
			tx_event_flags_set(&imu_event_flags_group, IMU_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&imu_event_flags_group, IMU_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & IMU_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &imu_unit_test_ret, 1);
				imu_unit_test_ret = HAL_ERROR;
			}
			else {
				imu_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &imu_unit_test_ret, 1);
			}

			actual_flags = 0;
			tx_event_flags_set(&bms_event_flags_group, BMS_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&bms_event_flags_group, BMS_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & BMS_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &bms_unit_test_ret, 1);
				bms_unit_test_ret = HAL_ERROR;
			}
			else {
				bms_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &bms_unit_test_ret, 1);
			}

			actual_flags = 0;
			tx_event_flags_set(&audio_event_flags_group, AUDIO_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&audio_event_flags_group, AUDIO_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & AUDIO_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &audio_unit_test_ret, 1);
				// NOTE: audio unit test not reset to HAL_ERROR because it is only run once
			}
			else {
				audio_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &audio_unit_test_ret, 1);
			}

			actual_flags = 0;
			tx_event_flags_set(&ecg_event_flags_group, ECG_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&ecg_event_flags_group, ECG_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & ECG_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &ecg_unit_test_ret, 1);
				ecg_unit_test_ret = HAL_ERROR;
			}
			else {
				ecg_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &ecg_unit_test_ret, 1);
			}

			actual_flags = 0;
			tx_event_flags_set(&depth_event_flags_group, DEPTH_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&depth_event_flags_group, DEPTH_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & DEPTH_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &depth_unit_test_ret, 1);
				depth_unit_test_ret = HAL_ERROR;
			}
			else {
				depth_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &depth_unit_test_ret, 1);
			}

			actual_flags = 0;
			tx_event_flags_set(&light_event_flags_group, LIGHT_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&light_event_flags_group, LIGHT_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & LIGHT_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &light_unit_test_ret, 1);
				light_unit_test_ret = HAL_ERROR;
			}
			else {
				light_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &light_unit_test_ret, 1);
			}

			actual_flags = 0;
			tx_event_flags_set(&rtc_event_flags_group, RTC_UNIT_TEST_FLAG, TX_OR);
			tx_event_flags_get(&rtc_event_flags_group, RTC_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, UART_UNIT_TEST_TIMEOUT);
			if (actual_flags & RTC_UNIT_TEST_DONE_FLAG) {
				HAL_UART_Transmit_IT(&huart2, &rtc_unit_test_ret, 1);
				rtc_unit_test_ret = HAL_ERROR;
			}
			else {
				rtc_unit_test_ret = HAL_TIMEOUT;
				HAL_UART_Transmit_IT(&huart2, &rtc_unit_test_ret, 1);
			}
			break;
		case UART_READ_SENSOR_CMD:
			// send read flag to respective sensor
			if (uartReceiveBuf[2] == BMS_SENSOR_ID) {
				tx_event_flags_set(&bms_event_flags_group, BMS_READ_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == AUDIO_SENSOR_ID) {
				tx_event_flags_set(&audio_event_flags_group, AUDIO_READ_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == ECG_SENSOR_ID) {
				tx_event_flags_set(&ecg_event_flags_group, ECG_READ_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == LIGHT_SENSOR_ID) {
				tx_event_flags_set(&light_event_flags_group, LIGHT_READ_FLAG, TX_OR);
			}
			break;
		case UART_WRITE_SENSOR_CMD:
			// send write flag to respective sensor
			if (uartReceiveBuf[2] == BMS_SENSOR_ID) {
				tx_event_flags_set(&bms_event_flags_group, BMS_WRITE_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == ECG_SENSOR_ID) {
				tx_event_flags_set(&ecg_event_flags_group, ECG_WRITE_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == LIGHT_SENSOR_ID) {
				tx_event_flags_set(&light_event_flags_group, LIGHT_WRITE_FLAG, TX_OR);
			}
			break;
		case UART_CMD_SENSOR_CMD:
			// send command flag to respective sensor
			if (uartReceiveBuf[2] == IMU_SENSOR_ID) {
				tx_event_flags_set(&imu_event_flags_group, IMU_CMD_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == BMS_SENSOR_ID) {
				tx_event_flags_set(&bms_event_flags_group, BMS_CMD_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == AUDIO_SENSOR_ID) {
				tx_event_flags_set(&audio_event_flags_group, AUDIO_CMD_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == ECG_SENSOR_ID) {
				tx_event_flags_set(&ecg_event_flags_group, ECG_CMD_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == DEPTH_SENSOR_ID) {
				tx_event_flags_set(&depth_event_flags_group, DEPTH_CMD_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == LIGHT_SENSOR_ID) {
				tx_event_flags_set(&light_event_flags_group, LIGHT_CMD_FLAG, TX_OR);
			}
			else if (uartReceiveBuf[2] == RTC_SENSOR_ID) {
				tx_event_flags_set(&rtc_event_flags_group, RTC_CMD_FLAG, TX_OR);
			}
			break;
		case UART_SIM_CMD:
			// exit simulation mode
			if (uartReceiveBuf[2] == SIM_EXIT_CMD) {
				tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SIM_FLAG, TX_OR);
			}
			// enter simulation mode
			else if (uartReceiveBuf[2] == SIM_ENTER_CMD) {
				tx_event_flags_set(&state_machine_event_flags_group, STATE_ENTER_SIM_FLAG, TX_OR);
			}
		default:
			// clear receive buffer if invalid command to avoid errors
			uartReceiveBuf[0] = 0;
			uartReceiveBuf[1] = 0;
			uartReceiveBuf[2] = 0;
			uartReceiveBuf[3] = 0;
			uartReceiveBuf[4] = 0;
			uartReceiveBuf[5] = 0;
			uartReceiveBuf[6] = 0;
			break;
	}
}



