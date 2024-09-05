//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Version    Date          Description
//  0.0.0   10/10/21   Begin work, establish framework
//  2.1.1   06/27/22   Fix first byte bug with SPI, verify 96 KSPS setting
//  2.1.4   11/05/22   Simplify stopAcq()
//  2.1.5   11/24/22   Correct MAX_FILE_SIZE definition
//  2.3.0   03/16/24   Runtime configuration of samplerate & bitdepth
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Setting up Sampling Rates
// SYSCLK is assumed to be 98.304 MHz to realize these exact rates
// MCLK is SYSCLK/4
// Output data rate is MCLK / ( DEC_RATE * MCLK_DIV)
// DCLK must be set fast enough to keep up with ADC output
// Use CAM opcode 1 to write registers 1,4 and 7 in the ADC to adjust rates
// See ADC data sheet (Analog Devices AD7768-4) and sampling.xlsx in the
// project file for more details on registers settings
//
//-----------------------------------------------------------------------------

#include "audio.h"

// Private local headers
#include "../cetiTag.h"
#include "../fpga.h"
#include "../gpio.h"
#include "../iox.h"
#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/error.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/config.h"

// Private system headers
#include <fcntl.h>
#include <FLAC/stream_encoder.h>
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <sched.h>   // to set process priority
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#if !ENABLE_FPGA
int audio_thread_init(void) {
  CETI_ERR("FPGA is not selected for operation, so audio cannot be used");
  return (-1);
}
#else

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
static int audio_buffer_toWrite = 0; // which buffer will be flushed to the output file
static char audio_acqDataFileName[AUDIO_DATA_FILENAME_LEN] = {};
static int audio_acqDataFileLength = 0;
static FLAC__StreamEncoder *flac_encoder = 0;
static FLAC__int32 buff[AUDIO_BUFFER_SIZE_SAMPLE16][AUDIO_CHANNELS] = {0};

static CetiAudioBuffer * shm_audio;
static sem_t * sem_audio_block;
static sem_t * sem_audio_page;

static struct timeval s_file_start_time;
static struct timeval s_block_start_time;

int g_audio_overflow_detected = 0;
int g_audio_force_overflow = 0;
static int audio_overflow_detected_location = -1;

static FILE *audio_status_file = NULL;
static char audio_status_file_notes[256] = "";
static const char *audio_status_file_headers[] = {
    "Overflow",
    "Overflow Detection Location",
    "Start Writing",
    "Done Writing",
    "See SPI Block",
};
static const int num_audio_status_file_headers = sizeof(audio_status_file_headers)/sizeof(*audio_status_file_headers);
static int audio_writing_to_status_file = 0;

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
// Global variables
int g_audio_thread_spi_is_running = 0;
int g_audio_thread_writeData_is_running = 0;

// Static variables
static bool s_audio_initialized = 0;


WTResult wt_audio_init(void) {
    WT_TRY(iox_init());
    // Initialize 5v enable as ouput and drive high.
    WT_TRY(iox_set_mode(IOX_GPIO_5V_EN, IOX_MODE_OUTPUT));
    WT_TRY(iox_write_pin(IOX_GPIO_5V_EN, 1));

    // Initialize audio overflow pin.
    gpioSetMode(AUDIO_OVERFLOW_GPIO, PI_INPUT);

    // Initialize audio data ready pin.
    gpioSetMode(AUDIO_DATA_AVAILABLE, PI_INPUT);

    return WT_OK;
}

int wt_audio_read_data_ready(void) {
    return gpioRead(AUDIO_DATA_AVAILABLE);
}

int wt_audio_read_overflow(void) {
    return gpioRead(AUDIO_OVERFLOW_GPIO);
}

//  Acquisition Hardware Setup and Control Utility Functions
void init_audio_buffers() {
  shm_audio->block = 0;
  shm_audio->page = 0;
  audio_buffer_toWrite = 0;
}

