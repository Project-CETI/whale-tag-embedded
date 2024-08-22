
//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "ecg.h"

#include "../utils/memory.h"

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_ecg_thread_getData_is_running = 0;
int g_ecg_thread_writeData_is_running = 0;
static char ecg_data_filepath[100];
static FILE* ecg_data_file = NULL;
static const char* ecg_data_file_headers[] = {
  "Sample Index",
  "ECG",
  "Leads-Off-P",
  "Leads-Off-N",
  };
static const int num_ecg_data_file_headers = sizeof(ecg_data_file_headers)/sizeof(*ecg_data_file_headers);

static int ecg_buffer_select_toWrite = 0; // which buffer will be flushed to the output file
static char ecg_data_file_notes[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH][75];

static CetiEcgBuffer *shm_ecg; // share memory of other processes to directly access samples
static sem_t *sem_ecg_sample;  // semaphore for other processes to sync with new sample becoming available
static sem_t *sem_ecg_page;    // semaphore for other processes to sync with new pages becoming available

int init_ecg() {
  // Initialize the GPIO expander and the ADC.
  init_ecg_electronics();

  // Create shared memory
  shm_ecg = create_shared_memory_region(ECG_SHM_NAME, sizeof(CetiEcgBuffer));
  if (shm_ecg == NULL) {
    CETI_ERR("Failed to create shared memory for ecg data buffer");
    return -1;
  }

  //setup semaphore
  sem_ecg_sample = sem_open(BATTERY_SEM_NAME, O_CREAT, 0644, 0);
  if(sem_ecg_sample == SEM_FAILED){
      CETI_ERR("Failed to create sample semaphore");
      return -1;
  }

  sem_ecg_page = sem_open(BATTERY_SEM_NAME, O_CREAT, 0644, 0);
  if(sem_ecg_page == SEM_FAILED){
      CETI_ERR("Failed to create page semaphore");
      return -1;
  }

  shm_ecg->lod_enabled = ENABLE_ECG_LOD;

  // Open an output file to write data.
  if(init_ecg_data_file(1) < 0)
    return -1;

  return 0;
}

int init_ecg_electronics() {
  // Set up and configure the ADC.
  if(ecg_adc_setup(ECG_I2C_BUS) < 0)
    return -1;
  ecg_adc_set_voltage_reference(ECG_ADC_VREF_EXTERNAL); // ECG_ADC_VREF_EXTERNAL or ECG_ADC_VREF_INTERNAL
  ecg_adc_set_gain(ECG_ADC_GAIN_ONE); // ECG_ADC_GAIN_ONE or ECG_ADC_GAIN_FOUR
  ecg_adc_set_data_rate(1000); // 20, 90, 330, or 1000
  ecg_adc_set_conversion_mode(ECG_ADC_MODE_CONTINUOUS); // ECG_ADC_MODE_CONTINUOUS or ECG_ADC_MODE_SINGLE_SHOT
  ecg_adc_set_channel(ECG_ADC_CHANNEL_ECG);
  // Start continuous conversion (or a single reading).
  ecg_adc_start();

  CETI_LOG("Successfully initialized the ECG electronics");
  CETI_LOG("ECG data-ready pin: %d", ECG_ADC_DATA_READY_PIN);

  return 0;
}

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Determine a new ECG data filename that does not already exist, and open a file for it.
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
                                              ecg_data_file_notes[shm_ecg->page][0],
                                              "init_ecg_data_file()");
  // Change the note from restarted to new file if this is not the first initialization.
  if(!restarted_program)
    strcpy(ecg_data_file_notes[shm_ecg->page][0], "New log file! | ");
  return init_data_file_success;
}

