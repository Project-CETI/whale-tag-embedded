//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_gpio_expander.h
// Description: Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#ifndef CETI_ECG_GPIO_EXPANDER_H
#define CETI_ECG_GPIO_EXPANDER_H

#include "../cetiTagLogging.h" // for CETI_LOG()
#include <pigpio.h> // for I2C functions
#include <unistd.h> // for usleep()
#include <stdio.h>  // for printing

// ----------------------------------------------------
// Define configuration constants for the GPIO expander
// ----------------------------------------------------

#define ECG_GPIO_EXPANDER_I2C_ADDRESS  0b0111001
#define ECG_GPIO_EXPANDER_CHANNEL_DATAREADY  3 // The data-ready line of the ADC
#define ECG_GPIO_EXPANDER_CHANNEL_LOD        1 // The leads-off detection output of the ECG chip

// ----------------------------------------------------------------
// Declare methods and state for interfacing with the GPIO expander
// ----------------------------------------------------------------

int ecg_gpio_expander_setup(int i2c_bus);
void ecg_gpio_expander_cleanup();
int ecg_gpio_expander_read_dataReady();
int ecg_gpio_expander_read_lod();
// Lower-level helpers.
int ecg_gpio_expander_read();
int ecg_gpio_expander_parse_dataReady(uint8_t data);
int ecg_gpio_expander_parse_lod(uint8_t data);
extern int ecg_gpio_expander_i2c_device;

#endif // CETI_ECG_GPIO_EXPANDER_H




