//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    Description
//  0.00    10/10/21   Begin work, establish framework
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagADC.c
// Description: Data Acquisition
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Setting up Sampling Rates
// SYSCLK is assumed to be 98.304 MHz to realize these exact rates
// MCLK is SYSCLK/4
// Output data rate is MCLK / ( DEC_RATE * MCLK_DIV)
// DCLK must be set fast enough to keep up with ADC output
// Use opcode 1 to write registers 1,4 and 7 in the ADC to adjust rates
// See ADC data sheet and sampling.xlsx in the project file for more details on
// registers settings
//
//-----------------------------------------------------------------------------
#define _GNU_SOURCE

#include <pigpio.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "cetiTagLogging.h"
#include "cetiTagWrapper.h"

#define DATA_FILENAME_LEN (100)

// At 96 kHz sampling rate; 16-bit; 3 channels 1 minute of data is 33750 KiB
#define MAX_DATA_FILE_SIZE (15 * 33750 * 1024)  // 15 minute files at 96

struct dataPage {
  char buffer[RAM_SIZE];
  int counter;
  bool readyToBeSavedToDisk;
};

static struct dataPage page[2];

static FILE *acqData = NULL;  // data file for audio data
static char acqDataFileName[DATA_FILENAME_LEN];
static int acqDataFileLength;

static void createNewDataFile(void);
void formatRaw(void);
void formatRawNoHeader3ch16bit(void);

//-----------------------------------------------------------------------------
//  Acquisition Hardware Setup and Control Utility Functions
//-----------------------------------------------------------------------------

void init_pages() {
  page[0].counter = 0;
  page[0].readyToBeSavedToDisk = false;
  page[1].counter = 0;
  page[1].readyToBeSavedToDisk = false;
}

int setup_default(void) {
  char cam_response[8];
  cam(1, 0x01, 0x0D, 0, 0, cam_response);  // DEC_RATE = 1024
  cam(1, 0x04, 0x00, 0, 0, cam_response);  // POWER_MODE = LOW; MCLK_DIV = 32
  cam(1, 0x07, 0x00, 0, 0, cam_response);  // DCLK_DIV = 8
  cam(2, 0, 0, 0, 0, cam_response);        // Apply the settings
  return 0;
}

int setup_48kHz(void) {
  char cam_response[8];
  cam(1, 0x01, 0x09, 0, 0, cam_response);  // DEC_RATE = 64
  cam(1, 0x04, 0x22, 0, 0, cam_response);  // POWER_MODE = MID; MCLK_DIV = 8
  cam(1, 0x07, 0x00, 0, 0, cam_response);  // DCLK_DIV = 8
  cam(2, 0, 0, 0, 0, cam_response);        // Apply the settings
  return 0;
}

int setup_96kHz(void) {
  char cam_response[8];
  cam(1, 0x01, 0x08, 0, 0, cam_response);  // DEC_RATE = 32
  cam(1, 0x04, 0x22, 0, 0, cam_response);  // POWER_MODE = MID; MCLK_DIV = 8
  cam(1, 0x07, 0x01, 0, 0, cam_response);  // DCLK_DIV = 4
  cam(2, 0, 0, 0, 0, cam_response);        // Apply the settings
  return 0;
}

int setup_192kHz(void) {
  char cam_response[8];
  cam(1, 0x01, 0x08, 0, 0, cam_response);  // DEC_RATE = 32
  cam(1, 0x04, 0x33, 0, 0, cam_response);  // POWER_MODE = FAST; MCLK_DIV = 4
  cam(1, 0x07, 0x01, 0, 0, cam_response);  // DCLK_DIV = 4
  cam(2, 0, 0, 0, 0, cam_response);        // Apply the settings
  return 0;
}

int setup_sim_rate(char rate) {
  char cam_response[8];
  cam(6, 0x00, rate, 0, 0, cam_response);  // arg 0 is period in microseconds
  printf("setup_sim_rate(): arg0 rate is %d\n", rate);
  return 0;
}

int reset_fifo(void) {
  char cam_response[8];
  cam(5, 0x01, 0x08, 0, 0, cam_response);  // Stop any incoming data
  cam(3, 0x04, 0x33, 0, 0, cam_response);  // Reset the FIFO
  return 0;
}

//-----------------------------------------------------------------------------
//  Acquisition Start and Stop Controls (TODO add file open/close checking)
//-----------------------------------------------------------------------------

int start_acq(void) {
  char cam_response[8];
  cam(5, 0, 0, 0, 0, cam_response);  // stops the input stream
  cam(3, 0, 0, 0, 0, cam_response);  // flushes the FIFO
  init_pages();
  createNewDataFile();
  cam(4, 0, 0, 0, 0, cam_response);  // starts the stream
  return 0;
}

