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
#include "tx_api.h"
#include "app_filex.h"
#include "app_threadx.h"

extern uint8_t counter;
extern uint8_t done;
extern SPI_HandleTypeDef hspi1;
uint8_t temp_counter;

bool tick = 0;
uint8_t writeFull = 0;
uint8_t writeHalf = 0;
bool yielding = 0;

/*******************************
 * PUBLIC FUNCTION DEFINITIONS *
 *******************************/

ad7768_dev audio_adc = {
    .spi_handler = &hspi1,
		.spi_cs_port = ADC_CS_GPIO_Port,
		.spi_cs_pin = ADC_CS_Pin,
    .channel_standby = {
        .ch[0] = AD7768_ENABLED,
        .ch[1] = AD7768_ENABLED,
        .ch[2] = AD7768_ENABLED,
        .ch[3] = AD7768_STANDBY
    },
    .channel_mode[AD7768_MODE_A] = {.filter_type = AD7768_FILTER_SINC, .dec_rate = AD7768_DEC_X32},
    .channel_mode[AD7768_MODE_B] = {.filter_type = AD7768_FILTER_WIDEBAND, .dec_rate = AD7768_DEC_X32},
    .channel_mode_select = {
        .ch[0] = AD7768_MODE_B,
        .ch[1] = AD7768_MODE_B,
        .ch[2] = AD7768_MODE_B,
        .ch[3] = AD7768_MODE_A,
    },
    .power_mode = {
        .sleep_mode = AD7768_ACTIVE,
        .power_mode = AD7768_MEDIAN,
        .lvds_enable = false,
        .mclk_div = AD7768_MCLK_DIV_8,
    },
    .interface_config = {
        .crc_select = AD7768_CRC_NONE,
        .dclk_div = AD7768_DCLK_DIV_1,
    },
    .pin_spi_ctrl = AD7768_SPI_CTRL,
};


extern AudioManager audio;

extern SAI_HandleTypeDef hsai_BlockB1;
extern uint8_t counter;
extern uint8_t done;
uint8_t bad = 0;
uint8_t true_counter = 0;

extern uint8_t writeFull;
extern uint8_t writeHalf;

extern SD_HandleTypeDef hsd1;

extern uint8_t temp_counter;
FX_FILE         fs;
FX_FILE         audio_file = {};

extern FX_MEDIA        sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

extern TX_THREAD test_thread;
TX_EVENT_FLAGS_GROUP audio_event_flags_group;

void audio_SAI_RxCpltCallback (SAI_HandleTypeDef * hsai){

	counter += 2;
	memcpy(audio.temp_buffer[audio.temp_counter], audio.audio_buffer[1], AUDIO_CIRCULAR_BUFFER_SIZE);
	counter -= 2;

	audio.temp_counter++;

	if (audio.temp_counter == 10){
		audio.buffer_state = AUDIO_BUF_STATE_HALF_FULL;
		tx_event_flags_set(&audio_event_flags_group, 0x1, TX_OR);
	}

	if (audio.temp_counter == 20){
		tx_event_flags_set(&audio_event_flags_group, 0x2, TX_OR);
		//audio.buffer_state = AUDIO_BUF_STATE_FULL;
		audio.temp_counter = 0;
	}
}

void audio_SAI_RxHalfCpltCallback (SAI_HandleTypeDef * hsai){

	counter++;
	memcpy(audio.temp_buffer[audio.temp_counter], audio.audio_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE);
	counter -= 1;

	audio.temp_counter++;

	if (audio.temp_counter == 10){
		audio.buffer_state = AUDIO_BUF_STATE_HALF_FULL;
		tx_event_flags_set(&audio_event_flags_group, 0x1, TX_OR);
	}

	if (audio.temp_counter == 20){
		//audio.buffer_state = AUDIO_BUF_STATE_FULL;
		tx_event_flags_set(&audio_event_flags_group, 0x2, TX_OR);
		audio.temp_counter = 0;
	}
}

void audio_SDWriteComplete(FX_FILE *file){
	//HAL_SAI_DMAResume(&hsai_BlockB1);
	//audio.temp_counter = 0;
	//done++;

	temp_counter = 0;
	done++;
    audio.buffer_state = AUDIO_BUF_STATE_EMPTY;

}

