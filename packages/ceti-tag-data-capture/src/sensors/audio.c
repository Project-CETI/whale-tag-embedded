//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Version    Date          Description
//  0.0.0   10/10/21   Begin work, establish framework
//  2.1.1   06/27/22   Fix first byte bug with SPI, verify 96 KSPS setting
//  2.1.4   11/5/22    Simplify stopAcq()
//  2.1.5   11/24/22   Correct MAX_FILE_SIZE definition
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

#if !ENABLE_FPGA
int init_audio() {
  CETI_LOG("init_audio(): XXXX FPGA is not selected for operation, so audio cannot be used");
  return (-1);
}
#else

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
static char audio_acqDataFileName[AUDIO_DATA_FILENAME_LEN] = {};
static int audio_acqDataFileLength = 0;
static FLAC__StreamEncoder* flac_encoder = 0;
static FLAC__int32 buff[SAMPLES_PER_RAM_PAGE*CHANNELS] = {0};
struct audio_dataPage {
    char buffer[RAM_SIZE];
    int counter;
    bool readyToBeSavedToDisk;
};
static struct audio_dataPage audio_page[2];

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
// Global variables
int g_audio_thread_spi_is_running = 0;
int g_audio_thread_writeData_is_running = 0;

int init_audio() {
  // Set the ADC Up with defaults (96 kHz)
  if (!setup_audio_96kHz())
      CETI_LOG("init_audio(): Successfully set audio sampling rate to 96 kHz");
  else {
      CETI_LOG("init_audio(): XXXX Failed to set initial audio configuration - ADC register did not read back as expected");
      return (-1);
  }
  return 0;
}

//  Acquisition Hardware Setup and Control Utility Functions
void init_pages() {
    audio_page[0].counter = 0;
    audio_page[0].readyToBeSavedToDisk = false;
    audio_page[1].counter = 0;
    audio_page[1].readyToBeSavedToDisk = false;
}

int setup_audio_default(void) {
    char cam_response[8];
    cam(1, 0x01, 0x0D, 0, 0, cam_response); // DEC_RATE = 1024
    cam(1, 0x04, 0x00, 0, 0, cam_response); // POWER_MODE = LOW; MCLK_DIV = 32
    cam(1, 0x07, 0x00, 0, 0, cam_response); // DCLK_DIV = 8
    cam(2, 0, 0, 0, 0, cam_response);       // Apply the settings
    return 0;
}

int setup_audio_48kHz(void) {
    char cam_response[8];
    cam(1, 0x01, 0x09, 0, 0, cam_response); // DEC_RATE = 64
    cam(1, 0x04, 0x22, 0, 0, cam_response); // POWER_MODE = MID; MCLK_DIV = 8
    cam(1, 0x07, 0x00, 0, 0, cam_response); // DCLK_DIV = 8
    cam(2, 0, 0, 0, 0, cam_response);       // Apply the settings
    return 0;
}

// Initial deployments will use 96 KSPS only. The setting is checked to ensure
// it  has been applied internal to the ADC.

int setup_audio_96kHz(void)   //Initial deployments will use 96 KSPS only. The setting is checked
{
    char cam_response[8];
    cam(1,0x01,0x08,0,0,cam_response);  //DEC_RATE = 32
    cam(1,0x04,0x22,0,0,cam_response);  //POWER_MODE = MID; MCLK_DIV = 8
    cam(1,0x07,0x01,0,0,cam_response);  //DCLK_DIV = 4
    cam(2,0,0,0,0,cam_response);        //Apply the settings

    // readback check first attempt
    cam(1,0x81,0,0,0,cam_response);    //must run x2 - first one loads the buffer in the ADC,
    cam(1,0,0,0,0,cam_response);       //second time  dummy clocks to get result

    if (cam_response[5] != 0x08) {      //try to set it again if it fails first time

        cam(1,0x01,0x08,0,0,cam_response);  //DEC_RATE = 32
        cam(1,0x04,0x22,0,0,cam_response);  //POWER_MODE = MID; MCLK_DIV = 8
        cam(1,0x07,0x01,0,0,cam_response);  //DCLK_DIV = 4
        cam(2,0,0,0,0,cam_response);        //Apply the settings

        // readback checks, 2nd attempt
        cam(1,0x81,0,0,0,cam_response);    //must run x2 - first one loads the buffer in the ADC,
        cam(1,0,0,0,0,cam_response);       //second time  dummy clocks to get result

        if (cam_response[5] != 0x08) {
            CETI_LOG("Failed to set audio ADC sampling to 96 kHz - Reg 0x01 reads back 0x%02X%02X should be 0x0008 ", cam_response[4],cam_response[5]);
            return -1; //ADC failed to configure as expected
        }
    }

    CETI_LOG("Successfully set audio ADC sampling to 96 kHz - Reg 0x01 reads back 0x%02X%02X should be 0x0008 ", cam_response[4],cam_response[5]);
    return 0;
}

