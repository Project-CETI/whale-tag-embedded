//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
// Description:  Interfacing with the PCA9674 GPIO expander
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
  //i2cWriteByte(ecg_gpio_expander_i2c_device, 0b11111111); // set all to inputs (1 = weakly driven high)

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
    case(PI_BAD_HANDLE):
      CETI_LOG("ecg_gpio_expander_read(): Failed to read (PI_BAD_HANDLE).\n");
      result = -1;
      break;
    case(PI_I2C_READ_FAILED):
      CETI_LOG("ecg_gpio_expander_read(): Failed to read (PI_I2C_READ_FAILED).\n");
      result = -1;
      break;
    default: break;
  }
  return result;
}

// Read the ADC data-ready output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
int ecg_gpio_expander_read_dataReady()
{
  int data = ecg_gpio_expander_read();
  if(data < 0) // There was an error reading the GPIO expander
    return -1;
  return ecg_gpio_expander_parse_dataReady(data);

}

// Read the ECG leads-off detection (positive electrode) output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
int ecg_gpio_expander_read_leadsOff_p()
{
  int data = ecg_gpio_expander_read();
  if(data < 0) // There was an error reading the GPIO expander
    return ECG_LEADSOFF_INVALID_PLACEHOLDER;
  return ecg_gpio_expander_parse_leadsOff_p(data);
}

// Read the ECG leads-off detection (negative electrode) output bit.
// Will first read all inputs of the GPIO expander, then extract the desired bit.
int ecg_gpio_expander_read_leadsOff_n()
{
  int data = ecg_gpio_expander_read();
  if(data < 0) // There was an error reading the GPIO expander
    return ECG_LEADSOFF_INVALID_PLACEHOLDER;
  return ecg_gpio_expander_parse_leadsOff_n(data);
}

// Given a byte of all GPIO expander inputs, extract the ADC data-ready bit.
int ecg_gpio_expander_parse_dataReady(uint8_t data)
{
  return (data >> ECG_GPIO_EXPANDER_CHANNEL_DATAREADY) & 0b00000001;
}

// Given a byte of all GPIO expander inputs, extract the ECG leads-off detection bit (positive electrode).
int ecg_gpio_expander_parse_leadsOff_p(uint8_t data)
{
  return (data >> ECG_GPIO_EXPANDER_CHANNEL_LEADSOFF_P) & 0b00000001;
}

// Given a byte of all GPIO expander inputs, extract the ECG leads-off detection bit (negative electrode).
int ecg_gpio_expander_parse_leadsOff_n(uint8_t data)
{
  return (data >> ECG_GPIO_EXPANDER_CHANNEL_LEADSOFF_N) & 0b00000001;
}

// Turn off all LEDs.
void ecg_gpio_expander_set_leds_off()
{
  #if ECG_GPIO_EXPANDER_USE_LEDS
  // Setting all bits to 1 will set them all to inputs (will turn LEDs off).
  i2cWriteByte(ecg_gpio_expander_i2c_device, 0b11111111);
  #endif
}

// Turn on the green LED (and turn off the other LEDs).
void ecg_gpio_expander_set_leds_green()
{
  #if ECG_GPIO_EXPANDER_USE_LEDS
  // Setting a 0 in the desired position will turn the LED off.
  // Setting all other bits to 1 will keep all other channels as inputs.
  i2cWriteByte(ecg_gpio_expander_i2c_device, (uint8_t)~(1 << (ECG_GPIO_EXPANDER_CHANNEL_LEDGREEN)));
  //i2cWriteByte(ecg_gpio_expander_i2c_device, 0b11101111);
  #endif
}

// Turn on the yellow LED (and turn off the other LEDs).
void ecg_gpio_expander_set_leds_yellow()
{
  #if ECG_GPIO_EXPANDER_USE_LEDS
  // Setting a 0 in the desired position will turn the LED off.
  // Setting all other bits to 1 will keep all other channels as inputs.
  i2cWriteByte(ecg_gpio_expander_i2c_device, (uint8_t)~(1 << (ECG_GPIO_EXPANDER_CHANNEL_LEDYELLOW)));
  //i2cWriteByte(ecg_gpio_expander_i2c_device, 0b11011111);
  #endif
}

// Turn on the red LED (and turn off the other LEDs).
void ecg_gpio_expander_set_leds_red()
{
  #if ECG_GPIO_EXPANDER_USE_LEDS
  // Setting a 0 in the desired position will turn the LED off.
  // Setting all other bits to 1 will keep all other channels as inputs.
  i2cWriteByte(ecg_gpio_expander_i2c_device, (uint8_t)~(1 << (ECG_GPIO_EXPANDER_CHANNEL_LEDRED)));
  //i2cWriteByte(ecg_gpio_expander_i2c_device, 0b10111111);
  #endif
}



