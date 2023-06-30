/*
 * VHF.c
 *
 *  Created on: Jun 10, 2023
 *      Author: Kaveet
 */

#include "Recovery Inc/VHF.h"
#include <stdio.h>
#include <string.h>

HAL_StatusTypeDef initialize_vhf(UART_HandleTypeDef huart, bool isHigh){

	//Set the modes of the GPIO pins attached to the vhf module.
	//Leave PTT floating, set appropriate power level and wake chip
	HAL_Delay(1000);
	set_ptt(true);
	set_power_level(isHigh);
	wake_vhf();
	HAL_Delay(1000);

	return configure_dra818v(huart, false, false, false);
}

HAL_StatusTypeDef configure_dra818v(UART_HandleTypeDef huart, bool emphasis, bool lpf, bool hpf){

	//Note: variable tracks failure so that false (0) maps to HAL_OK (also 0)
	bool failedConfig = false;

	//Data buffer to hold transmissions and responses
	char transmitData[100];
	char responseData[100];

	HAL_Delay(1000);

	//Start with the VHF handshake to confirm module is setup correctly
	sprintf(transmitData, "AT+DMOCONNECT \r\n");

	HAL_UART_Transmit(&huart, (uint8_t*) transmitData, HANDSHAKE_TRANSMIT_LENGTH, HAL_MAX_DELAY);
	HAL_UART_Receive(&huart, (uint8_t*) responseData, HANDSHAKE_RESPONSE_LENGTH, HAL_MAX_DELAY);

	//Ensure the response matches the expected response
	if (strncmp(responseData, VHF_HANDSHAKE_EXPECTED_RESPONSE, HANDSHAKE_RESPONSE_LENGTH) != 0)
		failedConfig = true;

	HAL_Delay(1000);

	//Now, set the parameters of the module
	sprintf(transmitData, "AT+DMOSETGROUP=0,144.3900,144.3900,0000,0,0000\r\n");

	HAL_UART_Transmit(&huart, (uint8_t*) transmitData, SET_PARAMETERS_TRANSMIT_LENGTH, HAL_MAX_DELAY);
	HAL_UART_Receive(&huart, (uint8_t*) responseData, SET_PARAMETERS_RESPONSE_LENGTH, HAL_MAX_DELAY);

	//Ensure the response matches the expected response
	if (strncmp(responseData, VHF_SET_PARAMETERS_EXPECTED_RESPONSE, SET_PARAMETERS_RESPONSE_LENGTH) != 0)
		failedConfig = true;

	HAL_Delay(1000);

	//Set the volume of the transmissions
	sprintf(transmitData, "AT+DMOSETVOLUME=%d\r\n", VHF_VOLUME_LEVEL);

	HAL_UART_Transmit(&huart, (uint8_t*) transmitData, SET_VOLUME_TRANSMIT_LENGTH, HAL_MAX_DELAY);
	HAL_UART_Receive(&huart, (uint8_t*) responseData, SET_VOLUME_RESPONSE_LENGTH, HAL_MAX_DELAY);

	//Ensure the response matches the expected response
	if (strncmp(responseData, VHF_SET_VOLUME_EXPECTED_RESPONSE, SET_VOLUME_RESPONSE_LENGTH) != 0)
		failedConfig = true;

	HAL_Delay(1000);

	//Set the filter parameters
	//Invert all the bools passed in since the VHF module treats "0" as true
	sprintf(transmitData, "AT+SETFILTER=%d,%d,%d\r\n", emphasis, hpf, lpf);

	HAL_UART_Transmit(&huart, (uint8_t*) transmitData, SET_FILTER_TRANSMIT_LENGTH, HAL_MAX_DELAY);
	HAL_UART_Receive(&huart, (uint8_t*) responseData, SET_FILTER_RESPONSE_LENGTH, HAL_MAX_DELAY);

	//Ensure the response matches the expected response
	if (strncmp(responseData, VHF_SET_FILTER_EXPECTED_RESPONSE, SET_FILTER_RESPONSE_LENGTH) != 0)
		failedConfig = true;

	HAL_Delay(1000);

	return failedConfig;
}


void set_ptt(bool isTx){

	//isTx determines if the GPIO is high or low, and thus if we are transmitting or not
	HAL_GPIO_WritePin(VHF_PTT_GPIO_Port, VHF_PTT_Pin, isTx);

}

void set_power_level(bool isHigh){

	//isHigh determines if we should use high power (1W) or low (0.5W)
	HAL_GPIO_WritePin(APRS_H_L_GPIO_Port, APRS_H_L_Pin, isHigh);

}

void sleep_vhf(){

	//Set the PD pin on the module to low to make the module sleep
	HAL_GPIO_WritePin(APRS_PD_GPIO_Port, APRS_PD_Pin, GPIO_PIN_RESET);
}

void wake_vhf(){

	//Set the PD pin on the module to high to wake the module
	HAL_GPIO_WritePin(APRS_PD_GPIO_Port, APRS_PD_Pin, GPIO_PIN_SET);

}
