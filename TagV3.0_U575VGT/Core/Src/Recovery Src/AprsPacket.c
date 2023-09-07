/*
 * AprsPacket.c
 *
 *  Created on: Jul 5, 2023
 *      Author: Kaveet
 */

#include "Recovery Inc/AprsPacket.h"
#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

static void append_flag(uint8_t * buffer, uint8_t numFlags);
static void append_callsign(uint8_t * buffer, char * callsign, uint8_t ssid);
static void append_gps_data(uint8_t * buffer, float lat, float lon);
static void append_other_data(uint8_t * buffer, uint16_t course, uint16_t speed, char * comment);
static void append_frame_check(uint8_t * buffer, uint8_t buffer_length);

void aprs_generate_packet(uint8_t * buffer, float lat, float lon){

	append_flag(buffer, 150);

	append_callsign(&buffer[150], APRS_DESTINATION_CALLSIGN, APRS_DESTINATION_SSID);

	append_callsign(&buffer[157], APRS_SOURCE_CALLSIGN, APRS_SOURCE_SSID);

	//We can also treat the digipeter as a callsign since it has the same format
	append_callsign(&buffer[164], APRS_DIGI_PATH, APRS_DIGI_SSID);
	buffer[170] += 1;

	//Add the control ID and protocol ID
	buffer[171] = APRS_CONTROL_FIELD;
	buffer[172] = APRS_PROTOCOL_ID;

	//Attach the payload (including other control characters)
	append_gps_data(&buffer[173], lat, lon);

	//Attach other information (course, speed and the comment)
	append_other_data(&buffer[193], 360, 0, APRS_COMMENT);

	append_frame_check(buffer, 219);

	append_flag(&buffer[221], 3);

	HAL_Delay(100);
}

//Appends the flag character (0x7E) to the buffer 'numFlags' times.
static void append_flag(uint8_t * buffer, uint8_t numFlags){

	//Add numFlags flag characters to the buffer
	for (uint8_t index = 0; index < numFlags; index++){
		buffer[index] = APRS_FLAG;
	}
}

//Appends a callsign to the buffer with its SSID.
static void append_callsign(uint8_t * buffer, char * callsign, uint8_t ssid){

	//Determine the length of the callsign
	uint8_t length  = strlen(callsign);

	//Append the callsign to the buffer. Note that ASCII characters must be left shifted by 1 bit as per APRS101 standard
	for (uint8_t index = 0; index < length; index++){
		buffer[index] = (callsign[index] << 1);
	}

	//The callsign field must be atleast 6 characters long, so fill any missing spots with blanks
	if (length < APRS_CALLSIGN_LENGTH){
		for (uint8_t index = length; index < APRS_CALLSIGN_LENGTH; index++){
			//We still need to shift left by 1 bit
			buffer[index] = (' ' << 1);
		}
	}

	//Now, we've filled the first 6 bytes with the callsign (index 0-5), so the SSID must be in the 6th index.
	//We can find its ASCII character by adding the integer value to the ascii value of '0'. Still need to shift left by 1 bit.
	buffer[APRS_CALLSIGN_LENGTH] = (ssid + '0') << 1;
}

