/*
 * Burnwire.c
 *
 *  Created on: Aug 10, 2023
 *      Author: Kaveet
 */


#include "Recovery Inc/Burnwire.h"
#include "main.h"
#include "stm32u5xx_hal_gpio.h"
#include "Lib Inc/state_machine.h"

//State machine event flags to signal when burnwire is finishes
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

void burnwire_thread_entry(ULONG thread_input){

	/* The burnwire is just a GPIO, and GPIOs are already setup in MX_GPIO_Init (main.c), so theres nothing to initialize or setup.
	 *
	 * Also, the burnwire only runs once, so theres no need to have a infinite while loop, just run it once to completion.
	 */

	//If we reach this line, then the state machine has started the thread (and thus signalled for the burnwire to start).
	//Ensure that the burnwire pin is named to "BURNWIRE_ON" in the IOC so that across multiple boards, the macro defined in main.h will set it to the correct pin.
	HAL_GPIO_WritePin(BURNWIRE_ON_GPIO_Port, BURNWIRE_ON_Pin, GPIO_PIN_SET);

	//Go to sleep for the burnwire time
	tx_thread_sleep(BURNWIRE_TIME_TICKS);

	//Turn off the burnwire
	HAL_GPIO_WritePin(BURNWIRE_ON_GPIO_Port, BURNWIRE_ON_Pin, GPIO_PIN_RESET);

	//Signal to the state machine that the tag has released
	tx_event_flags_set(&state_machine_event_flags_group, STATE_TAG_RELEASED_FLAG, TX_OR);


}
