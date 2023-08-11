/*
 * Burnwire.h
 *
 *  Created on: Aug 10, 2023
 *      Author: Kaveet
 *
 *
 * The burnwire is simply a GPIO that we set ON for a specified period of time (usually ~20 mins).
 *
 * The thread is very simple. It turns on a GPIO, goes to sleep, and then turns it off. Then it signals that the burnwire has released to the state machine.
 *
 * The thread only runs once, and the state machine is responsible for triggering/starting it. It will be in a suspended state by default, and then the state machine can resume the thread to start the burnwire.
 */

#ifndef INC_RECOVERY_INC_BURNWIRE_H_
#define INC_RECOVERY_INC_BURNWIRE_H_

#include "tx_api.h"
#include "Lib Inc/timing.h"

#define BURNWIRE_TIME_MINS 1

#define BURNWIRE_TIME_SECONDS (BURNWIRE_TIME_MINS * 60)

#define BURNWIRE_TIME_TICKS tx_s_to_ticks(BURNWIRE_TIME_SECONDS)

void burnwire_thread_entry(ULONG thread_input);

#endif /* INC_RECOVERY_INC_BURNWIRE_H_ */
