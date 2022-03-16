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
#include <pigpio.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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
static int fileNum = 0;
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
  fileNum = 0;
  strcpy(acqDataFileName, ACQ_FILE);
  acqData = fopen(
      acqDataFileName,
      "wb");  // opens the data file for writing (the writes happen in ISR)
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
    if (page[pageIndex].readyToBeSavedToDisk) {
      printf("Writing %d SPI_BLOCKS from ram buff to SD Card  \n",
             NUM_SPI_BLOCKS);
      fwrite(page[pageIndex].buffer, 1, RAM_SIZE, acqData);  //
      page[pageIndex].readyToBeSavedToDisk = false;
      acqDataFileLength += RAM_SIZE;
      if (acqDataFileLength > MAX_DATA_FILE_SIZE) createNewDataFile();
    } else {
      usleep(1000);
    }
    pageIndex = !pageIndex;
  }
  return NULL;
}

//-----------------------------------------------------------------------------
static void createNewDataFile() {
  char newFile[50];
  static int fileNum = 0;
  fclose(acqData);
  ++fileNum;
  snprintf(newFile, (DATA_FILENAME_LEN + 8), "%s.%u", acqDataFileName, fileNum);
  rename(acqDataFileName, newFile);
  acqDataFileLength = 0;  // reset
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

  int i = 0;
  char sample_buffer[16];
  char temp[1];

  rawData = fopen(ACQ_FILE, "rb");  // input file to parse and format
  outData = fopen("../data/test_acq_out.dat", "w");  // output file

  fread(temp, 1, 1, rawData);  // drop the first byte returned on SPI, not valid

  while (i < NUM_SMPL_SETS) {
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

    i++;
  }

  fclose(rawData);

  outData = fopen("../data/test_acq_out.dat", "w");  // output file

  for (i = 0; i < NUM_SMPL_SETS; i++) {
    fprintf(outData, "%d %d %d %d %02X %02X %02X %02X\n", chan_0[i], chan_1[i],
            chan_2[i], chan_3[i], chan_0_header[i], chan_1_header[i],
            chan_2_header[i], chan_3_header[i]);
  }

  fclose(outData);
}

//---------------------------------------------------------------------------------------
void formatRawNoHeader3ch16bit(void) {
#define NUM_SMPL_SETS 1024

  signed int chan_0[NUM_SMPL_SETS];  // each sample is 16-bit
  signed int chan_1[NUM_SMPL_SETS];
  signed int chan_2[NUM_SMPL_SETS];
  // signed int chan_3[NUM_SMPL_SETS];

  FILE *rawData = NULL;
  FILE *outData = NULL;

  int i = 0;

  // char * pTemp;
  char sample_buffer[6];  // No header, and 3 ch 16 bit so 6 bytes for each
                          // sample set
  char temp[1];

  rawData = fopen(ACQ_FILE, "rb");  // input file to parse and format
  outData = fopen("../data/test_acq_out.dat", "w");  // output file

  fread(temp, 1, 1, rawData);  // drop the first byte returned on SPI, not valid

  while (i < NUM_SMPL_SETS) {
    fread(sample_buffer, 1, 6, rawData);

    // The samples
    chan_0[i] = ((sample_buffer[0] << 24) + (sample_buffer[1] << 16)) / 256;

    chan_1[i] = ((sample_buffer[2] << 24) + (sample_buffer[3] << 16)) / 256;

    chan_2[i] = ((sample_buffer[4] << 24) + (sample_buffer[5] << 16)) / 256;

    // Just three channels while we improve OS latencies

    /*	   	chan_3[i] = ((sample_buffer[9]  << 24 ) +
                                             (sample_buffer[10]	 << 16 ) +
                                             (sample_buffer[11]  <<  8 ))/256;
     */

    i++;
  }

  fclose(rawData);

  outData = fopen("../data/test_acq_out.dat", "w");  // output file

  for (i = 0; i < NUM_SMPL_SETS; i++) {
    fprintf(outData, "%d %d %d\n", chan_0[i], chan_1[i], chan_2[i]);
  }

  fclose(outData);
}