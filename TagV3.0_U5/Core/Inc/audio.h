#ifndef INC_AUDIO_H
#define INC_AUDIO_H
#include "stm32u5xx_hal.h"
#include "ad7768.h"
#include "config.h"

/*******************************
 * MACROS
 */
#define AUDIO_BUFFER_MAX_TIME_S 1
#define AUDIO_MAX_CHANNELS 4
#define AUDIO_MAX_BYTES_PER_CHANNEL 4
#define AUDIO_MAX_SAMPLE_RATE_Hz 192000

#define AUDIO_BUFFER_MAX_SIZE 2*(AUDIO_BUFFER_MAX_TIME_S * 192000 * AUDIO_MAX_CHANNELS * AUDIO_MAX_BYTES_PER_CHANNEL)

#ifndef AUDIO_ADC_CLK
#define AUDIO_ADC_CLK 24000000
#endif


/*******************************
 * TYPEDEFS
 */
typedef enum {
    AUDIO_BUF_STATE_EMPTY,
    AUDIO_BUF_STATE_HALF_FULL,
    AUDIO_BUF_STATE_FULL
}AudioBufferState;

typedef struct audio_manager_t {
    /* pointers to dependencies*/;
    ad7768_dev adc;
    SAI_HandleTypeDef *hsai;

    /*settings*/
    uint8_t active_channels[4];
    uint8_t keep_audio_headers;
    TagConfigAudioSampleDepth bit_depth;

    /*data_storage*/
    AudioBufferState butter_state;
    uint8_t audio_buffer[2][AUDIO_BUFFER_MAX_SIZE];
    size_t active_channel_count;
    size_t bytes_per_channel;
    uint32_t transfer_size;
}AudioManager;

/*******************************
 * FUNCTION DECLARATIONS
 */

/* initialize and configure audio manager */
HAL_StatusTypeDef audio_init(
    AudioManager *self, 
    SPI_HandleTypeDef *hspi,
    SAI_HandleTypeDef *hsai,
    TagConfig *config
);


/* set audio manager sample byte mask */
HAL_StatusTypeDef audio_set_sample_byte_mask(
    AudioManager *self, 
    uint8_t channel_enabled[4], 
    uint8_t channel_headers_enabled,
    TagConfigAudioSampleDepth channel_bit_depth
);


/* set the audio sample rate*/
HAL_StatusTypeDef audio_set_sample_rate(
    AudioManager *self, 
    TagConfigAudioSampleRate audio_rate
);




#endif 