// The setting is checked to ensure it  has been applied internal to the ADC.
int audio_setup(AudioConfig *config){
  if (audio_set_bit_depth(config->bit_depth) != 0){
    CETI_ERR("Failed to set audio bit depth");
    return -1;
  }

  //update filter
  //ToDo: why does the first audio config write fail?
  if (audio_set_filter_type(config->filter_type) != 0){
    CETI_ERR("Failed to set audio filter type");
    return -1;
  }

  if (audio_set_filter_type(config->filter_type) != 0){
    CETI_ERR("Failed to set audio filter type");
    return -1;
  }

  //update sample rate
  int attempt = 0;
  uint16_t result = 0;
  uint16_t expected_result = (config->sample_rate == AUDIO_SAMPLE_RATE_DEFAULT) ? 0x0D
                       : (config->sample_rate == AUDIO_SAMPLE_RATE_48KHZ) ? 0x09
                       : (config->sample_rate == AUDIO_SAMPLE_RATE_96KHZ) ? 0x08
                       : (config->sample_rate == AUDIO_SAMPLE_RATE_192KHZ) ? 0x08
                       : 0;
  do{
    if(attempt == 2){
      CETI_ERR("Failed to set audio ADC sampling to %d kHz - Reg 0x01 reads back 0x%04X expected 0x%04X", config->sample_rate, result, expected_result);
      return -1; // ADC failed to configure as expected
    }
    if (audio_set_sample_rate(config->sample_rate) != 0) {
      CETI_ERR("Failed to set audio ADC sampling rate");
      return -1;
    }
    wt_fpga_adc_read(1, &result); //check that write occured
    attempt++;
  }while(result != expected_result);

  //set channel 4 to standby
  #if AUDIO_CHANNELS==3
  wt_fpga_adc_write(0x00, 0x08);
  #endif

  s_audio_initialized = 1;
  CETI_LOG("Audio successfully initialized");
  return 0;
}

int audio_set_bit_depth(AudioBitDepth bit_depth){
  switch (bit_depth){
    case AUDIO_BIT_DEPTH_16:
    case AUDIO_BIT_DEPTH_24:
      wt_fpga_fifo_set_bitdepth(bit_depth);
      return 0;

    default:
      CETI_ERR("Invalid bit depth");
      return -1;
  }
}

// mode A is left sinc5 filter for energy reasons (see "applications information" in datasheet)
// Channels are swap from mode A to mode B to change filter
int audio_set_filter_type(AudioFilterType filter_type){
  switch(filter_type){
    case AUDIO_FILTER_WIDEBAND:
      #if AUDIO_CHANNELS==3
      wt_fpga_adc_write(0x03, 0x17); //channels 0-2 use mode B
      #elif AUDIO_CHANNELS==4
      wt_fpga_adc_write(0x03, 0x3F); //all channels use mode B
      #endif
      break;
    case AUDIO_FILTER_SINC5:
      wt_fpga_adc_write(0x03, 0x00); //all channels use mode A
      break;

    default:
      CETI_ERR("Invalid filter type");
      return -1;
  }
  return 0;
}

int audio_set_sample_rate(AudioSampleRate sample_rate){
  switch(sample_rate){
    case AUDIO_SAMPLE_RATE_DEFAULT: { // 750HZ
      wt_fpga_adc_write(0x01, (AUDIO_FILTER_SINC5 << 3) | 0b101);    // MODE A: SINC5, DEC_RATE = 1024
      wt_fpga_adc_write(0x02, (AUDIO_FILTER_WIDEBAND << 3) | 0b101); // MODE B: WIDEBAND, DEC_RATE = 1024
      wt_fpga_adc_write(0x04, 0x00); // POWER_MODE = LOW; MCLK_DIV = 32
      wt_fpga_adc_write(0x07, 0x00); // DCLK_DIV = 8
      wt_fpga_adc_sync();       // Apply the settings
      return 0;
    }

    case AUDIO_SAMPLE_RATE_48KHZ: {
      wt_fpga_adc_write(0x01, (AUDIO_FILTER_SINC5 << 3) | 0b001);    // MODE A: SINC5, DEC_RATE = 64
      wt_fpga_adc_write(0x02, (AUDIO_FILTER_WIDEBAND << 3) | 0b001); // MODE B: WIDEBAND, DEC_RATE = 1024
      wt_fpga_adc_write(0x04, 0x22); // POWER_MODE = MID; MCLK_DIV = 8
      wt_fpga_adc_write(0x07, 0x00); // DCLK_DIV = 8
      wt_fpga_adc_sync();       // Apply the settings
      return 0;
    }

    case AUDIO_SAMPLE_RATE_96KHZ: {
      wt_fpga_adc_write(0x01, (AUDIO_FILTER_SINC5 << 3) | 0b000);    // MODE A: SINC5, DEC_RATE = 32
      wt_fpga_adc_write(0x02, (AUDIO_FILTER_WIDEBAND << 3) | 0b000); // MODE B: WIDEBAND, DEC_RATE = 1024
      wt_fpga_adc_write(0x04, 0x22); // POWER_MODE = MID; MCLK_DIV = 8
      wt_fpga_adc_write(0x07, 0x01); // DCLK_DIV = 4
      wt_fpga_adc_sync();       // Apply the settings
      return 0;
    }

    case AUDIO_SAMPLE_RATE_192KHZ: {
      wt_fpga_adc_write(0x01, (AUDIO_FILTER_SINC5 << 3) | 0b000);    // MODE A: SINC5, DEC_RATE = 32
      wt_fpga_adc_write(0x02, (AUDIO_FILTER_WIDEBAND << 3) | 0b000); // MODE B: WIDEBAND, DEC_RATE = 1024
      wt_fpga_adc_write(0x04, 0x33); // POWER_MODE = FAST; MCLK_DIV = 4
      wt_fpga_adc_write(0x07, 0x01); // DCLK_DIV = 4
      wt_fpga_adc_sync();       // Apply the settings
    return 0;
    }

    default: {
      CETI_ERR("Invalid sample rate");
      return -1;
    }
  }
}

