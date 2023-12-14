#ifndef __AUDIO_INC_H__
#define __AUDIO_INC_H__

/*
 * author: Kaveet
 *
 * This file is responsible for getting the Audio data from the ADC (ad7768) and writing to the SD card
 *
 * The audio data comes in through the SAI (Serial-Audio Interface) through DMA, and then is written to a temporary buffer. Once the temporary buffer fills, we write to the SD card
 *
 * Due to limitations by STM32, the DMA buffer is not large enough (They only allow 16-bit length values). This is why we introduced the larger temporary buffer, which is about 10x the size of the DMA buffer.
 *
 * SD card writes can take some time to setup, so limiting the number of writes (and increasing the quantity of each write) can help reduce the time servicing the audio task.
 *
 * For more of a breakdown, see hand-over documents.
 * ADC Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ad7768-7768-4.pdf
 */
#include <stdint.h>
#include "Lib Inc/timing.h"
#include "main.h"
#include "ad7768.h"
#include "config.h"
#include "app_filex.h"

//Buffer constants
#define AUDIO_CIRCULAR_BUFFER_SIZE_MAX	(UINT16_MAX/2)
#define AUDIO_CIRCULAR_BUFFER_SIZE 		32256

//Audio flags
#define AUDIO_BUFFER_HALF_FULL_FLAG		0x1
#define AUDIO_BUFFER_FULL_FLAG			0x2
#define AUDIO_STOP_THREAD_FLAG			0x4
#define AUDIO_UNIT_TEST_FLAG			0x8
#define AUDIO_UNIT_TEST_DONE_FLAG		0x10
#define AUDIO_READ_FLAG					0x20
#define AUDIO_CMD_FLAG					0x40

//Audio commands
#define AUDIO_GET_SAMPLES_CMD			0x1
#define AUDIO_NUM_SAMPLES				100

//DMA block length
#define DMA_BUF_BLOCK_LENGTH			2 //DO NOT CHANGE

//Number of blocks the temp buffer should use. This MUST be an even number.
#define TEMP_BUF_BLOCK_LENGTH			18
#define TEMP_BUF_HALF_BLOCK_LENGTH		((TEMP_BUF_BLOCK_LENGTH) / 2)

//Time interval to create new logging file in minutes
#define RTC_AUDIO_REFRESH_MINS			5
#define RTC_AUDIO_REFRESH_TOL			1

//String indices for file name
#define AUDIO_FILENAME_HOURS_INDEX		7
#define AUDIO_FILENAME_MINS_INDEX		9

typedef enum {
    AUDIO_BUF_STATE_EMPTY,
    AUDIO_BUF_STATE_HALF_FULL,
    AUDIO_BUF_STATE_FULL,
} AudioBufferState;

typedef struct audio_manager_s {
    /*Analog to Digital Converter*/
    ad7768_dev *adc;
    SAI_HandleTypeDef *sai;

    /*Memory DMA Transfer Variables*/
    AudioBufferState buffer_state;
    size_t sample_size;
    size_t channel_count;

    //DMA buffer
    uint8_t audio_buffer[DMA_BUF_BLOCK_LENGTH][AUDIO_CIRCULAR_BUFFER_SIZE];

    //Temp buffer & its index tracker
    uint8_t temp_buffer[TEMP_BUF_BLOCK_LENGTH][AUDIO_CIRCULAR_BUFFER_SIZE];
    uint8_t temp_counter;

    //Flag to show SD card has been written to
    bool sd_write_complete;

    /*FS/SD Card writing variables*/
    FX_FILE *file;

} AudioManager;

HAL_StatusTypeDef audio_init(AudioManager *self, ad7768_dev *adc, SAI_HandleTypeDef *hsai, TagConfig *config, FX_FILE *file);
HAL_StatusTypeDef audio_set_sample_rate(AudioManager *self, TagConfigAudioSampleRate audio_rate);
HAL_StatusTypeDef audio_configure(AudioManager *self, TagConfig *config);
HAL_StatusTypeDef audio_record(AudioManager *self);
HAL_StatusTypeDef audio_tick(AudioManager *self);

//Main audio thread to run on RTOS
void audio_thread_entry(ULONG thread_input);

#endif /*__AUDIO_INC_H__*/