//-----------------------------------------------------------------------------
// Thread to acquire data into a rolling buffer
//-----------------------------------------------------------------------------
void* ecg_thread_getData(void* paramPtr)
{
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_ecg_thread_getData_tid = gettid();

  // Set the thread CPU affinity.
  if(ECG_GETDATA_CPU >= 0)
  {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(ECG_GETDATA_CPU, &cpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", ECG_GETDATA_CPU);
    else
      CETI_LOG("XXX Failed to set affinity to CPU %d", ECG_GETDATA_CPU);
  }
  // Set the thread priority.
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_max(SCHED_RR);
  if(pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
    CETI_LOG("Successfully set priority");
  else
    CETI_LOG("XXX Failed to set priority");

  // Main loop while application is running.
  CETI_LOG("Starting loop to periodically acquire data");
  g_ecg_thread_getData_is_running = 1;

  // Continuously poll the ADC and the leads-off detection output.
  long long prev_ecg_adc_latest_reading_global_time_us = 0;
  shm_ecg->sample = 0;
  long long sample_index = 0;
  long consecutive_zero_ecg_count = 0;
  long instantaneous_sampling_period_us = 0;
  int first_sample = 1;
  int should_reinitialize = 0;
  long long start_time_ms = get_global_time_ms();
  while(!g_stopAcquisition)
  {
    // Request an update of the ECG data, then see if new data was received yet.
    //  The new data may be read immediately by this call after waiting for data to be ready,
    //  or nothing may happen if waiting for an interrupt callback to be triggered.
    ecg_adc_update_data(&g_stopAcquisition, ECG_SAMPLE_TIMEOUT_US);
    if(g_ecg_adc_latest_reading_global_time_us != prev_ecg_adc_latest_reading_global_time_us)
    {
      // Store the new data sample and its timestamp.
      shm_ecg->ecg_readings[shm_ecg->page][shm_ecg->sample] = g_ecg_adc_latest_reading;
      shm_ecg->sys_time_us[shm_ecg->page][shm_ecg->sample] = g_ecg_adc_latest_reading_global_time_us;
      // Update the previous timestamp, for checking whether new data is available.
      instantaneous_sampling_period_us = shm_ecg->sys_time_us[shm_ecg->page][shm_ecg->sample] - prev_ecg_adc_latest_reading_global_time_us;
      prev_ecg_adc_latest_reading_global_time_us = shm_ecg->sys_time_us[shm_ecg->page][shm_ecg->sample];

      #if ENABLE_ECG_LOD
      // Read the GPIO expander for the latest leads-off detection.
      // Assume it's fast enough that the ECG sample timestamp is close enough to this leads-off timestamp.
      ecg_get_latest_leadsOff_detections(
        &shm_ecg->leadsOff_readings_p[shm_ecg->page][shm_ecg->sample],
        &shm_ecg->leadsOff_readings_n[shm_ecg->page][shm_ecg->sample]
      );
      #endif
      
      // Read the RTC.
      shm_ecg->rtc_time_s[shm_ecg->page][shm_ecg->sample] = getRtcCount();

      // Update indexes.
      shm_ecg->sample_indexes[shm_ecg->page][shm_ecg->sample] = sample_index;
      sample_index++;

      // Check if there was an error reading from the ADC.
      // Note that the sample will already be set to ECG_INVALID_PLACEHOLDER
      //  if there was an explicit I2C error communicating with the ADC.
      // But if the ECG board is not connected, then the ADC will seemingly
      //  always have data ready and always return 0.
      // So also check if the ADC returned exactly 0 many times in a row.
      if(shm_ecg->ecg_readings[shm_ecg->page][shm_ecg->sample] == ECG_INVALID_PLACEHOLDER)
      {
        should_reinitialize = 1;
        strcat(ecg_data_file_notes[shm_ecg->page][shm_ecg->sample], "ADC ERROR | ");
        CETI_DEBUG("XXX ADC encountered an error");
      }
      if(shm_ecg->ecg_readings[shm_ecg->page][shm_ecg->sample] == 0)
        consecutive_zero_ecg_count++;
      else
        consecutive_zero_ecg_count = 0;
      if(consecutive_zero_ecg_count > ECG_ZEROCOUNT_THRESHOLD)
      {
        should_reinitialize = 1;
        strcat(ecg_data_file_notes[shm_ecg->page][shm_ecg->sample], "ADC ZEROS | ");
        CETI_DEBUG("ADC returned %ld zero readings in a row", consecutive_zero_ecg_count);
      }

      #if ENABLE_ECG_LOD
      // Check if there was an error communicating with the GPIO expander.
      if(shm_ecg->leadsOff_readings_p[shm_ecg->page][shm_ecg->sample] == ECG_LEADSOFF_INVALID_PLACEHOLDER
         || shm_ecg->leadsOff_readings_n[shm_ecg->page][shm_ecg->sample] == ECG_LEADSOFF_INVALID_PLACEHOLDER)
      {
        strcat(ecg_data_file_notes[shm_ecg->page][shm_ecg->sample], "LO ERROR | ");
        // Note that should_reinitialize is not set to 1 here since the leads-off detection uses separate hardware.
        //   Errors for the relevant hardware will be handled in the LOD thread.
      }
      #endif

      // Check if it took longer than expected to receive the sample (from the ADC and the GPIO expander combined).
      if(instantaneous_sampling_period_us > ECG_SAMPLE_TIMEOUT_US && !first_sample)
      {
        should_reinitialize = 1;
        strcat(ecg_data_file_notes[shm_ecg->page][shm_ecg->sample], "TIMEOUT | ");
        CETI_DEBUG("XXX Reading a sample took %ld us", instantaneous_sampling_period_us);
      }
      first_sample = 0;
      // If the ADC or the GPIO expander had an error,
      //  wait a bit and then try to reconnect to them.
      if(should_reinitialize && !g_stopAcquisition)
      {
        strcat(ecg_data_file_notes[shm_ecg->page][shm_ecg->sample], "INVALID? | ");
        usleep(1000000);
        init_ecg_electronics();
        usleep(10000);
        consecutive_zero_ecg_count = 0;
        first_sample = 1;
        should_reinitialize = 0;
      }

      // Advance the buffer index.
      // If the buffer has filled, switch to the other buffer
      //   (this will also trigger the writeData thread to write the previous buffer to a file).
      shm_ecg->sample++;
      if(shm_ecg->sample == ECG_BUFFER_LENGTH)
      {
        shm_ecg->sample = 0;
        shm_ecg->page++;
        shm_ecg->page %= ECG_NUM_BUFFERS;
        sem_post(sem_ecg_page);
      }
      sem_post(sem_ecg_sample);

      // Clear the next notes.
      strcpy(ecg_data_file_notes[shm_ecg->page][shm_ecg->sample], "");
    }

    // Note that there is no delay to implement a desired sampling rate,
    //  since the rate will be set by the ADC configuration.
  }

  // Print the duration and the sampling rate.
  long long duration_ms = get_global_time_ms() - start_time_ms;
  CETI_LOG("Average rate %0.2f Hz (%lld samples in %lld ms)",
            1000.0*(float)sample_index/(float)duration_ms,
            sample_index, duration_ms);

  // Clean up.
  ecg_adc_cleanup();
  munmap(shm_ecg, sizeof(CetiEcgBuffer));
  sem_close(sem_ecg_sample);
  sem_close(sem_ecg_page);

  g_ecg_thread_getData_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}

//-----------------------------------------------------------------------------
// Thread to write data from the rolling buffer to a file
//-----------------------------------------------------------------------------
void* ecg_thread_writeData(void* paramPtr)
{
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_ecg_thread_writeData_tid = gettid();

  // Set the thread CPU affinity.
  if(ECG_WRITEDATA_CPU >= 0)
  {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(ECG_WRITEDATA_CPU, &cpuset);
    if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", ECG_WRITEDATA_CPU);
    else
      CETI_LOG("XXX Failed to set affinity to CPU %d", ECG_WRITEDATA_CPU);
  }
  // Set the thread to a high priority.
 struct sched_param sp;
 memset(&sp, 0, sizeof(sp));
 sp.sched_priority = sched_get_priority_max(SCHED_RR);
 if(pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
   CETI_LOG("Successfully set priority");
 else
   CETI_LOG("XXX Failed to set priority");

  // Main loop while application is running.
  CETI_LOG("Starting loop to write data as it is acquired");
  g_ecg_thread_writeData_is_running = 1;

  // Continuously wait for new data and then write it to the file.
  while(!g_stopAcquisition)
  {
    // Wait for new data to be in the buffer.
    while(shm_ecg->page == ecg_buffer_select_toWrite && !g_stopAcquisition)
      usleep(250000);

    // Write the last buffer to a file.
    long ecg_data_file_size_b = 0;
    ecg_data_file = fopen(ecg_data_filepath, "at");
    if(ecg_data_file == NULL)
    {
      CETI_LOG("failed to open data output file: %s", ecg_data_filepath);
      init_ecg_data_file(0);
    }
    else
    {
      // Determine the last index to write.
      // During normal operation, will want to write the entire buffer
      //  since the acquisition thread has just finished filling it.
      int ecg_buffer_last_index_toWrite = ECG_BUFFER_LENGTH-1;
      // If the program exited though, will want to write only as much
      //  as the acquisition thread has filled.
      if(shm_ecg->page == ecg_buffer_select_toWrite)
      {
        ecg_buffer_last_index_toWrite = shm_ecg->sample-1;
        if(ecg_buffer_last_index_toWrite < 0)
          ecg_buffer_last_index_toWrite = 0;
      }
      // Write the buffer data to the file.
      for(int ecg_buffer_index_toWrite = 0; ecg_buffer_index_toWrite <= ecg_buffer_last_index_toWrite; ecg_buffer_index_toWrite++)
      {
        // Write timing information.
        fprintf(ecg_data_file, "%lld", shm_ecg->sys_time_us[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        fprintf(ecg_data_file, ",%d", shm_ecg->rtc_time_s[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        // Write any notes.
        fprintf(ecg_data_file, ",%s", ecg_data_file_notes[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        // Write the sensor data.
        fprintf(ecg_data_file, ",%lld", shm_ecg->sample_indexes[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        fprintf(ecg_data_file, ",%ld", shm_ecg->ecg_readings[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        #if ENABLE_ECG_LOD
        fprintf(ecg_data_file, ",%d", shm_ecg->leadsOff_readings_p[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        fprintf(ecg_data_file, ",%d", shm_ecg->leadsOff_readings_n[ecg_buffer_select_toWrite][ecg_buffer_index_toWrite]);
        #else
        fprintf(ecg_data_file, ",,");
        #endif
        // Finish the row of data.
        fprintf(ecg_data_file, "\n");
      }
      // Check the file size and close the file.
      fseek(ecg_data_file, 0L, SEEK_END);
      ecg_data_file_size_b = ftell(ecg_data_file);
      fclose(ecg_data_file);

      // If the file size limit has been reached, start a new file.
      if((ecg_data_file_size_b >= (long)(ECG_MAX_FILE_SIZE_MB)*1024L*1024L || ecg_data_file_size_b < 0) && !g_stopAcquisition)
        init_ecg_data_file(0);

      //CETI_LOG("Wrote %d entries in %lld us", ECG_BUFFER_LENGTH, get_global_time_us() - start_time_us);
    }
    // Advance to the next buffer.
    ecg_buffer_select_toWrite++;
    ecg_buffer_select_toWrite %= ECG_NUM_BUFFERS;
  }

  // Clean up.
  g_ecg_thread_writeData_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}