int reset_audio_fifo(void) {
  wt_fpga_fifo_stop(); // Stop any incoming data
  wt_fpga_fifo_reset(); // Reset the FIFO
  return 0;
}

//-----------------------------------------------------------------------------
//  Acquisition Start and Stop Controls (TODO add file open/close checking)
//-----------------------------------------------------------------------------

int start_audio_acq(void) {
  CETI_LOG("Starting audio acquisition");
  init_audio_buffers();
#if ENABLE_AUDIO_FLAC
  audio_createNewFlacFile();
#else
  audio_createNewRawFile();
#endif
  wt_fpga_fifo_stop(); // Stop any incoming data
  wt_fpga_fifo_reset(); // Reset the FIFO
  wt_fpga_fifo_start(); // starts the stream
  return 0;
}

int stop_audio_acq(void) {
  CETI_LOG("Stopping audio acquisition");
  wt_fpga_fifo_stop(); // stops the input stream

  // wt_fpga_fifo_reset(); // flushes the FIFO
  return 0;
}

//-----------------------------------------------------------------------------
// SPI thread - Gets Data from HW FIFO on Interrupt
//-----------------------------------------------------------------------------


int audio_thread_init(void) {
#if !ENABLE_RUNTIME_AUDIO
  g_config.audio.bit_depth = AUDIO_BIT_DEPTH_16;
  g_config.audio.sample_rate = AUDIO_SAMPLE_RATE_96KHZ;
#endif
  WTResult hal_result = wt_audio_init();
  if(hal_result != WT_OK) {
    CETI_ERR("Failed to initialize audio hardware: %s", wt_strerror(hal_result));
    return (-1);
  }
  if (!audio_setup(&g_config.audio)) {
    CETI_LOG("Successfully set audio sampling rate to %d kHz", g_config.audio.sample_rate);
  } else {
    CETI_ERR("Failed to set initial audio configuration - ADC register did not read back as expected");
    return (-1);
  }

  //create shared memory region for audio buffer
  shm_audio = create_shared_memory_region(AUDIO_SHM_NAME, sizeof(CetiAudioBuffer));
  if (shm_audio == NULL) {
    CETI_ERR("Failed to create shared memory region");
    return -1;
  }

  //create synchronization semaphores
  sem_audio_block = sem_open(AUDIO_BLOCK_SEM_NAME, O_CREAT, 0644, 0);
  if(sem_audio_block == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create block ready semaphore");
    return -1;
  }

  sem_audio_page = sem_open(AUDIO_BLOCK_SEM_NAME, O_CREAT, 0644, 0);
  if(sem_audio_page == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create page ready semaphore");
    return -1;
  }

  // Open an output file to write status information.
  if (init_data_file(audio_status_file, AUDIO_STATUS_FILEPATH,
                     audio_status_file_headers, num_audio_status_file_headers,
                     audio_status_file_notes, "audio_thread_init()") < 0)
    return -1;

  g_audio_overflow_detected = 0;
  audio_overflow_detected_location = -1;

  return 0;
}