int setup_audio_192kHz(void) {
    char cam_response[8];
    cam(1, 0x01, 0x08, 0, 0, cam_response); // DEC_RATE = 32
    cam(1, 0x04, 0x33, 0, 0, cam_response); // POWER_MODE = FAST; MCLK_DIV = 4
    cam(1, 0x07, 0x01, 0, 0, cam_response); // DCLK_DIV = 4
    cam(2, 0, 0, 0, 0, cam_response);       // Apply the settings
    return 0;
}

int reset_audio_fifo(void) {
    char cam_response[8];
    cam(5, 0x01, 0x08, 0, 0, cam_response); // Stop any incoming data
    cam(3, 0x04, 0x33, 0, 0, cam_response); // Reset the FIFO
    return 0;
}

//-----------------------------------------------------------------------------
//  Acquisition Start and Stop Controls (TODO add file open/close checking)
//-----------------------------------------------------------------------------

int start_audio_acq(void) {
    char cam_response[8];
    cam(5, 0, 0, 0, 0, cam_response); // stops the input stream
    cam(3, 0, 0, 0, 0, cam_response); // flushes the FIFO
    init_pages();
    #if ENABLE_AUDIO_FLAC
    audio_createNewFlacFile();
    #else
    audio_createNewRawFile();
    #endif
    cam(4, 0, 0, 0, 0, cam_response); // starts the stream
    return 0;
}

int stop_audio_acq(void) {
    char cam_response[8];
    cam(5, 0, 0, 0, 0, cam_response); // stops the input stream

    if (flac_encoder) {
        FLAC__stream_encoder_finish(flac_encoder);
        FLAC__stream_encoder_delete(flac_encoder);
        flac_encoder = 0;
    }

//    cam(3, 0, 0, 0, 0, cam_response); // flushes the FIFO
    return 0;
}

//-----------------------------------------------------------------------------
// SPI thread - Gets Data from HW FIFO on Interrupt
//-----------------------------------------------------------------------------
static char first_byte=1;  //first byte in stream must be discarded

