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

#include "../launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update
#include "../cetiTag.h"

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
#define AUDIO_DATA_FILENAME_LEN (100)

// Data Acq SPI Settings and Audio Data Buffering
#define SPI_CE (0)
#define SPI_CLK_RATE (15000000) // Hz

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
    AUDIO_BIT_DEPTH_16 = 16,
    AUDIO_BIT_DEPTH_24 = 24,
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
int audio_set_bit_depth(AudioBitDepth bit_depth);
int audio_set_filter_type(AudioFilterType filter_type);
int audio_set_sample_rate(AudioSampleRate sample_rate);
void init_audio_buffers();
int setup_audio_default(void);
int reset_audio_fifo(void);
int start_audio_acq(void);
int stop_audio_acq(void);
// Thread Methods
int audio_thread_init(void);
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
