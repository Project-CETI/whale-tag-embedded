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

//Library includes
#include "tx_api.h"

#define APRS_FLAG 0x7e
#define APRS_CONTROL_FIELD 0x03
#define APRS_PROTOCOL_ID 0xF0

#define APRS_SOURCE_CALLSIGN "J75Y"
#define APRS_SOURCE_SSID 1

#define APRS_SYMBOL "/C"
#define APRS_DESTINATION_CALLSIGN "APRS"
#define APRS_DESTINATION_SSID 0

#define APRS_DIGI_PATH "WIDE2"
#define APRS_DIGI_SSID 2

#define APRS_COMMENT "Build Demonstration"

#define APRS_CALLSIGN_LENGTH 6

#define APRS_DT_POS_CHARACTER '!'
#define APRS_SYM_TABLE_CHAR '1'
#define APRS_SYM_CODE_CHAR 's'

#define APRS_LATITUDE_LENGTH 9
#define APRS_LONGITUDE_LENGTH 10

//Main thread entry
void aprs_thread_entry(ULONG aprs_thread_input);

//generates an aprs packet given the latitude and longitude
void aprs_generate_packet(float lat, float lon);

#endif /* INC_RECOVERY_INC_APRS_H_ */
