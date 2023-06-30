//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
// Description:  Interfacing with the ADS1219 ADC
// Note: ADC interfacing originally based on https://github.com/OM222O/ADS1219
//-----------------------------------------------------------------------------

#ifndef ECG_ADC_H
#define ECG_ADC_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "ecg_gpioExpander.h" // for reading the ADC data-ready bit
#include "../../utils/logging.h" // for CETI_LOG()
#include "../../utils/timing.h"  // for get_global_time_us
#include "../ecg.h" // for ECG_INVALID_PLACEHOLDER
#include <pigpio.h> // for I2C functions
#include <unistd.h> // for usleep()
#include <stdio.h>  // for printing

// ------------------------------------------
// Definitions/Configuration
// ------------------------------------------

#define ECG_ADC_I2C_ADDRESS  0b1000100
#define ECG_ADC_DEBUG_PRINTOUTS  0

#define ECG_ADC_DATA_READY_PIN  6  // -1 to use the GPIO expander instead of a direct connection
// Set up an interrupt on the data ready pin.
// Requires that the pin defined above is not -1
#define ECG_ADC_DATA_READY_USE_INTERRUPT (0 && (ECG_ADC_DATA_READY_PIN >= 0))
#define ECG_ADC_DATA_READY_INTERRUPT_TIMEOUT_MS 10 // will try to reconfigure ADC if data is not available after this timeout
// Alternatively, set up a timer interrupt.
// The pin defined above can be -1 or a direct pin.
#define ECG_ADC_DATA_READY_USE_TIMER 0

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

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern long g_ecg_adc_latest_reading;
extern long long g_ecg_adc_latest_reading_global_time_us;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------

// Initialization and shutdown
int ecg_adc_setup(int i2c_bus);
void ecg_adc_start();
void ecg_adc_cleanup();
void ecg_adc_powerDown();
int ecg_adc_start_data_acquisition_via_interrupt();
int ecg_adc_start_data_acquisition_via_timer();
int ecg_adc_stop_data_acquisition_via_interrupt();
// Configuration
void ecg_adc_set_gain(uint8_t gain);
void ecg_adc_set_data_rate(int rate);
void ecg_adc_set_conversion_mode(uint8_t mode);
void ecg_adc_set_voltage_reference(uint8_t vref);
void ecg_adc_set_channel(int channel);
// Reading
void ecg_adc_update_data(int* exit_flag, long long timeout_us); // update the global variables of the latest data/timestamp
long ecg_adc_read_singleEnded(int channel, int* exit_flag, long long timeout_us);
long ecg_adc_read_differential_0_1(int* exit_flag, long long timeout_us);
long ecg_adc_read_differential_2_3(int* exit_flag, long long timeout_us);
long ecg_adc_read_differential_1_2(int* exit_flag, long long timeout_us);
long ecg_adc_read_shorted(int* exit_flag, long long timeout_us);
void ecg_adc_data_ready_interrupt_fn(int gpio, int level, uint32_t tick); // callback function
void ecg_adc_data_ready_timer_fn(int sig_num); // callback function
// Lower-level helpers
void ecg_adc_config_reset();
void ecg_adc_config_apply();
void ecg_adc_write_config_register(uint8_t data);
uint8_t ecg_adc_read_register(uint8_t reg);
int ecg_adc_read_data(int* exit_flag, long long timeout_us);
int ecg_adc_read_data_ready();

#endif // ECG_ADC_H






