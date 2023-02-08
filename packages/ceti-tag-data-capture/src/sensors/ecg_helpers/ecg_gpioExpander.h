//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_gpio_expander.h
// Description: Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#ifndef ECG_GPIOEXPANDER_H
#define ECG_GPIOEXPANDER_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "../../utils/logging.h" // for CETI_LOG()
#include <pigpio.h> // for I2C functions
#include <unistd.h> // for usleep()
#include <stdio.h>  // for printing

// ------------------------------------------
// Definitions/Configuration
// ------------------------------------------

#define ECG_GPIO_EXPANDER_I2C_ADDRESS  0b0111001
#define ECG_GPIO_EXPANDER_CHANNEL_DATAREADY  3 // The data-ready line of the ADC
#define ECG_GPIO_EXPANDER_CHANNEL_LEADSOFF   1 // The leads-off detection output of the ECG chip

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------

int ecg_gpio_expander_setup(int i2c_bus);
void ecg_gpio_expander_cleanup();
int ecg_gpio_expander_read_dataReady();
int ecg_gpio_expander_read_leadsOff();
// Lower-level helpers.
int ecg_gpio_expander_read();
int ecg_gpio_expander_parse_dataReady(uint8_t data);
int ecg_gpio_expander_parse_leadsOff(uint8_t data);

#endif // ECG_GPIOEXPANDER_H




