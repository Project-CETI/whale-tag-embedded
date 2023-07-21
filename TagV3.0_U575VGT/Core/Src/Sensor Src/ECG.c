/*
 * ECG.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: Kaveet
 */

#include "Sensor Inc/ECG.h"
#include "main.h"
#include "stm32u5xx_hal_cortex.h"
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c4;

TX_EVENT_FLAGS_GROUP ecg_event_flags_group;

bool ecg_running = 0;

void ecg_thread_entry(ULONG thread_input){

	//Declare ecg handler and initialize chip
	ECG_HandleTypeDef ecg;
	ecg_init(&hi2c4, &ecg);

	//Create our flag that indicates when data is ready
	tx_event_flags_create(&ecg_event_flags_group, "ECG Event Flags");

	//Enable our interrupt handler (signals when data is ready)
	HAL_NVIC_EnableIRQ(EXTI14_IRQn);

	while(1){

		//holds event flags
		ULONG actual_events;

		//We've initialized the ECG to be in continuous conversion mode, and we have an interrupt line signalling when data is ready.
		//Thus, wait for our flag to be set in the interrupt handler. This function call will block the entire task until we receive the data.
		tx_event_flags_get(&ecg_event_flags_group, ECG_DATA_READY_FLAG, TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

		ecg_running = true;

		//New data is ready, retrieve it
		ecg_read_adc(&ecg);

		ecg_running = false;
	}
}
HAL_StatusTypeDef ecg_init(I2C_HandleTypeDef* hi2c, ECG_HandleTypeDef* ecg){

	ecg->i2c_handler = hi2c;
	ecg->n_data_ready_port = ECG_NDRDY_GPIO_Port;
	ecg->n_data_ready_pin = ECG_NDRDY_Pin;

	//Reset the ADC
	ecg_reset_adc(ecg);

	//Configure the ecg adc
	ecg_write_configuration_register(ecg, ECG_ADC_DEFAULT_CONFIG_REGISTER);

	//Start the conversions
	uint8_t start_command = ECG_ADC_START;
	HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &start_command, 1, HAL_MAX_DELAY);

	return HAL_OK;
}

HAL_StatusTypeDef ecg_read_adc(ECG_HandleTypeDef* ecg){

	//Issue read command
	uint8_t read_command = ECG_ADC_READ_DATA;
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &read_command, 1, HAL_MAX_DELAY);

	//return if failed
	if (ret != HAL_OK)
		return ret;

	//Read the data
	ret = HAL_I2C_Master_Receive(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), ecg->raw_data, 3, HAL_MAX_DELAY);

	//We have 24 bits of data. Turn it into a signed 32 bit integer. Data is shifted out of ADC with the MSB first.
	//TODO: Do 2's complement conversion (once we can test values)
	int32_t digitalReading = (ecg->raw_data[0] << 16) | (ecg->raw_data[1] << 8) | ecg->raw_data[0];
	ecg->voltage = digitalReading * ECG_ADC_LSB;

	return ret;
}

HAL_StatusTypeDef ecg_write_configuration_register(ECG_HandleTypeDef* ecg, uint8_t data){

	uint8_t configure_command[2] = {0b01000000, data};

	return HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), configure_command, 2, HAL_MAX_DELAY);
}

HAL_StatusTypeDef ecg_read_configuration_register(ECG_HandleTypeDef* ecg, uint8_t * data){

	//Send read command
	uint8_t read_command = ECG_ADC_READ_CONFIG_REG;
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &read_command, 1, HAL_MAX_DELAY);

	//return if failed
	if (ret != HAL_OK)
		return ret;

	//Read the config register into the data buffer
	ret = HAL_I2C_Master_Receive(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), data, 1, HAL_MAX_DELAY);

	return ret;
}

HAL_StatusTypeDef ecg_configure_electrodes(ECG_HandleTypeDef* ecg, uint8_t electrode_config){

	//The ADC has 8 possible configurations for the Positive and negative channel. See datasheet for a list of them.
	//They are MUXED to allow for 3 bits to control the 8 combinations. These 3 bits are bits 7-5 in the config register.

	//Read the config register
	uint8_t config_register = 0;
	ecg_read_configuration_register(ecg, &config_register);

	//We want to only change the first 3 bits, so we should first unset them then set them appropriately.
	//This prevents the other settings from being changed
	//
	//Unset the first 3 bits
	config_register &= ~(111 << 5);

	//Now set them with our passed in values
	config_register |= (electrode_config << 5);

	//Now, call the function to write to the config register with our new values
	return ecg_write_configuration_register(ecg, config_register);
}

HAL_StatusTypeDef ecg_poll_data_ready(ECG_HandleTypeDef* ecg){

	uint32_t startTime = HAL_GetTick();

	//Poll for a falling edge (data ready is low)
	while (HAL_GPIO_ReadPin(ecg->n_data_ready_port, ecg->n_data_ready_pin)){

		//Track current time and check if we timeout
		uint32_t currentTime = HAL_GetTick();

		if ((currentTime - startTime) > ECG_ADC_DATA_TIMEOUT){
			return HAL_TIMEOUT;
		}
	}

	return HAL_OK;
}

HAL_StatusTypeDef ecg_reset_adc(ECG_HandleTypeDef* ecg){

	uint8_t reset_command = ECG_ADC_RESET;

	return HAL_I2C_Master_Transmit(ecg->i2c_handler, (ECG_ADC_I2C_ADDRESS << 1), &reset_command, 1, HAL_MAX_DELAY);
}
