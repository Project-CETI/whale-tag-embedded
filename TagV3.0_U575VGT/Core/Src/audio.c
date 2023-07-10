/* audio_setup
//attach adc, filex
// audio_configure()
//setup file_x
*/

/* audio_configure()
//setup adc
//apply config settings to adc and sai
*/

/* audio_record()
// start audio circular buffer
// enable RAM->SRAM_ext DMA
// enable SRAM_ext->SD Card
*/

/* audio_halt()
// stop circular buffer
// write SRAM_ext to SD_Card
*/

/* audio_unit_test()
*/

#include "audio.h"
#include "util.h"
#include "fx_api.h"

#define AUDIO_CIRCULAR_BUFFER_SIZE_MAX (UINT16_MAX/2)
#define AUDIO_CIRCULAR_BUFFER_SIZE  AUDIO_CIRCULAR_BUFFER_SIZE_MAX

static uint8_t audio_circular_buffer[2][AUDIO_CIRCULAR_BUFFER_SIZE];
/*******************************
 * PUBLIC FUNCTION DEFINITIONS *
 *******************************/
/* 
 * Desc: initialize and configure audio manager
 */
HAL_StatusTypeDef audio_init(AudioManager *self, ad7768_dev *adc, SAI_HandleTypeDef *hsai, TagConfig *config, FX_FILE *file){
    self->adc = adc;
    self->sai = hsai;
    if(config){
        audio_configure(self, config);
    }
    self->file = file;
    return HAL_OK;
}

HAL_StatusTypeDef audio_configure(AudioManager *self, TagConfig *config){
    uint32_t channel_bytemask = 0x0000FFFF;
    
    if( !config->audio_ch_headers ){
        channel_bytemask &= 0x00007777;
    }
    
    if( config->audio_depth == CFG_AUDIO_DEPTH_16_BIT ){
        channel_bytemask &= 0x0000EEEE;
    }

    for(uint_fast8_t ch = 0; ch < 4; ch++){
        if( !config->audio_ch_enabled[ch] ){ //disable channel
            channel_bytemask &= ~(0x0000000F << (4*ch));
            ad7768_set_ch_mode(self->adc, (ad7768_ch)ch, AD7768_MODE_A);
            ad7768_set_ch_state(self->adc, (ad7768_ch)ch, AD7768_SLEEP);
        }
        else{ //enable channel
            ad7768_set_ch_mode(self->adc, (ad7768_ch)ch, AD7768_MODE_B);
            ad7768_set_ch_state(self->adc, (ad7768_ch)ch, AD7768_ACTIVE);
        }
    }
    self->sai->SlotInit.SlotActive = channel_bytemask;
    HAL_RESULT_PROPAGATE(HAL_SAI_Init(self->sai) != HAL_OK)

    HAL_RESULT_PROPAGATE(audio_set_sample_rate(self, config->audio_rate));
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
        HAL_RESULT_PROPAGATE(ad7768_set_mclk_div(self->adc, AD7768_MCLK_DIV_8));
        HAL_RESULT_PROPAGATE(ad7768_set_power_mode(self->adc, AD7768_MEDIAN));
        HAL_RESULT_PROPAGATE(ad7768_set_dclk_div(self->adc, AD7768_DCLK_DIV_2));

    } else if (audio_rate == CFG_AUDIO_RATE_192_KHZ){
        HAL_RESULT_PROPAGATE(ad7768_set_mclk_div(self->adc, AD7768_MCLK_DIV_4));
        HAL_RESULT_PROPAGATE(ad7768_set_power_mode(self->adc, AD7768_FAST));
        HAL_RESULT_PROPAGATE(ad7768_set_dclk_div(self->adc, AD7768_DCLK_DIV_1));

    } else {
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef audio_record(AudioManager *self){
    HAL_SAI_Receive_DMA(self->sai, self->audio_buffer, AUDIO_CIRCULAR_BUFFER_SIZE);
    return HAL_OK;
}

HAL_StatusTypeDef audio_tick(AudioManager *self){
    switch (self->buffer_state){
        case AUDIO_BUF_STATE_HALF_FULL:
            fx_file_write(self->file, audio_circular_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE);
            //ToDo: Error Handling
            break;
        case AUDIO_BUF_STATE_FULL:
            fx_file_write(self->file, audio_circular_buffer[1], AUDIO_CIRCULAR_BUFFER_SIZE);
            //ToDo: Error Handling
            break;
        case AUDIO_BUF_STATE_EMPTY:
            break;
        default:
            break;
            
    }
    return HAL_OK;
}
