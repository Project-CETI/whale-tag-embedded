/*
 * Aprs.h
 *
 *  Created on: May 26, 2023
 *      Author: Kaveet
 */

#ifndef INC_RECOVERY_INC_APRS_H_
#define INC_RECOVERY_INC_APRS_H_

/*
 * This file contains the main APRS thread responsible for board recovery.
 *
 * It receives the GPS data from the GPS functions, and then calls the appropriate library functions
 *  to break up the data into an array and transmit it to the VHF module
 *
 *  Library Functions/Files:
 *  	- AprsTransmit -> handles the transmission of the sine wave
 *  	- AprsPacket -> breaks the GPS data into appropriate packets
 */
#include "tx_api.h"

#define APRS_PACKET_MAX_LENGTH 255

#define APRS_PACKET_LENGTH 224

#define GPS_SLEEP_LENGTH tx_s_to_ticks(10)

#define APRS_BASE_SLEEP_LENGTH tx_s_to_ticks(60)

#define NUM_TX_ATTEMPTS 3

//Main thread entry
void aprs_thread_entry(ULONG aprs_thread_input);

#endif /* INC_RECOVERY_INC_APRS_H_ */
