/*
 * ECG.h
 *
 *  Created on: Feb. 9, 2023
 *      Author: Kaveet
 */

#ifndef INC_SENSOR_INC_ECG_H_
#define INC_SENSOR_INC_ECG_H_

#include "main.h"
#include "tx_api.h"

#define ECG_ADC_I2C_ADDRESS 0b1000100

//Internal V_reference on the ADC
#define ECG_ADC_V_REF 2.048

//Programmable gain value
#define ECG_ADC_GAIN 1

//The full-scale range of the ADC
#define ECG_ADC_FS_RANGE (ECG_ADC_V_REF / ECG_ADC_GAIN)

//1 LSB is FS/2^23 by the datasheet.
#define ECG_ADC_LSB (ECG_ADC_FS_RANGE / 8388608)

//Registers
#define ECG_ADC_CONFIG_REGISTER 0
#define ECG_ADC_STATUS_REGISTER 1

//Commands to send to ADC
#define ECG_ADC_RESET 					0b00000110
#define ECG_ADC_START 					0b00001000
#define ECG_ADC_POWERDOWN 				0b00000010
#define ECG_ADC_READ_DATA 				0b00010000
#define ECG_ADC_READ_CONFIG_REG 		0b00100000
#define ECG_ADC_READ_STATUS_REG 		0b00100100
#define ECG_ADC_WRITE_CONFIG_REG 		0b01000000

//Timeouts for polling data ready
#define ECG_ADC_DATA_TIMEOUT 2000

//ThreadX flag bit for when data is ready
#define ECG_DATA_READY_FLAG 0x1

//ECG configuration register has the following structure:
// Bit 7-5: MUX Electrode Selection
// Bit 4: Gain (0 = 1 or 1 = 4)
// Bit 3-2: Data Rate
// Bit 1: Conversion mode (0 = single, 1 = continuous)
// Bit 0: Vref (0 = internal/2.048V, 1 = external reference)
#define ECG_ADC_DEFAULT_CONFIG_REGISTER 0b00000010


//The number of ECG Samples to collect before writing to the SD card
#define ECG_NUM_SAMPLES 100

typedef struct __ECG_TypeDef {

	I2C_HandleTypeDef *i2c_handler;

	GPIO_TypeDef* n_data_ready_port;
	uint16_t n_data_ready_pin;

	// Data buffers for I2C data
	uint8_t raw_data[3];

	float voltage;

} ECG_HandleTypeDef;

//Our ECG function that serves as the entry point for the thread. It manages the entire ECG.
void ecg_thread_entry(ULONG thread_input);

/*
 * Initializes the ECG ADC so we can begin sampling data and retrieving it.
 *
 * Resets the chip and writes the appropriate registers to start continuous conversions.
 *
 * Parameters:
 * 			-hi2c: an I2C handler for communication
 * 			-ecg: an ECG handler to store data
 */
HAL_StatusTypeDef ecg_init(I2C_HandleTypeDef* hi2c, ECG_HandleTypeDef* ecg);

//Read the ADC for the newest data and stores it in the ECG struct
HAL_StatusTypeDef ecg_read_adc(ECG_HandleTypeDef* ecg);

//Writes to "data" to the ecg configuration register
HAL_StatusTypeDef ecg_write_configuration_register(ECG_HandleTypeDef* ecg, uint8_t data);

//Reads the configuration register and stores it in "data".
HAL_StatusTypeDef ecg_read_configuration_register(ECG_HandleTypeDef* ecg, uint8_t * data);

/*
 * Configures the ADC MUX to toggle which electrodes to read from.
 *
 * Parameters:
 * 			-ecg: the ecg handler
 * 			-electrode_config: the bits of the 3-8 MUX to appropriately set the correct electrodes.
 * 								The MUX only has 3 control bits, but this is an 8 bit number. Use the 3 least significant bits.
 */
HAL_StatusTypeDef ecg_configure_electrodes(ECG_HandleTypeDef* ecg, uint8_t electrode_config);

//Polls the ECG data ready pin for new data. Will timeout if no new data.
HAL_StatusTypeDef ecg_poll_data_ready(ECG_HandleTypeDef* ecg);

//Resets the ECG sensor (should be done before initialization)
HAL_StatusTypeDef ecg_reset_adc(ECG_HandleTypeDef* ecg);

#endif /* INC_SENSOR_INC_ECG_H_ */
