
#include "audio.h"
#include "util.h"

/**********
 * MACROS *
 **********/

/********************
 * PRIVATE TYPEDEFS *
 ********************/

/*********************
 * PRIVATE VARIABLES *
 *********************/

/********************************
 * PRIVATE FUNCTION DEFINITIONS *
 ********************************/

/*******************************
 * PUBLIC FUNCTION DEFINITIONS *
 *******************************/
/* 
 * Desc: initialize and configure audio manager
 */
HAL_StatusTypeDef audio_init(
    AudioManager *self, 
    SPI_HandleTypeDef *hspi,
    SAI_HandleTypeDef *hsai,
    TagConfig *config
){
    if( (self == NULL) 
        || (hspi == NULL)
        || (hsai == NULL)
        || (config == NULL)
    ){ 
        return HAL_ERROR;
    }

    HAL_RESULT_PROPAGATE(ad7768_setup(&self->adc, hspi));
    
    //reconfigure ADC Sample rate
    HAL_RESULT_PROPAGATE(audio_set_sample_rate(self, config->audio_rate));

    //mask bytes
    HAL_RESULT_PROPAGATE(audio_set_sample_byte_mask(self, config->audio_ch_enabled, config->audio_ch_headers, config->audio_depth));
    return HAL_OK;
}

/* 
 * Desc: set audio manager sample byte mask 
 */
HAL_StatusTypeDef audio_set_sample_byte_mask(
    AudioManager *self, 
    uint8_t channel_enabled[4], 
    uint8_t channel_headers_enabled,
    TagConfigAudioSampleDepth channel_bit_depth
){
    uint32_t slot_mask = 0x0000FFFF;
    if(self == NULL){
        return HAL_ERROR;
    }

    /* reconfigure hsai slot mask basd on config settings*/
    self->active_channel_count = 4;
    
    if( !channel_headers_enabled ){
        slot_mask &= 0x00007777;  /* strip headers */
        self->bytes_per_channel -= 1;
    }

    if(channel_bit_depth == CFG_AUDIO_DEPTH_16_BIT){ 
        slot_mask &= 0x0000EEEE; /* strip LSB Byte */
        self->bytes_per_channel -= 1;
    }

    for(size_t i = 0; i < 4; i++){
        if( !channel_enabled[i] ){
            slot_mask &= ~(0x00000000F << (i * 4)); /* mask out channel */
            self->active_channel_count -= 1;
            HAL_RESULT_PROPAGATE(ad7768_set_ch_state(&self->adc, (ad7768_ch)i, AD7768_STANDBY));
            HAL_RESULT_PROPAGATE(ad7768_set_ch_mode(&self->adc, (ad7768_ch)i, AD7768_MODE_A));
        }
        else{
            HAL_RESULT_PROPAGATE(ad7768_set_ch_state(&self->adc, (ad7768_ch)i, AD7768_ENABLED));
            HAL_RESULT_PROPAGATE(ad7768_set_ch_state(&self->adc, (ad7768_ch)i, AD7768_MODE_B));
        }
    }

    /* apply mask and update peripheral */
    self->hsai->SlotInit.SlotActive = slot_mask;
    HAL_RESULT_PROPAGATE(HAL_SAI_Init(self->hsai));
    return HAL_OK;
}

/* 
 * Desc: set the audio sample rate
 */
HAL_StatusTypeDef audio_set_sample_rate(
    AudioManager *self, 
    TagConfigAudioSampleRate audio_rate
){
    if (audio_rate == CFG_AUDIO_RATE_96_KHZ){
        HAL_RESULT_PROPAGATE(ad7768_set_mclk_div(&self->adc, AD7768_MCLK_DIV_8));
        HAL_RESULT_PROPAGATE(ad7768_set_power_mode(&self->adc, AD7768_MEDIAN));
        HAL_RESULT_PROPAGATE(ad7768_set_dclk_div(&self->adc, AD7768_DCLK_DIV_2));

    } else if (audio_rate == CFG_AUDIO_RATE_192_KHZ){
        HAL_RESULT_PROPAGATE(ad7768_set_mclk_div(&self->adc, AD7768_MCLK_DIV_4));
        HAL_RESULT_PROPAGATE(ad7768_set_power_mode(&self->adc, AD7768_FAST));
        HAL_RESULT_PROPAGATE(ad7768_set_dclk_div(&self->adc, AD7768_DCLK_DIV_1));

    } else {
        return HAL_ERROR;
    }
    return HAL_OK;
}

// /*
//  *
//  */
HAL_StatusTypeDef audio_receive_dma(
    AudioManager *self;
    uint32_t sample_count;
//     //callback???
){
    uint32_t frame_size = self->bytes_per_channel*self->active_channel_count
    if (sample_count*frame_size <= UINT16_MAX){
        //simple circular dma
        HAL_SAI_RegisterCallback(&self->hsai, HAL_SAI_RX_HALFCOMPLETE_CB_ID, audio_SAI_RxHalfCpltCallback);
        HAL_SAI_RegisterCallback(&self->hsai, HAL_SAI_RX_COMPLETE_CB_ID, audio_SAI_RxCpltCallback);
        HAL_RESULT_PROPAGATE(HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)audio_buffer, 2*AUDIO_BUFFER_SIZE_BYTES));
    }else{
        //non-circular dma

    }
}

