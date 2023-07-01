//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
// Description:  Interfacing with the ADS1219 ADC
// Note: ADC interfacing originally based on https://github.com/OM222O/ADS1219
//-----------------------------------------------------------------------------

#include "ecg_adc.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Static variables
static uint8_t ecg_adc_config = 0;
static uint8_t ecg_adc_config_prev = 0;
static int ecg_adc_is_singleShot = 0;
static int ecg_adc_i2c_device = 0;
static int ecg_adc_channel = ECG_ADC_CHANNEL_ECG;
// Global variables
long g_ecg_adc_latest_reading = ECG_INVALID_PLACEHOLDER;
long long g_ecg_adc_latest_reading_global_time_us = 0;

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
    CETI_LOG("ecg_adc_setup(): XXX Failed to connect to the ADC: returned %d.", ecg_adc_i2c_device);
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
    return -1;
  }
  CETI_LOG("ecg_adc_setup(): ADC connected successfully!");

  // Initialize state.
  ecg_adc_is_singleShot = 1;
  ecg_adc_config = ECG_ADC_CONFIG_RESET;
  ecg_adc_config_prev = ECG_ADC_CONFIG_RESET;

  // Send a reset command.
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_RESET);
  // Reset the configuration and initialize our ecg_adc_config state.
  ecg_adc_config_reset();

  return 0;
}

// Start continuous conversion if continuous mode is configured,
//  or initiate a single reading if single-shot mode is configured.
void ecg_adc_start()
{
  // Start continuous conversion or a single reading.
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_START);

  // Enable the interrupt callback for continuous acquisition if desired.
  #if ECG_ADC_DATA_READY_USE_INTERRUPT
  ecg_adc_start_data_acquisition_via_interrupt();
  #endif
  // Enable the timer callback for continuous acquisition if desired.
  #if ECG_ADC_DATA_READY_USE_TIMER
  ecg_adc_start_data_acquisition_via_timer();
  #endif
}

// Power down the ADC chip.
void ecg_adc_powerDown()
{
  // Power down the ADC chip.
  i2cWriteByte(ecg_adc_i2c_device, ECG_ADC_CMD_POWERDOWN);

  // Stop the interrupt callback for continuous acquisition if needed.
  #if ECG_ADC_DATA_READY_USE_INTERRUPT
  ecg_adc_stop_data_acquisition_via_interrupt();
  #endif
}

// Stop data acquisition and terminate the I2C connection.
void ecg_adc_cleanup()
{
  // Stop the interrupt callback for continuous acquisition if needed.
  #if ECG_ADC_DATA_READY_USE_INTERRUPT
  ecg_adc_stop_data_acquisition_via_interrupt();
  #endif

  // Commenting the below since the launcher will call gpioTerminate()
  //  as part of the tag-wide cleanup.
  //CETI_LOG("ecg_adc_cleanup(): Terminating the GPIO interface.\n");
  //gpioTerminate();
}

//-----------------------------------------------------------------------------
// Modes/Settings
//-----------------------------------------------------------------------------

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

// Set which channel to use for the main ECG signal.
void ecg_adc_set_channel(int channel)
{
  ecg_adc_channel = channel;
}

//-----------------------------------------------------------------------------
// Config/Registers
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// Read Data
//-----------------------------------------------------------------------------

// Start continuous sampling via a data-ready interrupt pin.
int ecg_adc_start_data_acquisition_via_interrupt()
{
  #ifdef ECG_ADC_DATA_READY_USE_INTERRUPT
  int res = gpioSetISRFunc(ECG_ADC_DATA_READY_PIN, FALLING_EDGE,
                           ECG_ADC_DATA_READY_INTERRUPT_TIMEOUT_MS,
                           ecg_adc_data_ready_interrupt_fn);
  switch(res)
  {
    case PI_BAD_GPIO: CETI_LOG("ecg_adc_start_data_acquisition_via_interrupt(): XXX PI_BAD_GPIO"); break;
    case PI_BAD_EDGE: CETI_LOG("ecg_adc_start_data_acquisition_via_interrupt(): XXX PI_BAD_EDGE"); break;
    case PI_BAD_ISR_INIT: CETI_LOG("ecg_adc_start_data_acquisition_via_interrupt(): XXX PI_BAD_ISR_INIT"); break;
    case 0: CETI_LOG("ecg_adc_start_data_acquisition_via_interrupt(): Attached data-ready interrupt callback"); break;
  }
  return res == 0 ? 0 : -1;
  #else
  return -1;
  #endif
}

