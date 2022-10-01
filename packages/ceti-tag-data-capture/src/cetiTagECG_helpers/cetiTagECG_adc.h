//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// ADC interfacing originally based on https://github.com/OM222O/ADS1219
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_adc.h
// Description: Interfacing with the ADS1219 ADC
//-----------------------------------------------------------------------------

#ifndef CETI_ECG_ADC_H
#define CETI_ECG_ADC_H

#include "cetiTagECG_gpio_expander.h" // for reading the ADC data-ready bit
#include "../cetiTagLogging.h"        // for CETI_LOG()
#include <pigpio.h> // for I2C functions
#include <unistd.h> // for usleep()
#include <stdio.h>  // for printing

// ------------------------------------------
// Define configuration constants for the ADC
// ------------------------------------------

#define ECG_ADC_I2C_ADDRESS  0b1000100
#define ECG_ADC_DATA_READY_PIN  -1  // -1 to use the GPIO expander instead of a direct connection
#define ECG_ADC_DEBUG_PRINTOUTS  0

#define ECG_ADC_CHANNEL_ECG          0
#define ECG_ADC_CHANNEL_ELECTRODE_1  2
#define ECG_ADC_CHANNEL_ELECTRODE_2  3

#define ECG_ADC_CONFIG_REGISTER_ADDRESS  0x40
#define ECG_ADC_STATUS_REGISTER_ADDRESS  0x24

#define ECG_ADC_MUX_MASK      0x1F
#define ECG_ADC_MUX_DIFF_0_1  0x00
#define ECG_ADC_MUX_DIFF_2_3  0x20
#define ECG_ADC_MUX_DIFF_1_2  0x40
#define ECG_ADC_MUX_SINGLE_0  0x60
#define ECG_ADC_MUX_SINGLE_1  0x80
#define ECG_ADC_MUX_SINGLE_2  0xA0
#define ECG_ADC_MUX_SINGLE_3  0xC0
#define ECG_ADC_MUX_SHORTED   0xE0

#define ECG_ADC_GAIN_MASK  0xEF
#define ECG_ADC_GAIN_ONE   0x00
#define ECG_ADC_GAIN_FOUR  0x10

#define ECG_ADC_DATA_RATE_MASK  0xF3
#define ECG_ADC_DATA_RATE_20    0x00
#define ECG_ADC_DATA_RATE_90    0x04
#define ECG_ADC_DATA_RATE_330   0x08
#define ECG_ADC_DATA_RATE_1000  0x0c

#define ECG_ADC_MODE_MASK         0xFD
#define ECG_ADC_MODE_SINGLE_SHOT  0x00
#define ECG_ADC_MODE_CONTINUOUS   0x02

#define ECG_ADC_VREF_MASK      0xFE
#define ECG_ADC_VREF_INTERNAL  0x00
#define ECG_ADC_VREF_EXTERNAL  0x01

#define ECG_ADC_CMD_RESET      0x06
#define ECG_ADC_CMD_START      0x08
#define ECG_ADC_CMD_POWERDOWN  0x02
#define ECG_ADC_CMD_RREG       0x10
#define ECG_ADC_CONFIG_RESET   0x00

// -------------------------------------------------------
// Declare methods and state for interfacing with the ADC
// -------------------------------------------------------

// Initialization and shutdown
int ecg_adc_setup(int i2c_bus);
void ecg_adc_start();
void ecg_adc_cleanup();
void ecg_adc_powerDown();
// Configuration
void ecg_adc_set_gain(uint8_t gain);
void ecg_adc_set_data_rate(int rate);
void ecg_adc_set_conversion_mode(uint8_t mode);
void ecg_adc_set_voltage_reference(uint8_t vref);
// Reading
long ecg_adc_read_singleEnded(int channel);
long ecg_adc_read_differential_0_1();
long ecg_adc_read_differential_2_3();
long ecg_adc_read_differential_1_2();
long ecg_adc_read_shorted();
// Lower-level helpers
void ecg_adc_config_reset();
void ecg_adc_config_apply();
void ecg_adc_write_config_register(uint8_t data);
uint8_t ecg_adc_read_register(uint8_t reg);
long ecg_adc_read_data();
int ecg_adc_read_data_ready();

extern uint8_t ecg_adc_config;
extern uint8_t ecg_adc_config_prev;
extern int ecg_adc_is_singleShot;
extern int ecg_adc_i2c_device;

#endif // CETI_ECG_ADC_H