void *audio_thread_spi(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_audio_thread_spi_tid = gettid();

  init_audio_buffers();
  int spi_fd = spiOpen(SPI_CE, SPI_CLK_RATE, 1);

  if (spi_fd < 0) {
    CETI_ERR("pigpio SPI initialisation failed");
    return NULL;
  }

  // Set the thread CPU affinity.
  if (AUDIO_SPI_CPU >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(AUDIO_SPI_CPU, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", AUDIO_SPI_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", AUDIO_SPI_CPU);
  }
  // Set the thread priority.
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_max(SCHED_RR);
  if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
    CETI_LOG("Successfully set priority");
  else
    CETI_WARN("Failed to set priority");

  // Check if the audio is already overflowed.
  audio_check_for_overflow(0);

  // Main loop to acquire audio data.
  g_audio_thread_spi_is_running = 1;
  time_t expected_IQR_interval_us = AUDIO_BLOCK_FILL_SPEED_US(g_config.audio.sample_rate * 1000, g_config.audio.bit_depth);
  time_t retry_sleep_us = expected_IQR_interval_us/20;

  // Initialize state.
  CETI_LOG("Starting loop to fetch data via SPI");
  // Start the audio acquisition on the FPGA.
  gettimeofday(&s_file_start_time, NULL);

  start_audio_acq();

  // Discard the very first byte in the SPI stream.
  char first_byte;
  spiRead(spi_fd, &first_byte, 1);
  while (!g_stopAcquisition && !g_audio_overflow_detected) {
    // Wait for SPI data to be available.
    while (!wt_audio_read_data_ready()) {
      // Reduce the CPU load a bit.
      usleep(retry_sleep_us);
      // Check if the FPGA buffer overflowed.
      continue;
    }

    // Cause an overflow for testing purposes if desired.
    #ifdef DEBUG
    if (g_audio_force_overflow) {
      usleep(100000);
      g_audio_force_overflow = 0;
    }
    #endif

    // Read a block of data if an overflow has not occurred.
    audio_check_for_overflow(2);
    if (g_audio_overflow_detected) {
      break;
    }

    struct timeval current_timeval;
    gettimeofday(&current_timeval, NULL);
    spiRead(spi_fd, shm_audio->data[shm_audio->page].blocks[shm_audio->block], SPI_BLOCK_SIZE);

    // When NUM_SPI_BLOCKS are in the ram buffer, switch to using the other buffer.
    // This will also trigger the writeData thread to write the previous buffer to disk.
    shm_audio->block = (shm_audio->block + 1);
    if (shm_audio->block == AUDIO_BUFFER_SIZE_BLOCKS) {
      shm_audio->block = 0;
      s_block_start_time = current_timeval;
      CETI_DEBUG("%d blocks read", AUDIO_BUFFER_SIZE_BLOCKS);
      shm_audio->page ^= 1; //rotate to page
      // signal buffer is half full event to other processes working with buffered data
      sem_post(sem_audio_page);
    }
    // signal new data for other processes working with live streamed data
    sem_post(sem_audio_block);

    // don't wait if more data is ready
    if (!wt_audio_read_data_ready()) {
      continue;
    }

    // only perform checks/sleep if we have time to
    // Check if the FPGA buffer overflowed.
    audio_check_for_overflow(3);

    //wait until expected next interrupt
    time_t elapsed_time = get_global_time_us() - (int64_t)(current_timeval.tv_sec * 1000000LL) - (int64_t)(current_timeval.tv_usec);
    if (elapsed_time < expected_IQR_interval_us){
      usleep(expected_IQR_interval_us -  elapsed_time);
    } 
  }

  // Close the SPI communication.
  spiClose(spi_fd);

  // Log that the thread is stopping.
  if (g_audio_overflow_detected && !g_stopAcquisition)
    CETI_LOG("*** Audio overflow detected at location %d", audio_overflow_detected_location);
  else
    CETI_LOG("Done!");

  // Wait for the write-data thread to finish as well.
  while (g_audio_thread_writeData_is_running)
    usleep(100000);

  // Stop FPGA audio capture and reset its buffer.
  // stop_audio_acq();
  usleep(100000);
  reset_audio_fifo();
  usleep(100000);
  g_audio_overflow_detected = 0;
  audio_overflow_detected_location = -1;

  // Exit the thread.
  g_audio_thread_spi_is_running = 0;
  return NULL;
}