void audio_thread_entry(ULONG thread_input){

	TagConfig tag_config = {.audio_ch_enabled[0] = 1,
	  	  	  	  	  	  	  	  .audio_ch_enabled[1] = 1,
								  .audio_ch_enabled[2] = 1,
								  .audio_ch_enabled[3] = 0,
	  	  	  	  	  	  	  	  .audio_ch_headers = 1,
								  .audio_rate = CFG_AUDIO_RATE_96_KHZ,
	  	  	  	  	  	  	  	  .audio_depth = CFG_AUDIO_DEPTH_16_BIT};

	  volatile UINT fx_result = FX_SUCCESS;
	  ULONG acc_flag_pointer = 0;
	  //fx_result = fx_media_open(&sdio_disk, "", fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *) fx_sd_media_memory, sizeof(fx_sd_media_memory));

	    if(fx_result != FX_SUCCESS){
	        Error_Handler();
	    }

	  fx_result = fx_file_create(&sdio_disk, "audio_test.bin");
	  if((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)){
	      Error_Handler();
	  }

	  fx_result = fx_file_open(&sdio_disk, &audio_file, "audio_test.bin", FX_OPEN_FOR_WRITE);
	  if(fx_result != FX_SUCCESS){
	      Error_Handler();
	  }

	  //ULONG x = 0;
	  //fx_result = fx_file_extended_allocate(&audio_file, AUDIO_CIRCULAR_BUFFER_SIZE * 2000);
	  if(fx_result != FX_SUCCESS){
	      Error_Handler();
	  }

	  fx_result = fx_file_write_notify_set(&audio_file, audio_SDWriteComplete);
	  if(fx_result != FX_SUCCESS){
	      Error_Handler();
	  }

	  HAL_SAI_RegisterCallback(&hsai_BlockB1, HAL_SAI_RX_HALFCOMPLETE_CB_ID, audio_SAI_RxHalfCpltCallback);
	  HAL_SAI_RegisterCallback(&hsai_BlockB1, HAL_SAI_RX_COMPLETE_CB_ID, audio_SAI_RxCpltCallback);

	  tx_event_flags_create(&audio_event_flags_group, "Audio Event Flags");

	  ad7768_setup(&audio_adc);

	  audio_init(&audio, &audio_adc, &hsai_BlockB1, &tag_config, &audio_file);

	  HAL_Delay(1000);

	  //dummy write
	  temp_counter++;

	  fx_file_write(audio.file, audio.temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 10);
	  HAL_Delay(1000);
	  temp_counter++;
	  fx_file_write(audio.file, audio.temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 10);
	  audio_record(&audio);

	  while (1){

		  yielding = true;
		  tx_event_flags_get(&audio_event_flags_group, 0x1 | 0x2, TX_OR_CLEAR, &acc_flag_pointer, TX_WAIT_FOREVER);
		  yielding = false;

		  if (acc_flag_pointer & 0x1){
      		temp_counter = 1;
      		fx_file_write(audio.file, audio.temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 10);
		  }

		  if (acc_flag_pointer & 0x2){
      		temp_counter = 1;
      		fx_file_write(audio.file, audio.temp_buffer[10], AUDIO_CIRCULAR_BUFFER_SIZE * 10);
		  }

		  //audio_tick(&audio);
		  while (temp_counter != 0);

	  }
	  //fx_file_close(&audio_file);
	  //fx_media_close(&sdio_disk);
}


/* 
 * Desc: initialize and configure audio manager
 */
HAL_StatusTypeDef audio_init(AudioManager *self, ad7768_dev *adc, SAI_HandleTypeDef *hsai, TagConfig *config, FX_FILE *file){
    self->adc = adc;
    self->sai = hsai;

    self->temp_counter = 0;
    if(config){
        //audio_configure(self, config);
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
    //self->sai->SlotInit.SlotActive = channel_bytemask;
    //HAL_RESULT_PROPAGATE(HAL_SAI_Init(self->sai) != HAL_OK)

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
        ad7768_set_mclk_div(self->adc, AD7768_MCLK_DIV_8);
        ad7768_set_power_mode(self->adc, AD7768_MEDIAN);
        ad7768_set_dclk_div(self->adc, AD7768_DCLK_DIV_4);

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
    HAL_SAI_Receive_DMA(self->sai, self->audio_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 2);
    return HAL_OK;
}

HAL_StatusTypeDef audio_tick(AudioManager *self){
	tick = !tick;
    switch (self->buffer_state){
        case AUDIO_BUF_STATE_HALF_FULL:
            //fx_file_write(self->file, self->audio_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE);

        	//move to sram

        	//if (self->temp_counter == 5){
        		temp_counter++;
        		fx_file_write(self->file, self->temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 10);
        	//}
        	//HAL_SRAM_Write_8b(hramcfg_SRAM1, SRAM_Pointer, self->audio_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE);
            //ToDo: Error Handling
            break;
        case AUDIO_BUF_STATE_FULL:
        	//memcpy(self->temp_buffer[self->temp_counter], self->audio_buffer[1], AUDIO_CIRCULAR_BUFFER_SIZE);
        	//self->temp_counter++;
        	//counter -= 2;
        	//self->buffer_state = AUDIO_BUF_STATE_EMPTY;

        	//if (self->temp_counter == 5){
        		temp_counter++;
        		fx_file_write(self->file, self->temp_buffer[10], AUDIO_CIRCULAR_BUFFER_SIZE * 10);
        	//}
            //fx_file_write(self->file, self->audio_buffer[10], AUDIO_CIRCULAR_BUFFER_SIZE);
            //ToDo: Error Handling
            break;
        case AUDIO_BUF_STATE_EMPTY:
            break;
        default:
            break;
            
    }

    return HAL_OK;
}
