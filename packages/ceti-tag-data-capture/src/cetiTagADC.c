//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date          Description
//  0.0.0   10/10/21   Begin work, establish framework
//  2.1.1   06/27/22   Fix first byte bug with SPI, verify 96 KSPS setting
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagADC.c
// Description: AD7768-4 ADC setup and associated data acquisition functions
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
#define _GNU_SOURCE

#include <FLAC/stream_encoder.h>
#include <pigpio.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "cetiTagLogging.h"
#include "cetiTagWrapper.h"

// At 96 kHz sampling rate; 16-bit; 3 channels 1 minute of data is 33750 KiB
//#define MAX_AUDIO_DATA_FILE_SIZE (15 * 33750 * 1024) // 15 minute files at 96 KSPS
#define MAX_AUDIO_DATA_FILE_SIZE (5625) // 10 second files for testing
#define AUDIO_DATA_FILENAME_LEN (100)

static char acqDataFileName[AUDIO_DATA_FILENAME_LEN] = {};
static int acqDataFileLength = 0;

struct dataPage {
    char buffer[RAM_SIZE];
    int counter;
    bool readyToBeSavedToDisk;
};

static struct dataPage page[2];

static void createNewAudioDataFile(void);
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

//-----------------------------------------------------------------------------
// SPI Thread Gets Data from HW FIFO on Interrupt
//-----------------------------------------------------------------------------
static char first_byte=1;  //first byte in stream must be discarded

void *audioSpiThread(void *paramPtr) {
    int pageIndex = 0;
    init_pages();
    int spi_fd = spiOpen(SPI_CE, SPI_CLK_RATE, 1);

    if (spi_fd < 0) {
        CETI_LOG("pigpio SPI initialisation failed");
        return NULL;
    }

    /* Set GPIO modes */
    gpioSetMode(AUDIO_DATA_AVAILABLE, PI_INPUT);

    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_max(SCHED_RR);
    sched_setscheduler(0, SCHED_RR, &sp);
    mlockall(MCL_CURRENT | MCL_FUTURE);

    volatile int status;
    volatile int prev_status;

    prev_status = gpioRead(AUDIO_DATA_AVAILABLE);
    status = prev_status;

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
                        page[pageIndex].buffer +
                            (page[pageIndex].counter * SPI_BLOCK_SIZE),
                        SPI_BLOCK_SIZE);

                // When NUM_SPI_BLOCKS are in the ram buffer, set flag that
                // triggers a transfer from RAM to mass storage, then flip the
                // data page and continue get samples from SPI.
                if (page[pageIndex].counter == NUM_SPI_BLOCKS - 1) {
                    page[pageIndex].readyToBeSavedToDisk = true;
                    page[pageIndex].counter = 0; // reset for next chunk
                    pageIndex = !pageIndex;
                } else {
                    page[pageIndex].counter++;
                }
                status = gpioRead(AUDIO_DATA_AVAILABLE);
            }
        } else {
            usleep(1000);
        }
    }
    spiClose(spi_fd);
    return NULL;
}

//-----------------------------------------------------------------------------
// Write Data Thread moves the RAM buffer to mass storage
//-----------------------------------------------------------------------------

#define SAMPLE_RATE (96000)
#define CHANNELS (3)
#define BITS_PER_SAMPLE (16)
#define BYTES_PER_SAMPLE  (BITS_PER_SAMPLE/8)
#define MAX_SAMPLES_PER_FILE (MAX_AUDIO_DATA_FILE_SIZE / CHANNELS / BYTES_PER_SAMPLE)
#define SAMPLES_PER_RAM_PAGE (RAM_SIZE / CHANNELS / BYTES_PER_SAMPLE)

static FLAC__StreamEncoder *flac_encoder = 0;

