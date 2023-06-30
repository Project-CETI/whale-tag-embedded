//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#ifndef ECG_GPIOEXPANDER_H
#define ECG_GPIOEXPANDER_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "../../utils/logging.h" // for CETI_LOG()
#include "../ecg.h" // for ECG_LEADSOFF_INVALID_PLACEHOLDER
#include <pigpio.h> // for I2C functions
#include <unistd.h> // for usleep()
#include <stdio.h>  // for printing

// ------------------------------------------
// Definitions/Configuration
// ------------------------------------------

#define ECG_GPIO_EXPANDER_I2C_ADDRESS  0b0111001
#define ECG_GPIO_EXPANDER_CHANNEL_DATAREADY  7 // The data-ready line of the ADC.
                                               //   7 for the new ECG board (v1.1), and
                                               //   3 for the old board (v0.9 and v1.0).
#define ECG_GPIO_EXPANDER_CHANNEL_LEADSOFF_P 1 // The leads-off detection output of the ECG chip (positive electrode)
#define ECG_GPIO_EXPANDER_CHANNEL_LEADSOFF_N 0 // The leads-off detection output of the ECG chip (negative electrode)
#define ECG_GPIO_EXPANDER_CHANNEL_LEDRED     6 // 2
#define ECG_GPIO_EXPANDER_CHANNEL_LEDYELLOW  5 // 3
#define ECG_GPIO_EXPANDER_CHANNEL_LEDGREEN   4 // 4

#define ECG_GPIO_EXPANDER_USE_LEDS 0 // Currently, using the LEDs seems to prevent successful reading of the data-ready line (and thus raises concerns for reading leads-off detections too)

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------

int ecg_gpio_expander_setup(int i2c_bus);
void ecg_gpio_expander_cleanup();
int ecg_gpio_expander_read_dataReady();
int ecg_gpio_expander_read_leadsOff_p();
int ecg_gpio_expander_read_leadsOff_n();
void ecg_gpio_expander_set_leds_off();
void ecg_gpio_expander_set_leds_red();
void ecg_gpio_expander_set_leds_yellow();
void ecg_gpio_expander_set_leds_green();
// Lower-level helpers.
int ecg_gpio_expander_read();
int ecg_gpio_expander_parse_dataReady(uint8_t data);
int ecg_gpio_expander_parse_leadsOff_p(uint8_t data);
int ecg_gpio_expander_parse_leadsOff_n(uint8_t data);

#endif // ECG_GPIOEXPANDER_H