//Appends the GPS data (latitude and longitude) to the buffer
static void append_gps_data(uint8_t * buffer, float lat, float lon){

	//indicate start of real-time transmission
	buffer[0] = APRS_DT_POS_CHARACTER;

	//First, create the string containing the latitude and longitude data, then save it into our buffer
	bool is_north = true;

	//If we have a negative value, then the location is in the southern hemisphere.
	//Recognize this and then just use the magnitude of the latitude for future calculations.
	if (lat < 0){
		is_north = false;
		is_north *= -1;
	}

	//The coordinates we get from the GPS are in degrees and fractional degrees
	//We need to extract the whole degrees from this, then the whole minutes and finally the fractional minutes

	//The degrees are just the rounded-down integer
	uint8_t lat_deg_whole = (uint8_t) lat;

	//Find the remainder (fractional degrees) and multiply it by 60 to get the minutes (fractional and whole)
	float lat_minutes = (lat - lat_deg_whole) * 60;

	//Whole number minutes is just the fractional component.
	uint8_t lat_minutes_whole = (uint8_t) lat_minutes;

	//Find the remainder (fractional component) and save it to two decimal points (multiply by 100 and cast to int)
	uint8_t lat_minutes_frac = (lat_minutes - lat_minutes_whole) * 100;

	//Find our direction indicator (N for North of S for south)
	char lat_direction = (is_north) ? 'N' : 'S';

	//Create our string. We use the format ddmm.hh(N/S), where "d" is degrees, "m" is minutes and "h" is fractional minutes.
	//Store this in our buffer.
	snprintf(&buffer[1], APRS_LATITUDE_LENGTH, "%02d%02d.%02d%c", lat_deg_whole, lat_minutes_whole, lat_minutes_frac, lat_direction);


	//Right now we have the null-terminating character in the buffer "\0". Replace this with our latitude and longitude seperating symbol "1".
	buffer[APRS_LATITUDE_LENGTH] = APRS_SYM_TABLE_CHAR;

	//Now, repeat the process for longitude.
	bool is_east = true;

	//If its less than 0, remember it as West, and then take the magnitude
	if (lon < 0){
		is_east = false;
		lon *= -1;
	}

	//Find whole number degrees
	uint8_t lon_deg_whole = (uint8_t) lon;

	//Find remainder (fractional degrees), convert to minutes
	float lon_minutes = (lon - lon_deg_whole) * 60;

	//Find whole number and fractional minutes. Take two decimal places for the fractional minutes, just like before
	uint8_t lon_minutes_whole = (uint8_t) lon_minutes;
	uint8_t lon_minutes_fractional = (lon_minutes - lon_minutes_whole) * 100;

	//Find direction character
	char lon_direction = (is_east) ? 'E' : 'W';

	//Store this in the buffer, in the format dddmm.hh(E/W)
	snprintf(&buffer[APRS_LATITUDE_LENGTH + 1], APRS_LONGITUDE_LENGTH, "%03d%02d.%02d%c", lon_deg_whole, lon_minutes_whole, lon_minutes_fractional, lon_direction);

	//Appending payload character indicating the APRS symbol (using boat symbol). Replace the null-terminating character with it.
	buffer[APRS_LATITUDE_LENGTH + APRS_LONGITUDE_LENGTH] = APRS_SYM_CODE_CHAR;
}

//Appends other extra data (course, speed and the comment)
static void append_other_data(uint8_t * buffer, uint16_t course, uint16_t speed, char * comment){

	//Append the course and speed of the tag (course is the heading 0->360 degrees
	uint8_t length = 8 + strlen(comment);
	snprintf(buffer, length, "%03d/%03d%s", course, speed, comment);
}

//Calculates and appends the CRC frame checker. Follows the CRC-16 CCITT standard.
static void append_frame_check(uint8_t * buffer, uint8_t buffer_length){

	uint16_t crc = 0xFFFF;

	//Loop through each *bit* in the buffer. Only start after the starting flags.
	for (uint8_t index = 150; index < buffer_length; index++){

		uint8_t byte = buffer[index];

		for (uint8_t bit_index = 0; bit_index < 8; bit_index++){

			bool bit = (byte >> bit_index) & 0x01;

			//Bit magic for the CRC
			unsigned short xorIn;
			xorIn = crc ^ bit;

			crc >>= 1;

			if (xorIn & 0x01) crc ^= 0x8408;

		}
	}

	uint8_t crc_lo = (crc & 0xFF) ^ 0xFF;
	uint8_t crc_hi = (crc >> 8) ^ 0xFF;

	buffer[buffer_length] = crc_lo;
	buffer[buffer_length + 1] = crc_hi;
}

