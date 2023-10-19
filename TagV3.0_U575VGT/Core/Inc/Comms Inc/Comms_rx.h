/*
 * Comms_rx.h
 *
 *  Created on: Oct 18, 2023
 *      Author: kevinma
 */

#ifndef INC_COMMS_INC_COMMS_RX_H_
#define INC_COMMS_INC_COMMS_RX_H_

#include "tx_port.h"

#define CMD_START_CHAR 0x24
#define CMD_VALID_START_FLAG 0x1

typedef enum __COMMS_RX_COMMANDS {
	BAD_MESSAGE = 0,
	SLEEP_MODE = 1,
	EXIT_SLEEP_MODE = 2,
	NUM_RX_COMMANDS
} Command_IDs;


void comms_rx_thread_entry(ULONG thread_input);
void comms_parse_cmd(Command_IDs cmd_id);

#endif /* INC_COMMS_INC_COMMS_RX_H_ */