int stop_acq(void) {
  char cam_response[8];
  cam(5, 0, 0, 0, 0, cam_response);  // stops the input stream
  fclose(acqData);                   // close the data file
  cam(3, 0, 0, 0, 0, cam_response);  // flushes the FIFO
  // formatRaw();                   // makes formatted output file to plot
  // formatRawNoHeader3ch16bit();   // reduced data size
  return 0;
}

#define TEST_POINT (17)
#define DATA_AVAIL (22)

//-----------------------------------------------------------------------------
// SPI Thread Gets Data from HW FIFO on Interrupt
//-----------------------------------------------------------------------------
void *spiThread(void *paramPtr) {
  int pageIndex = 0;
  init_pages();
  int spi_fd = spiOpen(SPI_CE, SPI_CLK_RATE, 1);

  if (spi_fd < 0) {
    fprintf(stderr, "pigpio SPI initialisation failed\n");
    return NULL;
  }

  /* Set GPIO modes */
  gpioSetMode(TEST_POINT, PI_OUTPUT);
  gpioSetMode(DATA_AVAIL, PI_INPUT);

  gpioWrite(TEST_POINT, 0);

  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_max(SCHED_RR);
  sched_setscheduler(0, SCHED_RR, &sp);
  mlockall(MCL_CURRENT | MCL_FUTURE);

  volatile int status;
  volatile int prev_status;

  prev_status = gpioRead(DATA_AVAIL);
  status = prev_status;

  while (1) {
    status = gpioRead(DATA_AVAIL);

    if (status != prev_status) {
      prev_status = status;
      while (status) {
        gpioWrite(TEST_POINT, 1);
        gpioWrite(TEST_POINT, 0);

        // SPI block has become available, read it from the HW FIFO via SPI
        spiRead(
            spi_fd,
            page[pageIndex].buffer + (page[pageIndex].counter * SPI_BLOCK_SIZE),
            SPI_BLOCK_SIZE);

        // When NUM_SPI_BLOCKS are in the ram buffer, set flag that triggers
        // a transfer from RAM to mass storage, then flip the data page
        // and continue get samples from SPI.
        if (page[pageIndex].counter == NUM_SPI_BLOCKS - 1) {
          page[pageIndex].readyToBeSavedToDisk = true;
          page[pageIndex].counter = 0;  // reset for next chunk
          pageIndex = !pageIndex;
        } else {
          page[pageIndex].counter++;
        }
        status = gpioRead(DATA_AVAIL);
      }
    } else {
      usleep(1000);
    }
  }
  spiClose(spi_fd);
  return NULL;
}

//-----------------------------------------------------------------------------
// Write Data Thread moves the RAM Buffer to mass storage
//-----------------------------------------------------------------------------
void *writeDataThread(void *paramPtr) {
  int pageIndex = 0;
  while (1) {
    if (!page[pageIndex].readyToBeSavedToDisk) {
      usleep(1000);
    }
    if (acqDataFileLength > MAX_DATA_FILE_SIZE) {
      createNewDataFile();
      acqDataFileLength = 0;
    }

    printf("Writing %d SPI_BLOCKS from ram buff to SD Card  \n",
           NUM_SPI_BLOCKS);
    page[pageIndex].readyToBeSavedToDisk = false;
    acqDataFileLength += RAM_SIZE;
    fwrite(page[pageIndex].buffer, 1, RAM_SIZE, acqData);
    fflush(acqData);
    fsync(fileno(acqData));

    pageIndex = !pageIndex;
  }
  return NULL;
}

void *flacCompressThread(void *paramPtr) {

  // Set the priority to the lowest possible for the compression thread
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_max(SCHED_IDLE);
  sched_setscheduler(0, SCHED_IDLE, &sp);

  int filename_len = DATA_FILENAME_LEN+strlen(".flac");
  char flac_filename[filename_len];
  memset(flac_filename, 0, filename_len);
  snprintf(flac_filename, filename_len, "%s.flac", acqDataFileName);

  int cmd_len = 3 * filename_len;
  char cmd[cmd_len];
  memset(cmd, 0, cmd_len);
  snprintf(cmd, cmd_len, "flac --channels=3 --bps=16 --sample-rate=96000 --sign=signed --endian=little --force-raw-format %s --force %s", acqDataFileName, flac_filename);

  int retCode = system(cmd);
  if (!retCode) {
    CETI_LOG("Failed to flac compress %s", acqDataFileName);
    remove(acqDataFileName);
  }
  return NULL;
}