//-----------------------------------------------------------------------------
// Write Data Thread moves the RAM buffer to mass storage
//-----------------------------------------------------------------------------
void *audio_thread_writeFlac(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_audio_thread_writeData_tid = gettid();

  // Set the thread CPU affinity.
  if (AUDIO_WRITEDATA_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(AUDIO_WRITEDATA_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
  }
  // Set the thread to a low priority.
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_min(SCHED_RR);
  if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
    CETI_LOG("Successfully set priority");
  else
    CETI_WARN("Failed to set priority");

  // Calculate expected file size for given configuration
  size_t filesize_bytes = AUDIO_BUFFER_SIZE_BYTES*(g_config.audio.bit_depth/8);
  filesize_bytes *= (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_96KHZ) ? 2 
                      : (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_192KHZ) ? 4
                      : 1; 
  CETI_DEBUG("Audio file swapping every %lu bytes", filesize_bytes);

  // Wait for the SPI thread to finish initializing and start the main loop.
  while (!g_audio_thread_spi_is_running && !g_stopAcquisition && !g_audio_overflow_detected)
    usleep(1000);

  // Main loop.
  CETI_LOG("Starting loop to periodically write data");
  g_audio_thread_writeData_is_running = 1;

  while (!g_stopAcquisition && !g_audio_overflow_detected) {
    // Wait for the SPI thread to finish filling this buffer.
    // Note that this can use a long delay to yield the CPU,
    //  since the buffer will fill about once per minute and it only takes
    //  about 2 seconds to write the buffer to a file.
    if (audio_buffer_toWrite == shm_audio->page) {
      usleep(1000000);
      continue;
    }

    if ((audio_acqDataFileLength +  AUDIO_BUFFER_SIZE_BYTES > (filesize_bytes - 1)) || (flac_encoder == 0)) {
      s_file_start_time = s_block_start_time;
    }

    // Log that a write is starting.
    long long global_time_us = get_global_time_us();
    while (audio_writing_to_status_file)
      usleep(10);
    audio_writing_to_status_file = 1;
    audio_status_file = fopen(AUDIO_STATUS_FILEPATH, "at");
    fprintf(audio_status_file, "%lld", global_time_us);
    fprintf(audio_status_file, ",%d", getRtcCount());
    // Write any notes, then clear them so they are only written once.
    fprintf(audio_status_file, ",%s", audio_status_file_notes);
    audio_status_file_notes[0] = '\0';
    // Write overflow status information.
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",1");
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",");
    // Finish the row of data.
    fprintf(audio_status_file, "\n");
    fclose(audio_status_file);
    audio_writing_to_status_file = 0;



    // Write the buffer to a file.
    if(g_config.audio.bit_depth == AUDIO_BIT_DEPTH_24){
      for (size_t i_sample = 0; i_sample < AUDIO_BUFFER_SIZE_SAMPLE24; i_sample++) {
        for (size_t i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
          uint8_t *i_ptr = shm_audio->data[audio_buffer_toWrite].sample24[i_sample][i_channel];
          FLAC__int32 value = ((FLAC__int32)i_ptr[0] << 24) | ((FLAC__int32)i_ptr[1] << 16) | ((FLAC__int32)i_ptr[2] << 8);
          buff[i_sample][i_channel] = value / (1 << 8);
        }
      }
      FLAC__stream_encoder_process_interleaved(flac_encoder,&buff[0][0], AUDIO_BUFFER_SIZE_SAMPLE24);
    } else {
      for (size_t i_sample = 0; i_sample < AUDIO_BUFFER_SIZE_SAMPLE16; i_sample++) {
        for (size_t i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
          uint8_t *i_ptr = shm_audio->data[audio_buffer_toWrite].sample16[i_sample][i_channel];
          FLAC__int32 value = ((FLAC__int32)i_ptr[0] << 24) | ((FLAC__int32)i_ptr[1] << 16);
          buff[i_sample][i_channel] = value / (1 << 16);
        }
      }
      FLAC__stream_encoder_process_interleaved(flac_encoder,&buff[0][0], AUDIO_BUFFER_SIZE_SAMPLE16);
    }
    audio_acqDataFileLength += AUDIO_BUFFER_SIZE_BYTES;
    CETI_DEBUG("%d of %lu bytes converted to flac", audio_acqDataFileLength, filesize_bytes);
    
    // Create a new output file if this is the first flush
    //  or if the file size limit has been reached.
    if ((audio_acqDataFileLength > (filesize_bytes - 1)) || (flac_encoder == 0)) {
      audio_createNewFlacFile();
    }

    // Switch to waiting on the other buffer.
    audio_buffer_toWrite = !audio_buffer_toWrite;

    // Log that a write has finished.
    global_time_us = get_global_time_us();
    while (audio_writing_to_status_file)
      usleep(10);
    audio_writing_to_status_file = 1;
    audio_status_file = fopen(AUDIO_STATUS_FILEPATH, "at");
    fprintf(audio_status_file, "%lld", global_time_us);
    fprintf(audio_status_file, ",%d", getRtcCount());
    // Write any notes, then clear them so they are only written once.
    fprintf(audio_status_file, ",%s", audio_status_file_notes);
    audio_status_file_notes[0] = '\0';
    // Write overflow status information.
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",1");
    fprintf(audio_status_file, ",");
    // Finish the row of data.
    fprintf(audio_status_file, "\n");
    fclose(audio_status_file);
    audio_writing_to_status_file = 0;
}
  
  //Flush remaining partial buffer.
  if(shm_audio->block != 0) {
    //Maybe create new file.
    if ((audio_acqDataFileLength > (filesize_bytes - 1)) || (flac_encoder == 0)) {
      audio_createNewFlacFile();
    }

    int bytes_to_flush = (shm_audio->block*SPI_BLOCK_SIZE);
    if (g_config.audio.bit_depth == AUDIO_BIT_DEPTH_24) {
      int samples_to_flush = bytes_to_flush/(AUDIO_CHANNELS * 3);
      CETI_LOG("Flushing partial %d sample buffer.", samples_to_flush);
      for (size_t i_sample = 0; i_sample < samples_to_flush; i_sample++) {
        for (size_t i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
          uint8_t *i_ptr = shm_audio->data[shm_audio->page].sample24[i_sample][i_channel];
          FLAC__int32 value = ((FLAC__int32)i_ptr[0] << 24) | ((FLAC__int32)i_ptr[1] << 16) | ((FLAC__int32)i_ptr[2] << 8);
          buff[i_sample][i_channel] = value / (1 << 8);
        }
      }
      FLAC__stream_encoder_process_interleaved(flac_encoder,&buff[0][0], samples_to_flush);
    } else {
      int samples_to_flush = bytes_to_flush/(AUDIO_CHANNELS * sizeof(uint16_t));
      CETI_LOG("Flushing partial %d sample buffer.", samples_to_flush);
      for (size_t i_sample = 0; i_sample < samples_to_flush; i_sample++) {
        for (size_t i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
          uint8_t *i_ptr = shm_audio->data[shm_audio->page].sample16[i_sample][i_channel];
          FLAC__int32 value = ((FLAC__int32)i_ptr[0] << 24) | ((FLAC__int32)i_ptr[1] << 16);
          buff[i_sample][i_channel] = value / (1 << 16);
        }
      }
      CETI_DEBUG("All samples moved to flac buffer");
      if(flac_encoder == 0){
        CETI_WARN("flac encoder is missing,skipping flushing samples");
      } else if(FLAC__stream_encoder_get_state(flac_encoder) != FLAC__STREAM_ENCODER_OK){
        CETI_WARN("flac encoder in state %s, skipping flushing samples", FLAC__stream_encoder_get_resolved_state_string(flac_encoder));
      }else {
        FLAC__stream_encoder_process_interleaved(flac_encoder,&buff[0][0], samples_to_flush);
      }
      CETI_DEBUG("Flac stream sent");
    }
  }
  CETI_DEBUG("Waiting on flac encoding to end...");
  //Finish current file
  FLAC__stream_encoder_finish(flac_encoder);

  //All data flushed
  CETI_DEBUG("Deleting flac encoder...");
  FLAC__stream_encoder_delete(flac_encoder);
  flac_encoder = 0;
  // Exit the thread.
  if (g_audio_overflow_detected && !g_stopAcquisition)
    CETI_LOG("*** Audio overflow detected at location %d", audio_overflow_detected_location);
  else
    CETI_LOG("Done!");
  g_audio_thread_writeData_is_running = 0;
  return NULL;
}