// Start continuous sampling via a timer.
int ecg_adc_start_data_acquisition_via_timer()
{
  #if ECG_ADC_DATA_READY_USE_TIMER
  // Try to reach a point in the ADC cycle just a bit after data is ready.
  int data_is_ready = 0;
  long long wait_for_data_startTime_us = get_global_time_us();
  long timeout_us = 1000000;
  while(!data_is_ready
            && get_global_time_us() - wait_for_data_startTime_us < timeout_us)
      data_is_ready = (ecg_adc_read_data_ready() == 0 ? 1 : 0);
  usleep(50);
  // Start the timer.
  signal(SIGALRM, ecg_adc_data_ready_timer_fn);
  ualarm(1000, 1000);
  return 0;
  #else
  return -1;
  #endif
}

// Stop continuous acquisition via a data-ready pin interrupt.
int ecg_adc_stop_data_acquisition_via_interrupt()
{
  #if ECG_ADC_DATA_READY_USE_INTERRUPT
  // Cancel the callback.
  int res = gpioSetISRFunc(ECG_ADC_DATA_READY_PIN, FALLING_EDGE,
                           ECG_ADC_DATA_READY_INTERRUPT_TIMEOUT_MS,
                           NULL);
  switch(res)
  {
    case PI_BAD_GPIO: CETI_LOG("ecg_adc_stop_data_acquisition_via_interrupt(): XXX PI_BAD_GPIO"); break;
    case PI_BAD_EDGE: CETI_LOG("ecg_adc_stop_data_acquisition_via_interrupt(): XXX PI_BAD_EDGE"); break;
    case PI_BAD_ISR_INIT: CETI_LOG("ecg_adc_stop_data_acquisition_via_interrupt(): XXX PI_BAD_ISR_INIT"); break;
    case 0: CETI_LOG("ecg_adc_stop_data_acquisition_via_interrupt(): Removed data-ready interrupt callback"); break;
  }
  return res == 0 ? 0 : -1;
  #else
  return -1;
  #endif
}

// Define a callback function that will trigger with the data-ready pin interrupt.
void ecg_adc_data_ready_interrupt_fn(int gpio, int level, uint32_t tick)
{
  #if ECG_ADC_DATA_READY_USE_INTERRUPT
  // Check if the timeout was reached
  if(level == 2)
  {
    // Indicate that there was an error, so the main program will try to set up the ECG again.
    g_ecg_adc_latest_reading = ECG_INVALID_PLACEHOLDER;
    // Update the data timestamp, which will also trigger the main program to read the new sample/timestamp.
    g_ecg_adc_latest_reading_global_time_us = get_global_time_us();
  }
  else
  {
    // Acquire the new data sample and timestamp.
    // Will update g_ecg_adc_latest_reading and g_ecg_adc_latest_reading_global_time_us.
    ecg_adc_read_singleEnded(ecg_adc_channel, NULL, -1); // exit_flag and timeout_us are not needed since the interrupt implies data is already ready
  }
  #endif
}

// Define a callback function that will trigger with the data-ready timer.
void ecg_adc_data_ready_timer_fn(int sig_num)
{
  #if ECG_ADC_DATA_READY_USE_TIMER
  if(sig_num == SIGALRM)
  {
    // Acquire the new data sample and timestamp.
    // Will update g_ecg_adc_latest_reading and g_ecg_adc_latest_reading_global_time_us.
    ecg_adc_read_singleEnded(ecg_adc_channel, NULL, -1); // exit_flag and timeout_us are not needed since the interrupt implies data is already ready
  }
  #endif
}

