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

extern UART_HandleTypeDef huart4;

extern Thread_HandleTypeDef threads[NUM_THREADS];
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP bms_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;

// event flags
TX_EVENT_FLAGS_GROUP comms_event_flags_group;

// uart buffer for receiving commands
uint8_t dataBuffer[16] = {0};

void comms_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (dataBuffer[0] == CMD_START_CHAR){
		tx_event_flags_set(&comms_event_flags_group, CMD_VALID_START_FLAG, TX_OR);
	}
}

void comms_rx_thread_entry(ULONG thread_input) {

	tx_event_flags_create(&comms_event_flags_group, "Comms RX Event Flags");
	HAL_UART_RegisterCallback(&huart4, HAL_UART_RX_COMPLETE_CB_ID, comms_UART_RxCpltCallback);

	while (1) {
		ULONG actual_flags = 0;

		while(!(actual_flags & CMD_VALID_START_FLAG)){
			HAL_UART_Receive_IT(&huart4, &dataBuffer[0], 1);
			tx_event_flags_get(&comms_event_flags_group, CMD_VALID_START_FLAG , TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		}

		// receive command
		HAL_UART_Receive(&huart4, &dataBuffer[1], 1, HAL_MAX_DELAY);
		comms_parse_cmd(dataBuffer[1]);
	}
}


void comms_parse_cmd(Command_IDs cmd_id) {

	switch (cmd_id) {
		case SLEEP_CMD:
			tx_event_flags_set(&state_machine_event_flags_group, STATE_SLEEP_MODE_FLAG, TX_OR);
			break;

		case WAKE_CMD:
			tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG, TX_OR);
			break;

		case START_CHARGE_CMD:
			tx_event_flags_set(&bms_event_flags_group, BMS_START_CHARGE_FLAG, TX_OR);
			break;

		case STOP_CHARGE_CMD:
			tx_event_flags_set(&bms_event_flags_group, BMS_STOP_CHARGE_FLAG, TX_OR);
			break;

		case TEST_SENSORS_CMD:
			tx_event_flags_set(&state_machine_event_flags_group, STATE_TEST_SENSORS_FLAG, TX_OR);
			break;

		case GET_BMS_INFO_CMD:
			tx_event_flags_set(&bms_event_flags_group, BMS_GET_ALL_REG_FLAG, TX_OR);
			break;

		case GET_IMU_INFO_CMD:
			tx_event_flags_set(&imu_event_flags_group, IMU_GET_ALL_REG_FLAG, TX_OR);
			break;

		default:
			break;
	}
}



