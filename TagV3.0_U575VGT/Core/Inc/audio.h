#ifndef __AUDIO_INC_H__
#define __AUDIO_INC_H__

#include <stdint.h>

#include "main.h"
#include "ad7768.h"
#include "config.h"
#include "app_filex.h"

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
    uint8_t audio_buffer[512*2];


    /*FS/SD Card writing variables*/
    FX_FILE *file;
} AudioManager;

/* audio_setup
//attach adc, filex
// audio_configure()
//setup file_x
*/

/* Desc: set the audio sample rate */
HAL_StatusTypeDef audio_set_sample_rate(AudioManager *self, TagConfigAudioSampleRate audio_rate);

HAL_StatusTypeDef audio_configure(AudioManager *self, TagConfig *config);

/* audio_configure()
//setup adc
//apply config settings to adc and sai
*/

HAL_StatusTypeDef audio_record(AudioManager *self);
// start audio circular buffer
// enable RAM->SRAM_ext DMA
// enable SRAM_ext->SD Card

/* audio_halt()
// stop circular buffer
// write SRAM_ext to SD_Card
*/

/* audio_unit_test()
*/

HAL_StatusTypeDef audio_tick(AudioManager *self);

#endif /*__AUDIO_INC_H__*/