// Get the latest ECG sample, either by actually reading the ADC or by
//  using the latest interrupt/timer-based sample.
// The calling program can then check the sample timestamp to see if a new sample was received.
void ecg_adc_update_data(int* exit_flag, long long timeout_us)
{
  // If an interrupt/timer is used, do nothing since the callback will update the data.
  // Otherwise, acquire the new data sample and timestamp.
  // Will update g_ecg_adc_latest_reading and g_ecg_adc_latest_reading_global_time_us.
  #if !(ECG_ADC_DATA_READY_USE_INTERRUPT || ECG_ADC_DATA_READY_USE_TIMER)
  ecg_adc_read_singleEnded(ecg_adc_channel, exit_flag, timeout_us);
  #endif
}

// Read, parse, and return data.
// Assumes ecg_adc_config is already set for the desired channel.
int ecg_adc_read_data(int* exit_flag, long long timeout_us)
{
  // Start conversion if needed (if not using continuous mode).
  if(ecg_adc_is_singleShot)
    ecg_adc_start();

  // Wait for the data to be ready, unless the interrupt is being used
  //  (in which case assume this function is called since data is ready).
  #if !(ECG_ADC_DATA_READY_USE_INTERRUPT || ECG_ADC_DATA_READY_USE_TIMER)
  int data_is_ready = 0;
  long long wait_for_data_startTime_us = get_global_time_us();
  while(!data_is_ready
          && !*exit_flag
          && get_global_time_us() - wait_for_data_startTime_us < timeout_us)
    data_is_ready = (ecg_adc_read_data_ready() == 0 ? 1 : 0);
  if(!data_is_ready)
  {
    // Indicate that there was an error, so the main program will try to set up the ECG again.
    g_ecg_adc_latest_reading = ECG_INVALID_PLACEHOLDER;
    // Update the data timestamp, which will also trigger the main program to read the new sample/timestamp.
    g_ecg_adc_latest_reading_global_time_us = get_global_time_us();
    return -1;
  }
  #endif

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
  g_ecg_adc_latest_reading = (data32 << 8) >> 8;

  // Update the data timestamp, which will also trigger the main program to read the new sample/timestamp.
  g_ecg_adc_latest_reading_global_time_us = get_global_time_us();
  return 0;
}

// Read a single-ended measurement from the desired channel.
// Will update the ADC condiguration if needed
//  (if the specified channel is different than the one previously read).
// @param channel Can be 0, 1, 2, or 3.
long ecg_adc_read_singleEnded(int channel, int* exit_flag, long long timeout_us)
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
  ecg_adc_read_data(exit_flag, timeout_us);
  return g_ecg_adc_latest_reading;
}

// Read a differential measurement between channels 0 and 1.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_differential_0_1(int* exit_flag, long long timeout_us)
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_DIFF_0_1;
  ecg_adc_config_apply();
  // Read the data.
  ecg_adc_read_data(exit_flag, timeout_us);
  return g_ecg_adc_latest_reading;
}

// Read a differential measurement between channels 2 and 3.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_differential_2_3(int* exit_flag, long long timeout_us)
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_DIFF_2_3;
  ecg_adc_config_apply();
  // Read the data.
  ecg_adc_read_data(exit_flag, timeout_us);
  return g_ecg_adc_latest_reading;
}

// Read a differential measurement between channels 1 and 2.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_differential_1_2(int* exit_flag, long long timeout_us)
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_DIFF_1_2;
  ecg_adc_config_apply();
  // Read the data.
  ecg_adc_read_data(exit_flag, timeout_us);
  return g_ecg_adc_latest_reading;
}

// Read a measurement with the positive and negative inputs both shorted to AVDD/2.
// Will update the ADC configuration if needed
//  (if the previous read was for a different channel subset).
long ecg_adc_read_shorted(int* exit_flag, long long timeout_us)
{
  // Update the configuration for the desired channel if needed.
  ecg_adc_config &= ECG_ADC_MUX_MASK;
  ecg_adc_config |= ECG_ADC_MUX_SHORTED;
  ecg_adc_config_apply();
  // Read the data.
  ecg_adc_read_data(exit_flag, timeout_us);
  return g_ecg_adc_latest_reading;
}

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Read the data-ready bit, either via a direct connection or via the GPIO expander.
int ecg_adc_read_data_ready()
{
  if(ECG_ADC_DATA_READY_PIN >= 0)
    return gpioRead(ECG_ADC_DATA_READY_PIN);
  else
    return ecg_gpio_expander_read_dataReady();
}