void* audio_thread_spi(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_audio_thread_spi_tid = gettid();

    int pageIndex = 0;
    init_pages();
    int spi_fd = spiOpen(SPI_CE, SPI_CLK_RATE, 1);

    if (spi_fd < 0) {
        CETI_LOG("audio_thread_spi(): pigpio SPI initialisation failed");
        return NULL;
    }

    /* Set GPIO modes */
    gpioSetMode(AUDIO_DATA_AVAILABLE, PI_INPUT);

    // Set the thread CPU affinity.
    if(AUDIO_SPI_CPU >= 0)
    {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(AUDIO_SPI_CPU, &cpuset);
      if(pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("audio_thread_spi(): Successfully set affinity to CPU %d", AUDIO_SPI_CPU);
      else
        CETI_LOG("audio_thread_spi(): XXX Failed to set affinity to CPU %d", AUDIO_SPI_CPU);
    }
    // Set the thread priority.
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_max(SCHED_RR);
    if(pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
      CETI_LOG("audio_thread_spi(): Successfully set priority");
    else
      CETI_LOG("audio_thread_spi(): XXX Failed to set priority");

    // Initialize state.
    volatile int status;
    volatile int prev_status;
    prev_status = gpioRead(AUDIO_DATA_AVAILABLE);
    status = prev_status;

    // Main loop to acquire audio data.
    CETI_LOG("audio_thread_spi(): Starting loop to fetch data via SPI");
    g_audio_thread_spi_is_running = 1;
    while (!g_exit) {

        status = gpioRead(AUDIO_DATA_AVAILABLE);

        if (status != prev_status) {
            prev_status = status;

            while (status) {
                // SPI block has become available, read it from the HW FIFO via
                // SPI. Discard the first byte.

                if (first_byte) {
                    spiRead(spi_fd,&first_byte,1);
                    first_byte = 0;
                }

                spiRead(spi_fd,
                        audio_page[pageIndex].buffer +
                            (audio_page[pageIndex].counter * SPI_BLOCK_SIZE),
                        SPI_BLOCK_SIZE);

                // When NUM_SPI_BLOCKS are in the ram buffer, set flag that
                // triggers a transfer from RAM to mass storage, then flip the
                // data audio_page and continue get samples from SPI.
                if (audio_page[pageIndex].counter == NUM_SPI_BLOCKS - 1) {
                    audio_page[pageIndex].readyToBeSavedToDisk = true;
                    audio_page[pageIndex].counter = 0; // reset for next chunk
                    pageIndex = !pageIndex;
                } else {
                    audio_page[pageIndex].counter++;
                }
                status = gpioRead(AUDIO_DATA_AVAILABLE);
            }
        } else {
            usleep(1000);
        }
    }
    spiClose(spi_fd);
    g_audio_thread_spi_is_running = 0;
    CETI_LOG("audio_thread_spi(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Write Data Thread moves the RAM buffer to mass storage
//-----------------------------------------------------------------------------
void* audio_thread_writeFlac(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_audio_thread_writeData_tid = gettid();

    // Set the thread CPU affinity.
    if(AUDIO_WRITEDATA_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(AUDIO_WRITEDATA_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("audio_thread_writeData(): Successfully set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
      else
        CETI_LOG("audio_thread_writeData(): XXX Failed to set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
    }
    // Set the thread to a low priority.
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR);
    if(pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
      CETI_LOG("audio_thread_writeFlac(): Successfully set priority");
    else
      CETI_LOG("audio_thread_writeFlac(): XXX Failed to set priority");

    CETI_LOG("audio_thread_writeData(): Starting loop to periodically write data");
    g_audio_thread_writeData_is_running = 1;
    // FLAC__bool ok = true;
    int pageIndex = 0;
    while (!g_exit) {
        if (audio_page[pageIndex].readyToBeSavedToDisk) {
            if ( (audio_acqDataFileLength > MAX_AUDIO_DATA_FILE_SIZE) || (flac_encoder == 0) ) {
                audio_createNewFlacFile();
            }
            for (size_t ix = 0; ix < SAMPLES_PER_RAM_PAGE; ix++) {
                for (size_t channel = 0; channel < CHANNELS; channel++) {
                    size_t idx = (CHANNELS*BYTES_PER_SAMPLE*ix)+(BYTES_PER_SAMPLE*channel);
                    uint8_t byte1 = (uint8_t)audio_page[pageIndex].buffer[idx];
                    uint8_t byte2 = (uint8_t)audio_page[pageIndex].buffer[idx+1];
                    buff[ix * CHANNELS + channel] = (FLAC__int32)((FLAC__int16)((byte1 << 8) | byte2));
                }
            }
            FLAC__stream_encoder_process_interleaved(flac_encoder, buff, SAMPLES_PER_RAM_PAGE);
            audio_page[pageIndex].readyToBeSavedToDisk = false;
            audio_acqDataFileLength += RAM_SIZE;
        } else {
            usleep(1000);
        }
        pageIndex = !pageIndex;
    }
    FLAC__stream_encoder_finish(flac_encoder);
    FLAC__stream_encoder_delete(flac_encoder);
    flac_encoder = 0;
    g_audio_thread_writeData_is_running = 0;
    CETI_LOG("audio_thread_writeData(): Done!");
    return NULL;
}

void audio_createNewFlacFile() {
    FLAC__bool ok = true;
    FLAC__StreamEncoderInitStatus init_status;
    if (flac_encoder) {
        ok &= FLAC__stream_encoder_finish(flac_encoder);
        FLAC__stream_encoder_delete(flac_encoder);
        flac_encoder = 0;
        if (!ok) {
            CETI_LOG("audio_createNewFlacFile(): FLAC encoder failed to close for %s", audio_acqDataFileName);
        }
    }

    // filename is the time in ms at the start of audio recording
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    snprintf(audio_acqDataFileName, AUDIO_DATA_FILENAME_LEN, "/data/%lld.flac", milliseconds);
    audio_acqDataFileLength = 0;

    /* allocate the encoder */
    if ((flac_encoder = FLAC__stream_encoder_new()) == NULL) {
        CETI_LOG("audio_createNewFlacFile(): XXXX ERROR: allocating FLAC encoder");
        flac_encoder = 0;
        return;
    }

    ok &= FLAC__stream_encoder_set_channels(flac_encoder, CHANNELS);
    ok &= FLAC__stream_encoder_set_bits_per_sample(flac_encoder, BITS_PER_SAMPLE);
    ok &= FLAC__stream_encoder_set_sample_rate(flac_encoder, SAMPLE_RATE);
    ok &= FLAC__stream_encoder_set_total_samples_estimate(flac_encoder, SAMPLES_PER_RAM_PAGE);

    if (!ok) {
        CETI_LOG("audio_createNewFlacFile(): XXXX FLAC encoder failed to set parameters for %s", audio_acqDataFileName);
        flac_encoder = 0;
        return;
    }

    init_status = FLAC__stream_encoder_init_file(flac_encoder, audio_acqDataFileName, NULL, NULL);
    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        CETI_LOG("audio_createNewFlacFile(): XXXX ERROR: initializing encoder: %s", FLAC__StreamEncoderInitStatusString[init_status]);
        FLAC__stream_encoder_delete(flac_encoder);
        flac_encoder = 0;
        return;
    }
    CETI_LOG("audio_createNewFlacFile(): Saving hydrophone data to %s", audio_acqDataFileName);
}

static FILE *acqData = NULL; // file for audio recording
void* audio_thread_writeRaw(void* paramPtr) {
   // Get the thread ID, so the system monitor can check its CPU assignment.
   g_audio_thread_writeData_tid = gettid();

   // Set the thread CPU affinity.
   if(AUDIO_WRITEDATA_CPU >= 0)
   {
     pthread_t thread;
     thread = pthread_self();
     cpu_set_t cpuset;
     CPU_ZERO(&cpuset);
     CPU_SET(AUDIO_WRITEDATA_CPU, &cpuset);
     if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
       CETI_LOG("audio_thread_writeRaw(): Successfully set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
     else
       CETI_LOG("audio_thread_writeRaw(): XXX Failed to set affinity to CPU %d", AUDIO_WRITEDATA_CPU);
   }
   // Set the thread to a low priority.
   struct sched_param sp;
   memset(&sp, 0, sizeof(sp));
   sp.sched_priority = sched_get_priority_min(SCHED_RR);
   if(pthread_setschedparam(pthread_self(), SCHED_RR, &sp) == 0)
     CETI_LOG("audio_thread_writeRaw(): Successfully set priority");
   else
     CETI_LOG("audio_thread_writeRaw(): XXX Failed to set priority");

   CETI_LOG("audio_thread_writeRaw(): Starting loop to periodically write data");
   g_audio_thread_writeData_is_running = 1;
   int pageIndex = 0;
   while (!g_exit) {
       if (audio_page[pageIndex].readyToBeSavedToDisk) {
           if ( (audio_acqDataFileLength > MAX_AUDIO_DATA_FILE_SIZE) || (acqData == NULL) ) {
               audio_createNewRawFile();
           }
           fwrite(audio_page[pageIndex].buffer,1,RAM_SIZE,acqData); //
           audio_page[pageIndex].readyToBeSavedToDisk = false;
           audio_acqDataFileLength += RAM_SIZE;
           fflush(acqData);
           fsync(fileno(acqData));
       } else {
           usleep(1000);
       }
       pageIndex = !pageIndex;
   }
   g_audio_thread_writeData_is_running = 0;
   CETI_LOG("audio_thread_writeRaw(): Done!");
   return NULL;
}

void audio_createNewRawFile() {
    if (acqData != NULL) {
        fclose(acqData);
    }

    // filename is the time in ms at the start of audio recording
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    snprintf(audio_acqDataFileName, AUDIO_DATA_FILENAME_LEN, "/data/%lld.raw", milliseconds);
    acqData = fopen(audio_acqDataFileName, "wb");
    audio_acqDataFileLength = 0;
    if (!acqData) {
        CETI_LOG("Failed to open %s", audio_acqDataFileName);
        return;
    }
    CETI_LOG("audio_createNewRawFile(): Saving hydrophone data to %s", audio_acqDataFileName);
}


#endif // !ENABLE_FPGA