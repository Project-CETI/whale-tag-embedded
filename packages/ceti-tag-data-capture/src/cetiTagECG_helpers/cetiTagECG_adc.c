//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// ADC interfacing originally based on https://github.com/OM222O/ADS1219
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_adc.c
// Description: Interfacing with the ADS1219 ADC
//-----------------------------------------------------------------------------

#include "cetiTagECG_adc.h"

// Variables declared as extern in the header file.
uint8_t ecg_adc_config;
uint8_t ecg_adc_config_prev;
int ecg_adc_is_singleShot;
int ecg_adc_i2c_device;

// ========================================================
// ==================== INITIALIZATION ====================
// ========================================================

// Initialize the I2C and connect to the ADC.
int ecg_adc_setup(int i2c_bus)
{
  // Initialize I2C/GPIO functionality.
  gpioInitialise();

  // Initialize the data ready pin if it is a direction connection.
  if(ECG_ADC_DATA_READY_PIN >= 0)
    gpioSetMode(ECG_ADC_DATA_READY_PIN, PI_INPUT);

  // Connect to the ADC.
  ecg_adc_i2c_device = i2cOpen(i2c_bus, ECG_ADC_I2C_ADDRESS, 0);
  #if ECG_ADC_DEBUG_PRINTOUTS
  printf("ADC device index: %d\n", ecg_adc_i2c_device);
  #endif
  if(ecg_adc_i2c_device < 0)
  {
    CETI_LOG("ecg_adc_setup(): Failed to connect to the ADC: returned %d.", ecg_adc_i2c_device);
    switch(ecg_adc_i2c_device)
    {
      case(PI_BAD_I2C_BUS): CETI_LOG(" (PI_BAD_I2C_BUS)"); break;
      case(PI_BAD_I2C_ADDR): CETI_LOG(" (PI_BAD_I2C_ADDR)"); break;
      case(PI_BAD_FLAGS): CETI_LOG(" (PI_BAD_FLAGS)"); break;
      case(PI_NO_HANDLE): CETI_LOG(" (PI_NO_HANDLE)"); break;
      case(PI_I2C_OPEN_FAILED): CETI_LOG(" (PI_I2C_OPEN_FAILED)"); break;
      default: CETI_LOG(" (UNKNOWN CODE)"); break;
    }
    CETI_LOG("\n");
    return 0;
  }
  CETI_LOG("ecg_adc_setup(): ADC connected successfully!\n");

  // Initialize state.
  ecg_adc_is_singleShot = 1;
  ecg_adc_config = ECG_ADC_CONFIG_RESET;
  ecg_adc_config_prev = ECG_ADC_CONFIG_RESET;

  // Send a reset command.
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_RESET);
  // Reset the configuration and initialize our ecg_adc_config state.
  ecg_adc_config_reset();

  return 1;
}

// Start continuous conversion if continuous mode is configured,
//  or initiate a single reading if single-shot mode is configured.
void ecg_adc_start()
{
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_START);
}

// Power down the ADC chip.
void ecg_adc_powerDown()
{
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_POWERDOWN);
}

// Terminate the I2C connection.
void ecg_adc_cleanup()
{
  CETI_LOG("ecg_adc_cleanup(): Terminating the GPIO interface.\n");
  gpioTerminate();
}

// ========================================================
// ==================== MODES/SETTINGS ====================
// ========================================================

// Set the gain of the ADC.
// @param gain Can be ECG_ADC_GAIN_ONE or ECG_ADC_GAIN_FOUR.
void ecg_adc_set_gain(uint8_t gain)
{
  ecg_adc_config &= ECG_ADC_GAIN_MASK;
  ecg_adc_config |= gain;
  ecg_adc_config_apply();
}

// Set the data rate of the ADC for continuous conversion mode.
// @param rate Can be 20, 90, 330, or 1000.
void ecg_adc_set_data_rate(int rate)
{
	ecg_adc_config &= ECG_ADC_DATA_RATE_MASK;
	switch(rate)
	{
    case(20):
      ecg_adc_config |= ECG_ADC_DATA_RATE_20;
      break;
    case(90):
      ecg_adc_config |= ECG_ADC_DATA_RATE_90;
      break;
    case(330):
      ecg_adc_config |= ECG_ADC_DATA_RATE_330;
      break;
    case(1000):
      ecg_adc_config |= ECG_ADC_DATA_RATE_1000;
      break;
    default:
      break;
  }
  ecg_adc_config_apply();
}

// Set the conversion mode of the ADC.
// @param mode Can be ECG_ADC_MODE_CONTINUOUS or ECG_ADC_MODE_SINGLE_SHOT.
void ecg_adc_set_conversion_mode(uint8_t mode)
{
  ecg_adc_config &= ECG_ADC_MODE_MASK;
  ecg_adc_config |= mode;
  ecg_adc_config_apply();
  if(mode == ECG_ADC_MODE_CONTINUOUS)
	  ecg_adc_is_singleShot = 0;
  else
	  ecg_adc_is_singleShot = 1;
}

