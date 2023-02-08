//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_gpio_expander.c
// Description: Interfacing with the PCA9674 GPIO expander
//-----------------------------------------------------------------------------

#include "ecg_gpioExpander.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
static int ecg_gpio_expander_i2c_device = 0;

// Initialize and connect the GPIO expander via I2C.
int ecg_gpio_expander_setup(int i2c_bus)
{
  // Initialize I2C/GPIO functionality.
  gpioInitialise();

  // Connect to the GPIO expander.
  ecg_gpio_expander_i2c_device = i2cOpen(i2c_bus, ECG_GPIO_EXPANDER_I2C_ADDRESS, 0);
  if(ecg_gpio_expander_i2c_device < 0)
  {
    CETI_LOG("ecg_gpio_expander_setup(): XXX Failed to connect to the GPIO expander: returned %d.", ecg_gpio_expander_i2c_device);
    switch(ecg_gpio_expander_i2c_device)
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
  CETI_LOG("ecg_gpio_expander_setup(): GPIO expander connected successfully!");

  // Note that all ports are inputs by default at power-on.
  // The below (untested) code should also set them all to inputs,
  //   but will also cause strong pullups to be briefly connected (during HIGH time of the acknowledgment pulse).
  //   To avoid this for now, the default state is leveraged and no commands are sent.
  //i2cWriteByte(ecg_gpio_expander_i2c_device, 0b11111111) // set all to inputs (1 = weakly driven high)

  return 0;
}

// Terminate the I2C connection.
void ecg_gpio_expander_cleanup()
{
  // Commenting the below since the launcher will call gpioTerminate()
  //  as part of the tag-wide cleanup.
  //CETI_LOG("ecg_gpio_expander_cleanup(): Terminating the GPIO interface.\n");
  //gpioTerminate();
}

//-----------------------------------------------------------------------------
// Read/parse data
//-----------------------------------------------------------------------------

// Read all inputs of the GPIO expander.
int ecg_gpio_expander_read()
{
  int result = i2cReadByte(ecg_gpio_expander_i2c_device);
  switch(result)
  {
    case(PI_BAD_HANDLE): CETI_LOG("ecg_gpio_expander_read(): Failed to read (PI_BAD_HANDLE).\n"); break;
    case(PI_I2C_READ_FAILED): CETI_LOG("ecg_gpio_expander_read(): Failed to read (PI_I2C_READ_FAILED).\n"); break;
    default: break;
  }
  return result;
}

// Read the ADC data-ready output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
int ecg_gpio_expander_read_dataReady()
{
  return ecg_gpio_expander_parse_dataReady(ecg_gpio_expander_read());
}

// Read the ECG leads-off detection output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
int ecg_gpio_expander_read_leadsOff()
{
  return ecg_gpio_expander_parse_leadsOff(ecg_gpio_expander_read());
}

// Given a byte of all GPIO expander inputs, extract the ADC data-ready bit.
int ecg_gpio_expander_parse_dataReady(uint8_t data)
{
  return (data >> ECG_GPIO_EXPANDER_CHANNEL_DATAREADY) & 0b00000001;
}

// Given a byte of all GPIO expander inputs, extract the ECG leads-off detection bit.
int ecg_gpio_expander_parse_leadsOff(uint8_t data)
{
  return (data >> ECG_GPIO_EXPANDER_CHANNEL_LEADSOFF) & 0b00000001;
}




