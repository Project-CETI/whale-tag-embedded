//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG.c
// Description: Control ECG-related acquisition and logging
//-----------------------------------------------------------------------------

#include "cetiTagECG.h"

// -------------------------------
// Helpers
// -------------------------------
long long get_time_ms()
{
  long long time_ms;
  struct timeval te;
  gettimeofday(&te, NULL);
  time_ms = te.tv_sec * 1000LL + te.tv_usec / 1000;
  return time_ms;
}

// -------------------------------
// Main loop
// -------------------------------
void *ecgThread(void *paramPtr)
{
  // Set up the output log file.
  ecg_log_setup();

  // Set up the GPIO expander.
  //   The ADC code will use it to poll the data-ready output,
  //   and this main loop will use it to read the ECG leads-off detection output.
  ecg_gpio_expander_setup(ECG_I2C_BUS);

  // Set up and configure the ADC.
  ecg_adc_setup(ECG_I2C_BUS);
  ecg_adc_set_voltage_reference(ECG_ADC_VREF_EXTERNAL); // ECG_ADC_VREF_EXTERNAL or ECG_ADC_VREF_INTERNAL
  ecg_adc_set_gain(ECG_ADC_GAIN_ONE); // ECG_ADC_GAIN_ONE or ECG_ADC_GAIN_FOUR
  ecg_adc_set_data_rate(1000); // 20, 90, 330, or 1000
  ecg_adc_set_conversion_mode(ECG_ADC_MODE_CONTINUOUS); // ECG_ADC_MODE_CONTINUOUS or ECG_ADC_MODE_SINGLE_SHOT
  // Start continuous conversion (or a single reading).
  ecg_adc_start();

  // Continuously poll the ADC and the leads-off detection output.
  long ecg_reading = 0;
  int lod_reading  = 0;
  CETI_LOG("ecgThread(): Starting to log data!");
  long long start_time_ms = get_time_ms();
  long long sample_index = 0;
  while(!g_exit)
  {
    // Acquire new readings.
    // Note that the ADC will likely be slower than the GPIO expander,
    //  so read the ECG first to get the readings as close together as possible.
    ecg_reading = ecg_adc_read_singleEnded(ECG_ADC_CHANNEL_ECG);
    lod_reading = ecg_gpio_expander_read_lod();
    // Log the new entries.
    ecg_log_add_entry(sample_index, ecg_reading, lod_reading);
    sample_index++;

//    CETI_LOG("ADC Reading! %ld\n", ecg_reading);
//    CETI_LOG("ADC Reading! %6.3f ", 3.3*(float)ecg_reading/(float)(1 << 23));
//    CETI_LOG("\tLOD Reading! %1d\n", lod_reading);
  }

  // Print the duration and the sampling rate.
  long long duration_ms = get_time_ms() - start_time_ms;
  CETI_LOG("ecgThread(): ECG sampling finished!\n");
  CETI_LOG("  ECG samples : %lld\n", sample_index);
  CETI_LOG("  ECG duration: %lld ms\n", duration_ms);
  CETI_LOG("  ECG rate    : %0.2f Hz\n", 1000.0*(float)sample_index/(float)duration_ms);

  // Clean up.
  ecg_log_cleanup();
  ecg_adc_cleanup();
  ecg_gpio_expander_cleanup();
}





