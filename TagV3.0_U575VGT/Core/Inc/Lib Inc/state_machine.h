/*
 * state_machine.h
 *
 *  Created on: Jul 24, 2023
 *      Author: Kaveet
 */

#ifndef INC_LIB_INC_STATE_MACHINE_H_
#define INC_LIB_INC_STATE_MACHINE_H_

#include "tx_api.h"

#define IS_SIMULATING false
//Should correspond with the state types enum below
#define SIMULATING_STATE 1

//Flags inside of our state machine event flags
#define STATE_TIMEOUT_FLAG 0x1
#define STATE_GPS_FENCE_FLAG 0x2
#define STATE_LOW_BATT_FLAG 0x4
#define STATE_CRIT_BATT_FLAG 0x8
#define STATE_USB_CONNECTED_FLAG 0x10
#define STATE_USB_DISCONNECTED_FLAG 0x20
#define STATE_SLEEP_MODE_FLAG 0x40
#define STATE_EXIT_SLEEP_MODE_FLAG 0x80
#define STATE_TAG_RELEASED_FLAG 0x100
#define STATE_BMS_NONVOLATILE_SETUP_FLAG 0x200
#define STATE_BMS_OPEN_MOSFET_FLAG 0x400
#define STATE_BMS_CLOSE_MOSFET_FLAG 0x800
#define STATE_SD_CARD_FULL_FLAG 0x1000

#define DATA_OFFLOADING_SD_CLK_DIV 6
#define NORMAL_SD_CLK_DIV 2

#define ALL_STATE_FLAGS (STATE_TIMEOUT_FLAG | STATE_GPS_FENCE_FLAG | STATE_LOW_BATT_FLAG | STATE_CRIT_BATT_FLAG | STATE_USB_CONNECTED_FLAG | STATE_USB_DISCONNECTED_FLAG | STATE_SLEEP_MODE_FLAG | STATE_EXIT_SLEEP_MODE_FLAG | STATE_TAG_RELEASED_FLAG | STATE_BMS_NONVOLATILE_SETUP_FLAG | STATE_BMS_OPEN_MOSFET_FLAG | STATE_BMS_CLOSE_MOSFET_FLAG | STATE_SD_CARD_FULL_FLAG)

typedef enum {
	STATE_DATA_CAPTURE = 0,
	STATE_RECOVERY,
	STATE_DATA_OFFLOAD,
	STATE_FISHTRACKER,
	STATE_SLEEP,
	NUM_STATES //Add new states ABOVE this line
}State;

//Main thread entry for the state machine thread
void state_machine_thread_entry(ULONG thread_input);

//Starts the data capture threads (e.g., audio, imu, ecg, etc.)
void enter_data_capture();

//Suspends the threads with the intention that they may be resumed later
void soft_exit_data_capture();

//Signals the data collection thread to stop, which terminates the thread and stops collection entirely
void hard_exit_data_capture();

//Starts the recovery threads(e.g., aprs)
void enter_recovery();

//Signals recovery threads to exit (through a flag)
void exit_recovery();

//Starts the data offloading threads (e.g., USBX)
void enter_data_offload();

//Exits the USBX and data offloading threads
void exit_data_offload();

#endif /* INC_LIB_INC_STATE_MACHINE_H_ */
