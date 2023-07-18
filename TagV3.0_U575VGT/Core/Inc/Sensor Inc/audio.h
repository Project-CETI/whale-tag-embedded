#ifndef __AUDIO_INC_H__
#define __AUDIO_INC_H__

#include <stdint.h>

#include "main.h"
#include "ad7768.h"
#include "config.h"
#include "app_filex.h"

#define AUDIO_CIRCULAR_BUFFER_SIZE_MAX (UINT16_MAX/2)
#define AUDIO_CIRCULAR_BUFFER_SIZE  (32256)

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
    uint8_t audio_buffer[2][AUDIO_CIRCULAR_BUFFER_SIZE];

    uint8_t temp_buffer[20][AUDIO_CIRCULAR_BUFFER_SIZE];

    uint8_t temp_counter;
    /*FS/SD Card writing variables*/
    FX_FILE *file;
} AudioManager;

/* audio_setup
//attach adc, filex
// audio_configure()
//setup file_x
*/
HAL_StatusTypeDef audio_init(AudioManager *self, ad7768_dev *adc, SAI_HandleTypeDef *hsai, TagConfig *config, FX_FILE *file);

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


void audio_thread_entry(ULONG thread_input);
/* audio_halt()
// stop circular buffer
// write SRAM_ext to SD_Card
*/

/* audio_unit_test()
*/

HAL_StatusTypeDef audio_tick(AudioManager *self);

#endif /*__AUDIO_INC_H__*/
