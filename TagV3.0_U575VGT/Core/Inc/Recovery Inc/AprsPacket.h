/*
 * AprsPacket.h
 *
 *  Created on: Jul 5, 2023
 *      Author: Kaveet
 *
 * This file contains the appropriate functions and definitons to create an APRS packet. Requires the caller to pass in a buffer and the latitude and longitude.
 *
 * There are various static functions defined in the C file to help create this packet.
 *
 * The APRS task should call this after it receives its GPS data. It will then use this packet to transmit through the VHF module.
 */

#ifndef INC_RECOVERY_INC_APRSPACKET_H_
#define INC_RECOVERY_INC_APRSPACKET_H_

//Library includes
#include "tx_api.h"
#include <stdint.h>

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

//generates an aprs packet given the latitude and longitude
void aprs_generate_packet(uint8_t * buffer, float lat, float lon);

#endif /* INC_RECOVERY_INC_APRSPACKET_H_ */