void audio_createNewFlacFile() {
  FLAC__bool ok = true;
  FLAC__StreamEncoderInitStatus init_status;
  uint32_t flac_bit_depth = g_config.audio.bit_depth;
  uint32_t flac_sample_rate = (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_48KHZ) ? 48000
                              : (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_96KHZ) ? 96000
                              : (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_192KHZ) ? 192000
                              : 750;
  size_t samples_per_page = (g_config.audio.bit_depth == AUDIO_BIT_DEPTH_16) ? AUDIO_BUFFER_SIZE_SAMPLE16 : AUDIO_BUFFER_SIZE_SAMPLE24;

  if (flac_encoder) {
    ok &= FLAC__stream_encoder_finish(flac_encoder);
    CETI_DEBUG("Deleting flac encoder...");
    FLAC__stream_encoder_delete(flac_encoder);
    flac_encoder = 0;
    if (!ok) {
      CETI_LOG("FLAC encoder failed to close for %s", audio_acqDataFileName);
    }
  }

  // filename is the time in ms at the start of audio recording 
  snprintf(audio_acqDataFileName, AUDIO_DATA_FILENAME_LEN, "/data/%ld.flac", (int64_t)(s_file_start_time.tv_sec * 1000LL) - (int64_t)(s_file_start_time.tv_usec)/1000);
  audio_acqDataFileLength = 0;

  /* allocate the encoder */
  if ((flac_encoder = FLAC__stream_encoder_new()) == NULL) {
    CETI_ERR("Failed to allocate FLAC encoder");
    flac_encoder = 0;
    return;
  }

  CETI_DEBUG("Flac configured: channels: %d", AUDIO_CHANNELS);
  CETI_DEBUG("Flac configured: bit depth: %d bits", flac_bit_depth);
  CETI_DEBUG("Flac configured: sample_rate: %d sps", flac_sample_rate);

  ok &= FLAC__stream_encoder_set_channels(flac_encoder, AUDIO_CHANNELS);
  ok &= FLAC__stream_encoder_set_bits_per_sample(flac_encoder, flac_bit_depth);
  ok &= FLAC__stream_encoder_set_sample_rate(flac_encoder, flac_sample_rate);
  ok &= FLAC__stream_encoder_set_total_samples_estimate(flac_encoder, samples_per_page);

  if (!ok) {
    CETI_ERR("FLAC encoder failed to set parameters for %s", audio_acqDataFileName);
    flac_encoder = 0;
    return;
  }

  init_status = FLAC__stream_encoder_init_file(flac_encoder, audio_acqDataFileName, NULL, NULL);
  if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
    CETI_ERR("ERROR: initializing encoder: %s", FLAC__StreamEncoderInitStatusString[init_status]);
    CETI_DEBUG("Deleting flac encoder...");
    FLAC__stream_encoder_delete(flac_encoder);
    flac_encoder = 0;
    return;
  }
  CETI_LOG("Saving hydrophone data to %s", audio_acqDataFileName);
}

