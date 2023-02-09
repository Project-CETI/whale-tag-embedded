//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "ecg.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_ecg_thread_is_running = 0;
static char ecg_data_filepath[100];
static FILE* ecg_data_file = NULL;
static char ecg_data_file_notes[256] = "";
static const char* ecg_data_file_headers[] = {
  "Sample Index",
  "ECG",
  "Leads-Off",
  };
static const int num_ecg_data_file_headers = 3;

int init_ecg() {

  // Set up the GPIO expander.
  //   The ADC code will use it to poll the data-ready output,
  //   and this main loop will use it to read the ECG leads-off detection output.
  if(ecg_gpio_expander_setup(ECG_I2C_BUS) < 0)
    return -1;

  // Set up and configure the ADC.
  if(ecg_adc_setup(ECG_I2C_BUS) < 0)
    return -1;
  ecg_adc_set_voltage_reference(ECG_ADC_VREF_EXTERNAL); // ECG_ADC_VREF_EXTERNAL or ECG_ADC_VREF_INTERNAL
  ecg_adc_set_gain(ECG_ADC_GAIN_ONE); // ECG_ADC_GAIN_ONE or ECG_ADC_GAIN_FOUR
  ecg_adc_set_data_rate(1000); // 20, 90, 330, or 1000
  ecg_adc_set_conversion_mode(ECG_ADC_MODE_CONTINUOUS); // ECG_ADC_MODE_CONTINUOUS or ECG_ADC_MODE_SINGLE_SHOT
  // Start continuous conversion (or a single reading).
  ecg_adc_start();

  CETI_LOG("init_ecg(): Successfully initialized the ECG components");

  // Open an output file to write data.
  if(init_ecg_data_file(1) < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Determine a new ECG data filename that does not already exist.
int init_ecg_data_file(int restarted_program)
{
  // Append a number to the filename base until one is found that doesn't exist yet.
  int data_file_postfix_count = 0;
  int data_file_exists = 0;
  do
  {
    sprintf(ecg_data_filepath, "%s_%02d.csv", ECG_DATA_FILEPATH_BASE, data_file_postfix_count);
    data_file_exists = (access(ecg_data_filepath, F_OK) != -1);
    data_file_postfix_count++;
  } while(data_file_exists);

  // Open the new file.
  int init_data_file_success = init_data_file(ecg_data_file, ecg_data_filepath,
                                              ecg_data_file_headers,  num_ecg_data_file_headers,
                                              ecg_data_file_notes, "init_ecg_data_file()");
  // Change the note from restarted to new file if this is not the first initialization.
  if(!restarted_program)
    strcpy(ecg_data_file_notes, "New log file! | ");
  ecg_data_file = fopen(ecg_data_filepath, "at");
  return init_data_file_success;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* ecg_thread(void* paramPtr)
{
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_ecg_thread_tid = gettid();

  // Set the thread priority.
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_max(SCHED_RR);
  if(sched_setscheduler(getpid(), SCHED_RR, &sp) == 0)
    CETI_LOG("ecg_thread(): Successfully set priority");
  else
    CETI_LOG("ecg_thread(): !!! Failed to set priority");
  // Set the thread CPU affinity.
  if(ECG_CPU >= 0)
  {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(ECG_CPU, &cpuset);
    if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("ecg_thread(): Successfully set affinity to CPU %d", ECG_CPU);
    else
      CETI_LOG("ecg_thread(): XXX Failed to set affinity to CPU %d", ECG_CPU);
  }

  // Main loop while application is running.
  CETI_LOG("ecg_thread(): Starting loop to periodically acquire data");
  long long global_time_us;
  int rtc_count;
  g_ecg_thread_is_running = 1;

  // Continuously poll the ADC and the leads-off detection output.
  long ecg_reading = 0;
  int leadsOff_reading  = 0;
  long long start_time_ms = get_global_time_ms();
  long long sample_index = 0;
  long ecg_data_file_size_b = 0;
  while(!g_exit)
  {
    // Acquire timing and sensor information as close together as possible.
    global_time_us = get_global_time_us();
    rtc_count = getRtcCount();
    // Note that the ADC will likely be slower than the GPIO expander,
    //  so read the ECG first to get the readings as close together as possible.
    ecg_reading = ecg_adc_read_singleEnded(ECG_ADC_CHANNEL_ECG);
    leadsOff_reading = ecg_gpio_expander_read_leadsOff();

    //    CETI_LOG("ADC Reading! %ld\n", ecg_reading);
    //    CETI_LOG("ADC Reading! %6.3f ", 3.3*(float)ecg_reading/(float)(1 << 23));
    //    CETI_LOG("\tLeadsOff Reading! %1d\n", leadsOff_reading);

    // Write data to a file.
    ecg_data_file = fopen(ecg_data_filepath, "at");
    if(ecg_data_file == NULL)
    {
      CETI_LOG("ecg_thread(): failed to open data output file: %s", ecg_data_filepath);
      init_ecg_data_file(0);
    }
    else
    {
      // Write timing information.
      fprintf(ecg_data_file, "%lld", global_time_us);
      fprintf(ecg_data_file, ",%d", rtc_count);
      // Write any notes, then clear them so they are only written once.
      fprintf(ecg_data_file, ",%s", ecg_data_file_notes);
      strcpy(ecg_data_file_notes, "");
      // Write the sensor data.
      fprintf(ecg_data_file, ",%lld", sample_index);
      fprintf(ecg_data_file, ",%ld", ecg_reading);
      fprintf(ecg_data_file, ",%d", leadsOff_reading);
      // Finish the row of data.
      fprintf(ecg_data_file, "\n");

      // Check the file size and close the file.
      fseek(ecg_data_file, 0L, SEEK_END);
      ecg_data_file_size_b = ftell(ecg_data_file);
      fclose(ecg_data_file);

      // If the file size limit has been reached, start a new file.
      if(ecg_data_file_size_b >= (long)(ECG_MAX_FILE_SIZE_MB)*1024L*1024L || ecg_data_file_size_b < 0)
        init_ecg_data_file(0);
    }

    // Increment the sample index.
    sample_index++;

    // Note that there is no delay to implement a desired sampling rate,
    //  since the rate will be set by the ADC configuration.
  }

  // Print the duration and the sampling rate.
  long long duration_ms = get_global_time_ms() - start_time_ms;
  CETI_LOG("ecg_thread(): Average rate %0.2f Hz (%lld samples in %lld ms)",
            1000.0*(float)sample_index/(float)duration_ms,
            sample_index, duration_ms);

  // Clean up.
  ecg_adc_cleanup();
  ecg_gpio_expander_cleanup();

  g_ecg_thread_is_running = 0;
  CETI_LOG("ecg_thread(): Done!");
  return NULL;
}





