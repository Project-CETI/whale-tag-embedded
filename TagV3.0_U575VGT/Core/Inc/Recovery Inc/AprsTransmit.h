/*
 * AprsTransmit.h
 *
 *  Created on: May 26, 2023
 *      Author: Kaveet
 */

#ifndef INC_RECOVERY_INC_APRSTRANSMIT_H_
#define INC_RECOVERY_INC_APRSTRANSMIT_H_


/*
 * This file handles the transmission of the APRS sine wave to the VHF module.
 *
 * This includes:
 * 			- Tell the VHF module we will start talking to it
 * 			- Generate the sine wave and appropriately apply the frequency modulation to it
 *
 * Parameters:
 * 			- The raw data array to transmit (received from main APRS task)
 *
 *
 * Returns:
 * 			- Whether or not the transmission was successful
 *
 */

//Includes
#include <stdbool.h>
#include <stdint.h>
#include "tx_api.h"

//Defines
//The time to transmit each bit for
//Each ThreadX tick is 50us -> 17 ticks makes it 850us. Based off the PICO code for the old recovery boards, where the bit time is 832us.
#define APRS_TRANSMIT_BIT_TIME 17

//The hardware timer periods for 1200Hz and 2400Hz signals
#define APRS_TRANSMIT_PERIOD_1200HZ 84
#define APRS_TRANSMIT_PERIOD_2400HZ 45

//Max digital input to a dac for 8-bit inputs
#define APRS_TRANSMIT_MAX_DAC_INPUT 256

//Self-explanatory
#define BITS_PER_BYTE 8

//The number of sample points for the output sine wave. The more samples the smoother the wave.
#define APRS_TRANSMIT_NUM_SINE_SAMPLES 100

//Macros
//Reads out a specific bit inside of a byte. Used in source code to iterate through bits.
#define aprs_transmit_read_bit(BYTE, BIT_POS) (((BYTE) >> (BIT_POS)) & 0x01)

//Public functions
bool aprs_transmit_send_data(uint8_t * packet_data, uint16_t packet_length);
void aprs_transmit_bit_timer_entry(ULONG bit_timer_input);

#endif /* INC_RECOVERY_INC_APRSTRANSMIT_H_ */