void * audioWriteDataThread( void * paramPtr ) {
    FLAC__bool ok = true;
    FLAC__int32 buff[CHANNELS] = {0};
    int pageIndex = 0;
    while (!g_exit) {
        if (page[pageIndex].readyToBeSavedToDisk) {
            if ( (acqDataFileLength > MAX_AUDIO_DATA_FILE_SIZE) || (flac_encoder == 0) ) {
                createNewAudioDataFile();
            }
            // TODO: verify endianness is correct here
            // TODO: if you call the encoder function with more than just one set of samples, it will be more efficient
            //       but rpi0 does not have enough RAM to allocate the full buffer to fit all samples receieved from SPI
            for (size_t ix = 0; ix < SAMPLES_PER_RAM_PAGE; ix++) {
                for (size_t channel = 0; channel < CHANNELS; channel++) {
                    buff[channel] = 0;
                    buff[channel] = (FLAC__int32)(FLAC__int16)(page[pageIndex].buffer[ix*BYTES_PER_SAMPLE+1] << 8) | (FLAC__int16)(page[pageIndex].buffer[ix*BYTES_PER_SAMPLE]);
                }
                FLAC__stream_encoder_process_interleaved(flac_encoder, buff, 1);
            }
            page[pageIndex].readyToBeSavedToDisk = false;
            acqDataFileLength += RAM_SIZE;
        } else {
            usleep(1000);
        }
        pageIndex = !pageIndex;
    }
    ok &= FLAC__stream_encoder_finish(flac_encoder);
    FLAC__stream_encoder_delete(flac_encoder);
    flac_encoder = 0;
    return NULL;
}


static void createNewAudioDataFile() {
    FLAC__bool ok = true;
    FLAC__StreamEncoderInitStatus init_status;
    if (flac_encoder) {
        ok &= FLAC__stream_encoder_finish(flac_encoder);
        FLAC__stream_encoder_delete(flac_encoder);
        flac_encoder = 0;
        if (!ok) {
            CETI_LOG("FLAC encoder failed to close for %s", acqDataFileName);
        }
    }

    // filename is the time in ms at the start of audio recording
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    snprintf(acqDataFileName, AUDIO_DATA_FILENAME_LEN, "../data/%lld.flac", milliseconds);

    /* allocate the encoder */
    if ((flac_encoder = FLAC__stream_encoder_new()) == NULL) {
        CETI_LOG("ERROR: allocating FLAC encoder");
        flac_encoder = 0;
        return;
    }

    ok &= FLAC__stream_encoder_set_verify(flac_encoder, true);
    ok &= FLAC__stream_encoder_set_compression_level(flac_encoder, 5);
    ok &= FLAC__stream_encoder_set_channels(flac_encoder, CHANNELS);
    ok &= FLAC__stream_encoder_set_bits_per_sample(flac_encoder, BITS_PER_SAMPLE);
    ok &= FLAC__stream_encoder_set_sample_rate(flac_encoder, SAMPLE_RATE);
    ok &= FLAC__stream_encoder_set_total_samples_estimate(flac_encoder, MAX_SAMPLES_PER_FILE);

    if (!ok) {
        CETI_LOG("FLAC encoder failed to set parameters for %s", acqDataFileName);
        flac_encoder = 0;
        return;
    }

    init_status = FLAC__stream_encoder_init_file(flac_encoder, acqDataFileName, NULL, NULL);
    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        CETI_LOG("ERROR: initializing encoder: %s", FLAC__StreamEncoderInitStatusString[init_status]);
        FLAC__stream_encoder_delete(flac_encoder);
        flac_encoder = 0;
        return;
    }
    CETI_LOG("createNewAudioDataFile(): Saving hydrophone data to %s", acqDataFileName);
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
            CETI_LOG("Failed to set ADC sampling to 96 kHz - Reg 0x01 reads back 0x%02X%02X should be 0x0008 ", cam_response[4],cam_response[5]);
            return -1; //ADC failed to configure as expected
        }
    }
    
    CETI_LOG("Successfully set ADC sampling to 96 kHz - Reg 0x01 reads back 0x%02X%02X should be 0x0008 ", cam_response[4],cam_response[5]);
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
    createNewAudioDataFile();
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

    cam(3, 0, 0, 0, 0, cam_response); // flushes the FIFO
    // formatRaw();                   // makes formatted output file to plot
    // formatRawNoHeader3ch16bit();   // reduced data size
    return 0;
}

