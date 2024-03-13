//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef AUDIO_H
#define AUDIO_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE // change how sched.h will be included

#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../utils/logging.h"

#include <FLAC/stream_encoder.h>
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <sched.h>   // to set process priority
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

//-----------------------------------------------------------------------------
// Definitions/Configurations
//-----------------------------------------------------------------------------

// NUM_SPI_BLOCKS is the number of blocks acquired before writing out to
// mass storage. A setting of 2100 will give buffer about 30 seconds at a time if
// sample rate is 96 kHz. Example sizing of buffer (DesignMaps.xlsx has a
// calcuator for this):
//
//  - SPI block HWM * 32 bytes = 8192 bytes (25% of the hardware FIFO in this example)
//  - Sampling rate 96000 Hz
//  - 6 bytes per sample set (3 channels, 16 bits per channel)
//  - 1 SPI Block is then 8192/6 = 1365.333 sample sets or about 14 ms worth of data in
//  - 30 seconds is 30/.014  = about 2142 SPI blocks
//
// N.B. Make NUM_SPI_BLOCKS an integer multiple of 3 for alignment
// reasons

// SPI Block Size
#define HWM (256)                 // High Water Mark from Verilog code - these are 32 byte chunks (2 sample sets)
#define SPI_BLOCK_SIZE (HWM * 32) // make SPI block size <= HWM * 32 otherwise may underflow
#define SPI_BLOCK_SIZE_SAMPLES (SPI_BLOCK_SIZE / (CHANNELS * MIN_BYTES_PER_SAMPLE))

//#define NUM_SPI_BLOCKS (2100*10)                 // 5 minute buffer
#define NUM_SPI_BLOCKS (2100 * 2)                  // 1 minute buffer
#define RAM_SIZE (NUM_SPI_BLOCKS * SPI_BLOCK_SIZE) // bytes

#define CHANNELS (3)
#define MIN_BITS_PER_SAMPLE (16)
#define MIN_BYTES_PER_SAMPLE (MIN_BITS_PER_SAMPLE / 8) 
// At 96 kHz sampling rate; 16-bit; 3 channels 1 minute of data is 33750 KiB
#define MAX_AUDIO_DATA_FILE_SIZE ((5 - 1) * 33750 * 1024) // Yields approx 5 minute files at 96 KSPS
#define MAX_SAMPLES_PER_FILE (MAX_AUDIO_DATA_FILE_SIZE / CHANNELS / MIN_BYTES_PER_SAMPLE)
#define MAX_SAMPLES_PER_RAM_PAGE (RAM_SIZE / CHANNELS / 2)
#define AUDIO_DATA_FILENAME_LEN (100)

// Data Acq SPI Settings and Audio Data Buffering
#define SPI_CE (0)
#define SPI_CLK_RATE (15000000) // Hz

// Overflow indicator.
#define AUDIO_OVERFLOW_GPIO 12 // -1 to not use the indicator

//-----------------------------------------------------------------------------
// Dacq Flow Control Handshaking Interrupt
// The flow control signal is GPIO 22, (WiringPi 3). The FPGA sets this when
// FIFO is at HWM and clears it at LWM as viewed from the write count port. The
// FIFO margins may need to be tuned depending on latencies and how this program
// is constructed. The margins are set in the FPGA Verilog code, not here.

#define AUDIO_DATA_AVAILABLE (22)

// value assigned to kHz value for easy printing, but enum limit number of options

typedef enum audio_sample_rate_e{
    AUDIO_SAMPLE_RATE_DEFAULT,
    AUDIO_SAMPLE_RATE_48KHZ = 48,
    AUDIO_SAMPLE_RATE_96KHZ = 96,
    AUDIO_SAMPLE_RATE_192KHZ = 192,
} AudioSampleRate;

typedef enum audio_bit_depth_e {
    AUDIO_BIT_DEPTH_16,
    AUDIO_BIT_DEPTH_24,
} AudioBitDepth;

typedef enum audio_filter_type_e {
    AUDIO_FILTER_WIDEBAND = 0,
    AUDIO_FILTER_SINC5 = 1,
} AudioFilterType;

typedef struct audio_config_t{
    AudioFilterType filter_type;
    AudioSampleRate sample_rate;
    AudioBitDepth bit_depth;
} AudioConfig;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
// Device Driver Methods
int audio_setup(AudioConfig *config);
int audio_set_sample_rate(AudioSampleRate sample_rate);
void init_audio_buffers();
int setup_audio_default(void);
int reset_audio_fifo(void);
int start_audio_acq(void);
int stop_audio_acq(void);
// Thread Methods
int audio_thread_init(AudioConfig *config);
void createNewAudioDataFile(void);
void formatRaw(void);
void formatRawNoHeader3ch16bit(void);
void audio_createNewFlacFile();
void audio_createNewRawFile();
void *audio_thread_spi(void *paramPtr);
void *audio_thread_writeFlac(void *paramPtr);
void *audio_thread_writeRaw(void *paramPtr);
void audio_check_for_overflow(int location_index);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_audio_thread_spi_is_running;
extern int g_audio_thread_writeData_is_running;
extern int g_audio_overflow_detected;
extern int g_audio_force_overflow;

#endif // AUDIO_H