// Set which voltage reference the ADC should use.
// @param vref Can be ECG_ADC_VREF_EXTERNAL or ECG_ADC_VREF_INTERNAL.
void ecg_adc_set_voltage_reference(uint8_t vref)
{
  ecg_adc_config &= ECG_ADC_VREF_MASK;
  ecg_adc_config |= vref;
  ecg_adc_config_apply();
}

// ========================================================
// =================== CONFIG/REGISTERS ===================
// ========================================================

// Write configuration data to the ADC chip.
void ecg_adc_write_config_register(uint8_t new_config_data)
{
  i2cWriteByteData(ecg_adc_i2c_device, ECG_ADC_CONFIG_REGISTER_ADDRESS, new_config_data);
}

// Apply the currently stored configuration to the ADC chip.
// Will only send the command if the current configuration is different than the previous configuration.
void ecg_adc_config_apply()
{
  if(ecg_adc_config != ecg_adc_config_prev)
  {
    ecg_adc_write_config_register(ecg_adc_config);
    ecg_adc_config_prev = ecg_adc_config;
  }
}

// Reset the ADC configuration to default values.
void ecg_adc_config_reset()
{
  ecg_adc_config = ECG_ADC_CONFIG_RESET;
  ecg_adc_config_prev = ecg_adc_config;
	ecg_adc_write_config_register(ecg_adc_config);
}

// Read data from a specified register on the ADC chip.
uint8_t ecg_adc_read_register(uint8_t reg)
{
  int result = i2cReadByteData(ecg_adc_i2c_device, reg);
  if(result == PI_BAD_HANDLE || result == PI_I2C_READ_FAILED)
    CETI_LOG("ecg_adc_read_register(): Failed to read the desired register.\n");
  return result;
}

// ========================================================
// ======================= READ DATA ======================
// ========================================================

// Read, parse, and return data.
// Assumes ecg_adc_config is already set for the desired channel.
long ecg_adc_read_data()
{
  // Start conversion if needed (if not using continuous mode).
  if(ecg_adc_is_singleShot)
    ecg_adc_start();

  // Wait for the data to be ready.
  while(ecg_adc_read_data_ready() == 1);

  // Read the data!
  uint8_t result_length = 3;
  char result_bytes[3] = {0};
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_RREG);
  i2cReadDevice(ecg_adc_i2c_device, result_bytes, result_length);
  #if ECG_ADC_DEBUG_PRINTOUTS
  printf(" \t\t %d %d %d  \t\t ", result_bytes[0], result_bytes[1], result_bytes[2]);
  #endif

  // Parse the data bytes into a single long number.
  long data32 = result_bytes[0];
  data32 <<= 8;
  data32 |= result_bytes[1];
  data32 <<= 8;
  data32 |= result_bytes[2];
  return (data32 << 8) >> 8;
}

// Read a single-ended measurement from the desired channel.
// Will update the ADC condiguration if needed
//  (if the specified channel is different than the one previously read).
// @param channel Can be 0, 1, 2, or 3.
long ecg_adc_read_singleEnded(int channel)
{
  // Update the configuration for the desired channel if needed.
	ecg_adc_config &= ECG_ADC_MUX_MASK;
	switch(channel)
	{
    case(0):
      ecg_adc_config |= ECG_ADC_MUX_SINGLE_0;
      break;
    case(1):
      ecg_adc_config |= ECG_ADC_MUX_SINGLE_1;
      break;
    case(2):
      ecg_adc_config |= ECG_ADC_MUX_SINGLE_2;
      break;
    case(3):
      ecg_adc_config |= ECG_ADC_MUX_SINGLE_3;
      break;
	default:
	  break;
  }
  ecg_adc_config_apply(); // Will only send the config if it has changed.
  // Read the data.
  return ecg_adc_read_data();
}

// Read a differential measurement between channels 0 and 1.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_differential_0_1()
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_DIFF_0_1;
  ecg_adc_config_apply();
  // Read the data.
  return ecg_adc_read_data();
}

// Read a differential measurement between channels 2 and 3.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_differential_2_3()
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_DIFF_2_3;
  ecg_adc_config_apply();
  // Read the data.
  return ecg_adc_read_data();
}

// Read a differential measurement between channels 1 and 2.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_differential_1_2()
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_DIFF_1_2;
  ecg_adc_config_apply();
  // Read the data.
  return ecg_adc_read_data();
}

// Read a measurement with the positive and negative inputs both shorted to AVDD/2.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_shorted()
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_SHORTED;
  ecg_adc_config_apply();
  // Read the data.
  return ecg_adc_read_data();
}

// ========================================================
// ======================= HELPERS ========================
// ========================================================

// Read the data-ready bit, either via a direct connection or via the GPIO expander.
int ecg_adc_read_data_ready()
{
  if(ECG_ADC_DATA_READY_PIN >= 0)
    return gpioRead(ECG_ADC_DATA_READY_PIN);
  else
    return ecg_gpio_expander_read_dataReady();
}