static void flacCompress() {
  pthread_t thread_id = 0;
  if (!pthread_create(&thread_id, NULL, &flacCompressThread, NULL)) {
    pthread_detach(thread_id);
  }
}

static void createNewDataFile() {
  if (acqData!=NULL) {
    fclose(acqData);
    flacCompress();
  }

  // filename is the time in ms at the start of audio recording
  struct timeval te;
  gettimeofday(&te, NULL);
  long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
  snprintf(acqDataFileName, DATA_FILENAME_LEN, "../data/%lld", milliseconds);
  acqData = fopen(acqDataFileName, "wb");
}

//-----------------------------------------------------------------------------
// Test/debug utility - Format acquired raw data file for export - e.g. to plot
// the data in Excel.
//-----------------------------------------------------------------------------
#define NUM_SMPL_SETS 1024  // adjust size as needed

void formatRaw(void) {
  signed int chan_0[NUM_SMPL_SETS];  // raw samples are 24 bits 2s complement
                                     // and have 8-bit header
  signed int chan_1[NUM_SMPL_SETS];
  signed int chan_2[NUM_SMPL_SETS];
  signed int chan_3[NUM_SMPL_SETS];

  char chan_0_header[NUM_SMPL_SETS];
  char chan_1_header[NUM_SMPL_SETS];
  char chan_2_header[NUM_SMPL_SETS];
  char chan_3_header[NUM_SMPL_SETS];

  FILE *rawData = NULL;
  FILE *outData = NULL;

  char sample_buffer[16];

  rawData = fopen("../data/test_acq_raw.dat", "rb");  // input file to parse and format
  outData = fopen("../data/test_acq_out.dat", "w");  // output file

  char temp;
  fread(&temp, 1, 1, rawData);  // drop the first byte returned on SPI, not valid

  for (int i = 0; i < NUM_SMPL_SETS; i++) {
    fread(sample_buffer, 1, 16, rawData);

    // The headers
    chan_0_header[i] = sample_buffer[0];
    chan_1_header[i] = sample_buffer[4];
    chan_2_header[i] = sample_buffer[8];
    chan_3_header[i] = sample_buffer[12];

    // The samples
    chan_0[i] = ((sample_buffer[1] << 24) + (sample_buffer[2] << 16) +
                 (sample_buffer[3] << 8)) /
                256;

    chan_1[i] = ((sample_buffer[5] << 24) + (sample_buffer[6] << 16) +
                 (sample_buffer[7] << 8)) /
                256;

    chan_2[i] = ((sample_buffer[9] << 24) + (sample_buffer[10] << 16) +
                 (sample_buffer[11] << 8)) /
                256;

    chan_3[i] = ((sample_buffer[13] << 24) + (sample_buffer[14] << 16) +
                 (sample_buffer[15] << 8)) /
                256;

    fprintf(outData, "%d %d %d %d %02X %02X %02X %02X\n", chan_0[i], chan_1[i],
            chan_2[i], chan_3[i], chan_0_header[i], chan_1_header[i],
            chan_2_header[i], chan_3_header[i]);
  }

  fclose(rawData);
  fclose(outData);
}

//---------------------------------------------------------------------------------------
void formatRawNoHeader3ch16bit(void) {
#define NUM_SMPL_SETS 1024

  signed int chan_0[NUM_SMPL_SETS];  // each sample is 16-bit
  signed int chan_1[NUM_SMPL_SETS];
  signed int chan_2[NUM_SMPL_SETS];

  FILE *rawData = NULL;
  FILE *outData = NULL;

  char sample_buffer[6];  // No header, and 3 ch 16 bit so 6 bytes for each
                          // sample set

  rawData = fopen("../data/test_acq_raw.dat", "rb");  // input file to parse and format
  outData = fopen("../data/test_acq_out.dat", "w");  // output file

  char temp;
  fread(&temp, 1, 1, rawData);  // drop the first byte returned on SPI, not valid

  for (int i = 0; i < NUM_SMPL_SETS; i++) {
    fread(sample_buffer, 1, 6, rawData);
    chan_0[i] = ((sample_buffer[0] << 24) + (sample_buffer[1] << 16)) / 256;
    chan_1[i] = ((sample_buffer[2] << 24) + (sample_buffer[3] << 16)) / 256;
    chan_2[i] = ((sample_buffer[4] << 24) + (sample_buffer[5] << 16)) / 256;
    fprintf(outData, "%d %d %d\n", chan_0[i], chan_1[i], chan_2[i]);
  }

  fclose(rawData);
  fclose(outData);
}