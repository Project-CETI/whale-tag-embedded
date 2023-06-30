/*
 * VHF.h
 *
 *  Created on: Jun 10, 2023
 *      Author: Kaveet
 *
 *  This file outlines the UART driver for the VHF module (DRA818V).
 *
 *  It initializes, configures and manages the VHF module used in recovery mode.
 */

#ifndef INC_RECOVERY_INC_VHF_H_
#define INC_RECOVERY_INC_VHF_H_

#include "main.h"
#include <stdbool.h>

//Defines
#define TX_FREQ "144.3900"
#define RX_FREQ "144.3900"
#define VHF_VOLUME_LEVEL 4

//Lengths of messages used to configure the VHF module
#define DUMMY_TRANSMIT_LENGTH 6
#define DUMMY_RESPONSE_LENGTH 15

#define HANDSHAKE_TRANSMIT_LENGTH 16
#define HANDSHAKE_RESPONSE_LENGTH 15

#define SET_PARAMETERS_TRANSMIT_LENGTH 48
#define SET_PARAMETERS_RESPONSE_LENGTH 16

#define SET_VOLUME_TRANSMIT_LENGTH 19
#define SET_VOLUME_RESPONSE_LENGTH 17

#define SET_FILTER_TRANSMIT_LENGTH 20
#define SET_FILTER_RESPONSE_LENGTH 17

//Expected responses to transmitted messages
#define VHF_HANDSHAKE_EXPECTED_RESPONSE "+DMOCONNECT:0\r\n"
#define VHF_SET_PARAMETERS_EXPECTED_RESPONSE "+DMOSETGROUP:0\r\n"
#define VHF_SET_VOLUME_EXPECTED_RESPONSE "+DMOSETVOLUME:0\r\n"
#define VHF_SET_FILTER_EXPECTED_RESPONSE "+DMOSETFILTER:0\r\n"


/*Function to initialize and configure the VHF module based off the input parameters
 Parameters:
 	 -huart - UART handler to talk to the module
 	 -isHigh: whether or not to use high power (1W) or low power (0.5W). Passing true means high power.
*/
HAL_StatusTypeDef initialize_vhf(UART_HandleTypeDef huart, bool isHigh);

/*Configure the VHF module over UART
 Parameters:
 	 -huart - UART handler to talk to the module
 	 -isHigh: whether or not to use high power (1W) or low power (0.5W). Passing true means high power.
 	 -emphasis: whether or not to use emphasis on rf signals. True = use emphasis.
 	 -lpf: Apply a low-pass-filter on the signals.
 	 -hps: Apply a high-pass-filter on the signals.
*/
HAL_StatusTypeDef configure_dra818v(UART_HandleTypeDef huart, bool emphasis, bool lpf, bool hpf);

//Enables or disbles the push-to-talk pin. Setting true lets us talk to the module (i.e., transmit signals).
void set_ptt(bool isTx);

//Toggles the power level on the module from high (1W) and low (0.5W).
void set_power_level(bool isHigh);

//Puts the VHF module to sleep
void sleep_vhf();

//Wakes up the VHF module
void wake_vhf();

#endif /* INC_RECOVERY_INC_VHF_H_ */