static FILE *acqData = NULL; // file for audio recording
void *audio_thread_writeRaw(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_audio_thread_writeData_tid = gettid();

  // Set the thread CPU affinity.
  if (AUDIO_WRITEDATA_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(AUDIO_WRITEDATA_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
    else
      CETI_WARN("Failed to set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
  }
  // Set the thread to a low priority.
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_min(SCHED_RR);
  if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
    CETI_LOG("Successfully set priority");
  else
    CETI_WARN("Failed to set priority");

  // Wait for the SPI thread to finish initializing and start the main loop.
  while (!g_audio_thread_spi_is_running && !g_stopAcquisition && !g_audio_overflow_detected)
    usleep(1000);

  // Main loop.
  CETI_LOG("Starting loop to periodically write data");
  g_audio_thread_writeData_is_running = 1;
  int pageIndex = 0;
  while (!g_stopAcquisition && !g_audio_overflow_detected) {
    // Wait for the SPI thread to finish filling this buffer.
    // Note that this can use a long delay to yield the CPU,
    //  since the buffer will fill about once per minute and it only takes
    //  about 2 seconds to write the buffer to a file.
    while (audio_buffer_toWrite == shm_audio->page && !g_stopAcquisition && !g_audio_overflow_detected)
      usleep(1000000);

    if (!g_stopAcquisition && !g_audio_overflow_detected) {
      // Log that a write is starting.
      long long global_time_us = get_global_time_us();
      while (audio_writing_to_status_file)
        usleep(10);
      audio_writing_to_status_file = 1;
      audio_status_file = fopen(AUDIO_STATUS_FILEPATH, "at");
      fprintf(audio_status_file, "%lld", global_time_us);
      fprintf(audio_status_file, ",%d", getRtcCount());
      // Write any notes, then clear them so they are only written once.
      fprintf(audio_status_file, ",%s", audio_status_file_notes);
      audio_status_file_notes[0] = '\0';
      // Write overflow status information.
      fprintf(audio_status_file, ",");
      fprintf(audio_status_file, ",");
      fprintf(audio_status_file, ",1");
      fprintf(audio_status_file, ",");
      fprintf(audio_status_file, ",");
      // Finish the row of data.
      fprintf(audio_status_file, "\n");
      fclose(audio_status_file);
      audio_writing_to_status_file = 0;

      // Create a new output file if this is the first flush
      //  or if the file size limit has been reached.
      size_t filesize_bytes = AUDIO_BUFFER_SIZE_BYTES*((g_config.audio.bit_depth == AUDIO_BIT_DEPTH_16) ? 2 : 3);
      filesize_bytes *= (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_96KHZ) ? 2 
                        : (g_config.audio.sample_rate == AUDIO_SAMPLE_RATE_192KHZ) ? 4
                        : 1; 
      if ((audio_acqDataFileLength > (filesize_bytes - 1)) || (acqData == NULL)) {
        audio_createNewRawFile();
      }
      // Write the buffer to a file.
      fwrite(shm_audio->data[pageIndex].raw, 1, AUDIO_BUFFER_SIZE_BYTES, acqData);
      audio_acqDataFileLength += AUDIO_BUFFER_SIZE_BYTES;
      fflush(acqData);
      fsync(fileno(acqData));

      // Switch to waiting on the other buffer.
      audio_buffer_toWrite = !audio_buffer_toWrite;

      // Log that a write has finished.
      global_time_us = get_global_time_us();
      while (audio_writing_to_status_file)
        usleep(10);
      audio_writing_to_status_file = 1;
      audio_status_file = fopen(AUDIO_STATUS_FILEPATH, "at");
      fprintf(audio_status_file, "%lld", global_time_us);
      fprintf(audio_status_file, ",%d", getRtcCount());
      // Write any notes, then clear them so they are only written once.
      fprintf(audio_status_file, ",%s", audio_status_file_notes);
      audio_status_file_notes[0] = '\0';
      // Write overflow status information.
      fprintf(audio_status_file, ",");
      fprintf(audio_status_file, ",");
      fprintf(audio_status_file, ",");
      fprintf(audio_status_file, ",1");
      fprintf(audio_status_file, ",");
      // Finish the row of data.
      fprintf(audio_status_file, "\n");
      fclose(audio_status_file);
      audio_writing_to_status_file = 0;
    }
  }

  // Exit the thread.
  if (g_audio_overflow_detected && !g_stopAcquisition)
    CETI_LOG("*** Audio overflow detected at location %d", audio_overflow_detected_location);
  else
    CETI_LOG("Done!");
  g_audio_thread_writeData_is_running = 0;
  return NULL;
}

void audio_createNewRawFile() {
  if (acqData != NULL) {
    fclose(acqData);
  }

  // filename is the time in ms at the start of audio recording
  snprintf(audio_acqDataFileName, AUDIO_DATA_FILENAME_LEN, "/data/%lu.raw", (uint64_t)(s_file_start_time.tv_sec * 1000LL) - (uint64_t)(s_file_start_time.tv_usec)/1000);
  acqData = fopen(audio_acqDataFileName, "wb");
  audio_acqDataFileLength = 0;
  if (!acqData) {
    CETI_LOG("Failed to open %s", audio_acqDataFileName);
    return;
  }
  CETI_LOG("Saving hydrophone data to %s", audio_acqDataFileName);
}

//-----------------------------------------------------------------------------
// Various helpers
//-----------------------------------------------------------------------------

void audio_check_for_overflow(int location_index) {
#if AUDIO_OVERFLOW_GPIO >= 0
  g_audio_overflow_detected = g_audio_overflow_detected || wt_audio_read_overflow();
  if (g_audio_overflow_detected) {
    CETI_LOG("*** OVERFLOW detected at location %d, block %d***", location_index, shm_audio->block);
    audio_overflow_detected_location = location_index;
    long long global_time_us = get_global_time_us();
    while (audio_writing_to_status_file)
      usleep(10);
    audio_writing_to_status_file = 1;
    audio_status_file = fopen(AUDIO_STATUS_FILEPATH, "at");
    fprintf(audio_status_file, "%lld", global_time_us);
    fprintf(audio_status_file, ",%d", getRtcCount());
    // Write any notes, then clear them so they are only written once.
    fprintf(audio_status_file, ",%s", audio_status_file_notes);
    audio_status_file_notes[0] = '\0';
    // Write overflow status information.
    fprintf(audio_status_file, ",%d", g_audio_overflow_detected);
    fprintf(audio_status_file, ",%d", location_index);
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",");
    fprintf(audio_status_file, ",");
    // Finish the row of data.
    fprintf(audio_status_file, "\n");
    fclose(audio_status_file);
    audio_writing_to_status_file = 0;
  }
#endif
}

#endif // !ENABLE_FPGA