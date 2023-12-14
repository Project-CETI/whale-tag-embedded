/*
 * Comms_rx.h
 *
 *  Created on: Oct 18, 2023
 *      Author: kevinma
 */

#ifndef INC_COMMS_INC_COMMS_RX_H_
#define INC_COMMS_INC_COMMS_RX_H_

#include "main.h"
#include "tx_port.h"

// commsx rx flags
#define UART_VALID_START_FLAG		0x1

// uart command start char
#define UART_START_CHAR				0x24

// uart command ids
#define UART_SLEEP_CMD				0x1
#define UART_WAKE_CMD				0x2
#define UART_TEST_SENSORS_CMD		0x3
#define UART_READ_SENSOR_CMD		0x4
#define UART_WRITE_SENSOR_CMD		0x5
#define UART_CMD_SENSOR_CMD			0x6
#define UART_SIM_CMD				0x7

// sensor ids
#define IMU_SENSOR_ID				0x1
#define BMS_SENSOR_ID				0x2
#define AUDIO_SENSOR_ID				0x3
#define ECG_SENSOR_ID				0x4
#define DEPTH_SENSOR_ID				0x5
#define LIGHT_SENSOR_ID				0x6
#define RTC_SENSOR_ID				0x7

// subcommands for simulation command
#define SIM_EXIT_CMD				0x1
#define SIM_ENTER_CMD				0x2

// timeout constants
#define UART_UNIT_TEST_TIMEOUT		tx_s_to_ticks(30)

void comms_parse_cmd(uint8_t cmd_id);

// main comms rx thread to run on RTOS
void comms_rx_thread_entry(ULONG thread_input);

#endif /* INC_COMMS_INC_COMMS_RX_H_ */
