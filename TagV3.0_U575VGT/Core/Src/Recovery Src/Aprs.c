/*
 * Aprs.c
 *
 *  Created on: May 26, 2023
 *      Author: Kaveet
 */

#include "Recovery Inc/Aprs.h"
#include "Recovery Inc/VHF.h"
#include "Recovery Inc/GPS.h"
#include "Recovery Inc/AprsPacket.h"
#include "Recovery Inc/AprsTransmit.h"
#include "main.h"

//Extern variables for HAL UART handlers
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart3;

static bool toggleFreq(bool is_gps_dominica, bool is_currently_dominica);

void aprs_thread_entry(ULONG aprs_thread_input){

	//buffer for packet data
	uint8_t packetBuffer[APRS_PACKET_MAX_LENGTH] = {0};

	//Create the GPS handler and configure it
	GPS_HandleTypeDef gps;
	initialize_gps(&huart3, &gps);

	//Initialize VHF module for transmission. Turn transmission off so we don't hog the frequency
	initialize_vhf(huart4, false, TX_FREQ, RX_FREQ);
	set_ptt(false);

	//We arent in dominica by default
	bool isInDominica = false;

	//Main task loop
	while(1){

		//GPS data struct
		GPS_Data gps_data;

		//Attempt to get a GPS lock
		bool isLocked = get_gps_lock(&gps, &gps_data);

		//The time we will eventually put this task to sleep for. We assign this assuming the GPS lock has failed (only sleep for a shorter, fixed period of time).
		//If we did get a GPS lock, the sleepPeriod will correct itself by the end of the task (be appropriately assigned after APRS transmission)
		uint32_t sleepPeriod = GPS_SLEEP_LENGTH;

		//If we've locked onto a position, we can start creating an APRS packet.
		if (isLocked){

			aprs_generate_packet(packetBuffer, gps_data.latitude, gps_data.longitude);

			//We first initialized the VHF module with our default frequencies. If we are in Dominica, re-initialize the VHF module to use the dominica frequencies.
			//
			//The function also handles switching back to the default frequency if we leave dominica
			isInDominica = toggleFreq(gps_data.is_dominica, isInDominica);

			//Start transmission
			set_ptt(true);

			//Now, transmit the signal through the VHF module. Transmit a few times just for safety.
			for (uint8_t transmits = 0; transmits < 3; transmits++){
				aprs_transmit_send_data(packetBuffer, APRS_PACKET_LENGTH);
			}

			//end transmission
			set_ptt(false);

			//Set the sleep period for a successful APRS transmission
			sleepPeriod = APRS_BASE_SLEEP_LENGTH;
		}

		//Go to sleep now
		tx_thread_sleep(sleepPeriod);
	}
}

/*Reconfigures the VHF module to change the transmission frequency based on where the GPS is.
 *
 * is_gps_dominica: whether or not the current gps data is in dominica
 * is_currently_dominica: whether or not our VHF module is configured to the dominica frequency
 *
 * Returns: the current state of our configuration (whether or not VHF is configured for dominica
 */
static bool toggleFreq(bool is_gps_dominica, bool is_currently_dominica){

	//If the GPS is in dominica, but we are not configured for it, switch to dominica
	if (is_gps_dominica && !is_currently_dominica){

		//Re-initialize for dominica frequencies
		initialize_vhf(huart4, false, DOMINICA_TX_FREQ, DOMINICA_RX_FREQ);

		//Now configured for dominica, return to indicate that
		return true;
	}
	//elseif, we are not in dominica, but we are configured for dominica. Switch back to the regular frequencies
	else if (!is_gps_dominica && is_currently_dominica){

		//Re-initialize for default frequencies
		initialize_vhf(huart4, false, TX_FREQ, RX_FREQ);

		//No longer on dominica freq, return to indicate that
		return false;
	}

	//else: do nothing
	return is_currently_dominica;
}
