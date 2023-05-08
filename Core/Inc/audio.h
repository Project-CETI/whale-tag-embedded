#ifndef INC_AUDIO_H
#define INC_AUDIO_H
#include "stm32u5xx_hal.h"
#include "ad7768.h"
#include "config.h"

#define AUDIO_BUFFER_MAX_CHANNELS 4
#define AUDIO_BUFFER_MAX_BYTES_PER_CHANNEL 4

#ifndef AUDIO_ADC_CLK
#define AUDIO_ADC_CLK 24000000
#endif

typedef enum{
    AUDIO_DEPTH_16_BIT,
    AUDIO_DEPTH_24_BIT,
}AudioDepth;

typedef struct audio_manager_t {
    /* pointers to dependencies*/;
    ad7768_dev adc;
    SAI_HandleTypeDef *hsai;

    /*settings*/
    uint8_t active_channels[4];
    uint8_t keep_audio_headers;
    AudioDepth bit_depth;
    size_t sample_size;
    size_t active_channel_count;
}AudioManager;

/*******************************
 * FUNCTION DECLARATIONS
 */

/* initialize and configure audio manager */
HAL_StatusTypeDef audio_init(
    AudioManager *audio_mngr, 
    SPI_HandleTypeDef *hspi,
    SAI_HandleTypeDef *hsai,
    TagConfig *config
);


#endif 