
#include "audio.h"

HAL_StatusTypeDef audio_init(
    AudioManager *self, 
    SPI_HandleTypeDef *hspi,
    SAI_HandleTypeDef *hsai,
    TagConfig *config
){

    ad7768_setup(&self->adc, hspi, hsai);
    //ToDo: set adc sample rate from config

    /* reconfigure hsai slot mask basd on config settings*/
    uint32_t slot_mask = 0x0000FFFF;
    self->sample_size = 4;
    self->active_channel_count = 4;
    
    if( !config->audio_ch_headers ){
        slot_mask &= 0x00007777;  /* strip headers */
        self->sample_size -= 1;
    }

    if(config->audio_depth == CFG_AUDIO_DEPTH_16_BIT){ 
        slot_mask &= 0x0000EEEE; /* strip LSB Byte */
        self->sample_size -= 1;
    }

    for(size_t i = 0; i < 4; i++){
        if( !config->audio_ch_enabled[i] ){
            slot_mask &= ~(0x00000000F << (i * 4)); /* mask out channel */
            self->active_channel_count -= 1;
        }
    }

    hsai->SlotInit.SlotActive = slot_mask;
    return HAL_SAI_Init(hsai);
}
