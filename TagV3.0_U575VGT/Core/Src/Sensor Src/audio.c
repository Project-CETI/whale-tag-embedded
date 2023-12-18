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

#include "Sensor Inc/audio.h"
#include "Sensor Inc/RTC.h"
#include "Lib Inc/threads.h"
#include "util.h"
#include "fx_api.h"
#include "tx_api.h"
#include "app_filex.h"
#include "app_threadx.h"
#include "main.h"
#include "ux_device_cdc_acm.h"
#include <stdbool.h>
#include <stdio.h>

/*******************************
 * PUBLIC FUNCTION DEFINITIONS *
 *******************************/

//External HAL handlers for configuration
extern SPI_HandleTypeDef hspi1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern SD_HandleTypeDef hsd1;
extern UART_HandleTypeDef huart2;

//RTC current time
extern RTC_TimeTypeDef eTime;
extern RTC_DateTypeDef eDate;
extern RTC_TimeTypeDef sTime;

//USB buffers
extern uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitBuf[APP_RX_DATA_SIZE];
extern uint8_t usbTransmitLen;

//Statically declare ADC at runtime
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

//Use the audio manager declared in Main since it uses too much memory for a ThreadX thread (nearly 700kB)
extern AudioManager audio;

//FileX variables
FX_FILE audio_file = {};
extern FX_MEDIA sdio_disk;
extern ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

//Event flags for signaling data ready
TX_EVENT_FLAGS_GROUP audio_event_flags_group;

//Status variable to indicate thread is writing to SD card
bool audio_writing = 0;

// uart debugging
bool audio_get_samples = false;
uint16_t audio_sample_count = 0;
HAL_StatusTypeDef audio_unit_test_ret = HAL_ERROR;

void audio_SAI_RxCpltCallback (SAI_HandleTypeDef * hsai){

	//Our DMA buffer is completely filled, move the upper half into the temporary buffer
	memcpy(audio.temp_buffer[audio.temp_counter], audio.audio_buffer[1], AUDIO_CIRCULAR_BUFFER_SIZE);

	audio.temp_counter++;

	//Once the temporary buffer is half full, set the flag for the thread execution loop
	if (audio.temp_counter == TEMP_BUF_HALF_BLOCK_LENGTH){
		tx_event_flags_set(&audio_event_flags_group, AUDIO_BUFFER_HALF_FULL_FLAG, TX_OR);
	}

	//Same as above, but when it is full & reset our temp buffer index tracker
	if (audio.temp_counter == TEMP_BUF_BLOCK_LENGTH){
		tx_event_flags_set(&audio_event_flags_group, AUDIO_BUFFER_FULL_FLAG, TX_OR);
		audio.temp_counter = 0;
	}
}

void audio_SAI_RxHalfCpltCallback (SAI_HandleTypeDef * hsai){

	//Our DMA buffer is half full, move the lower half into the temp buffer
	memcpy(audio.temp_buffer[audio.temp_counter], audio.audio_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE);

	audio.temp_counter++;

	//Half full temp buffer, set flag for thread execution loop
	if (audio.temp_counter == TEMP_BUF_HALF_BLOCK_LENGTH){
		tx_event_flags_set(&audio_event_flags_group, AUDIO_BUFFER_HALF_FULL_FLAG, TX_OR);
	}

	//Full temp buffer, set flag & reset temp buffer index
	if (audio.temp_counter == TEMP_BUF_BLOCK_LENGTH){
		tx_event_flags_set(&audio_event_flags_group, AUDIO_BUFFER_FULL_FLAG, TX_OR);
		audio.temp_counter = 0;
	}
}

void audio_SDWriteComplete_Callback(FX_FILE *file){

	//Set polling flag to indicate a completed SD card write
	audio_writing = false;
	audio.sd_write_complete = true;
}

void audio_thread_entry(ULONG thread_input){

	//Tag configuration
	TagConfig tag_config = {.audio_ch_enabled[0] = 1,
	  	  	  	  	  	  	  	  .audio_ch_enabled[1] = 1,
								  .audio_ch_enabled[2] = 1,
								  .audio_ch_enabled[3] = 0,
	  	  	  	  	  	  	  	  .audio_ch_headers = 0,
								  .audio_rate = CFG_AUDIO_RATE_96_KHZ,
	  	  	  	  	  	  	  	  .audio_depth = CFG_AUDIO_DEPTH_16_BIT};

	char audio_file_name[30] = {0};
	char tmp_audio_file_name[30] = {0};
	sprintf(audio_file_name, "audio_%d_%d_%d_%d_%d.bin", eDate.Month, eDate.Date, eTime.Hours, eTime.Minutes, eTime.Seconds);

	//Create our binary file for dumping audio data
	UINT fx_result = FX_ACCESS_ERROR;
	fx_result = fx_file_create(&sdio_disk, audio_file_name);
	if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
		Error_Handler();
	}

	//Close file in case already opened
	fx_result = fx_file_close(&audio_file);

	//Open binary file
	fx_result = fx_file_open(&sdio_disk, &audio_file, audio_file_name, FX_OPEN_FOR_WRITE);
	if (fx_result != FX_SUCCESS) {
		Error_Handler();
	}

	//Set our "write complete" callback function
	fx_result = fx_file_write_notify_set(&audio_file, audio_SDWriteComplete_Callback);
	if (fx_result != FX_SUCCESS) {
		Error_Handler();
	}

	//Set our DMA buffer callbacks for both half full and full
	HAL_SAI_RegisterCallback(&hsai_BlockB1, HAL_SAI_RX_HALFCOMPLETE_CB_ID, audio_SAI_RxHalfCpltCallback);
	HAL_SAI_RegisterCallback(&hsai_BlockB1, HAL_SAI_RX_COMPLETE_CB_ID, audio_SAI_RxCpltCallback);

	//Create our event flags group
	tx_event_flags_create(&audio_event_flags_group, "Audio Event Flags");

	//Setup the ADC and sync it
	ad7768_setup(&audio_adc);

	//Initialize our audio manager
	audio_init(&audio, &audio_adc, &hsai_BlockB1, &tag_config, &audio_file);

	//Dummy delay
	HAL_Delay(1000);

	//The first write is usually the slowest, add in a dummy write to get it out of the way
	audio.sd_write_complete = false;
	fx_file_write(audio.file, audio.temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 10);

	//Poll for completion
	while (!audio.sd_write_complete);

	//Start gathering audio data through the SAI and DMA
	HAL_StatusTypeDef ret = audio_record(&audio);
	audio_unit_test_ret = ret;

	while (1) {
		ULONG actual_flags = 0;

		// wait for any debugging flag
		tx_event_flags_get(&audio_event_flags_group, AUDIO_UNIT_TEST_FLAG | AUDIO_READ_FLAG | AUDIO_CMD_FLAG, TX_OR_CLEAR, &actual_flags, 1);
		if (actual_flags & AUDIO_UNIT_TEST_FLAG) {
			tx_event_flags_set(&audio_event_flags_group, AUDIO_UNIT_TEST_DONE_FLAG, TX_OR);
		}
		else if (actual_flags & AUDIO_CMD_FLAG) {
			if (usbReceiveBuf[3] == AUDIO_GET_SAMPLES_CMD) {
				audio_get_samples = true;
			}
		}

		// check time to create new file
		if (abs(eTime.Minutes - sTime.Minutes) % RTC_AUDIO_REFRESH_MINS == 0) {

			//Create new audio file name
			sprintf(tmp_audio_file_name, "audio_%d_%d_%d_%d_%d.bin", eDate.Month, eDate.Date, eTime.Hours, eTime.Minutes, eTime.Seconds);

			//Only create new file if minutes is different or minutes is same and hours is different
			if ((tmp_audio_file_name[AUDIO_FILENAME_MINS_INDEX] != audio_file_name[AUDIO_FILENAME_MINS_INDEX]) || ((tmp_audio_file_name[AUDIO_FILENAME_HOURS_INDEX] != audio_file_name[AUDIO_FILENAME_HOURS_INDEX]) && tmp_audio_file_name[AUDIO_FILENAME_MINS_INDEX] == audio_file_name[AUDIO_FILENAME_MINS_INDEX])) {

				//Name new audio file
				strcpy(audio_file_name, tmp_audio_file_name);

				//Close previous audio file
				fx_result = fx_file_close(&audio_file);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Create new audio file
				fx_result = fx_file_create(&sdio_disk, audio_file_name);
				if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
					Error_Handler();
				}

				//Close file in case already opened
				fx_result = fx_file_close(&audio_file);

				//Open new audio file for writing
				fx_result = fx_file_open(&sdio_disk, &audio_file, audio_file_name, FX_OPEN_FOR_WRITE);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}

				//Set our "write complete" callback function
				fx_result = fx_file_write_notify_set(&audio_file, audio_SDWriteComplete_Callback);
				if (fx_result != FX_SUCCESS) {
					Error_Handler();
				}
			}
		}

		//Wait for the temp buffer to be either half of fully full. This suspends the audio task and lets others run.
		tx_event_flags_get(&audio_event_flags_group, AUDIO_BUFFER_FULL_FLAG | AUDIO_BUFFER_HALF_FULL_FLAG | AUDIO_STOP_THREAD_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

		//If half full, write the bottom half
		if (actual_flags & AUDIO_BUFFER_HALF_FULL_FLAG){
			if (audio_get_samples) {
				// send imu data over uart and usb
				HAL_UART_Transmit_IT(&huart2, &ret, 1);
				HAL_UART_Transmit_IT(&huart2, (uint8_t *) &audio.temp_buffer[0], AUDIO_NUM_SAMPLES);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					memcpy(&usbTransmitBuf[1], &audio.temp_buffer[0], AUDIO_NUM_SAMPLES);
					usbTransmitLen = AUDIO_NUM_SAMPLES + 1;
				}

				audio_get_samples = false;
			}

			audio.sd_write_complete = false;
			audio_writing = true;
			fx_file_write(audio.file, audio.temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * TEMP_BUF_HALF_BLOCK_LENGTH);
		}

		//If full, write the top half
		if (actual_flags & AUDIO_BUFFER_FULL_FLAG){
			if (audio_get_samples) {
				// send imu data over uart and usb
				HAL_UART_Transmit_IT(&huart2, &ret, 1);
				HAL_UART_Transmit_IT(&huart2, (uint8_t *) &audio.temp_buffer[0], AUDIO_NUM_SAMPLES);

				if (usbTransmitLen == 0) {
					memcpy(&usbTransmitBuf[0], &ret, 1);
					memcpy(&usbTransmitBuf[1], &audio.temp_buffer[0], AUDIO_NUM_SAMPLES);
					usbTransmitLen = AUDIO_NUM_SAMPLES + 1;
				}

				audio_get_samples = false;
			}

			audio.sd_write_complete = false;
			audio_writing = true;
			fx_file_write(audio.file, audio.temp_buffer[TEMP_BUF_HALF_BLOCK_LENGTH], AUDIO_CIRCULAR_BUFFER_SIZE * TEMP_BUF_HALF_BLOCK_LENGTH);
		}

	  //Poll for completion, this blocks out other tasks but is *neccessary*
	  //We block out the other tasks to prevent unneccessary context switches which would slow down the SD card writes significantly, to the point where we would lose data.
	  while (!audio.sd_write_complete);

	  //If we need to stop the thread, stop the data collection and suspend the thread
	  if (actual_flags & AUDIO_STOP_THREAD_FLAG){

		  //Stop DMA buffer
		  HAL_SAI_DMAPause(audio.sai);

		  //Close file
		  fx_file_close(audio.file);

		  //Terminate thread so it needs to be fully reset to start again
		  //tx_event_flags_delete(&audio_event_flags_group);
		  tx_thread_terminate(&threads[AUDIO_THREAD].thread);
	  }

	}
}


//TODO: Finsih audio initialization and configuration functions
/* 
 * Desc: initialize and configure audio manager
 */
HAL_StatusTypeDef audio_init(AudioManager *self, ad7768_dev *adc, SAI_HandleTypeDef *hsai, TagConfig *config, FX_FILE *file){
    self->adc = adc;
    self->sai = hsai;

    self->temp_counter = 0;
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
    
    /*
    if( config->audio_depth == CFG_AUDIO_DEPTH_16_BIT ){
        channel_bytemask &= 0x0000EEEE; // 0x00003333
    }
    */

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

    /*
    self->sai->SlotInit.SlotActive = channel_bytemask;
    HAL_RESULT_PROPAGATE(HAL_SAI_Init(self->sai) != HAL_OK)
    */

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
        ad7768_set_dclk_div(self->adc, AD7768_DCLK_DIV_1); //4, could try 8

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
    HAL_StatusTypeDef ret = HAL_SAI_Receive_DMA(self->sai, self->audio_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * 2);
    return ret;
}

HAL_StatusTypeDef audio_tick(AudioManager *self){
    switch (self->buffer_state){
        case AUDIO_BUF_STATE_HALF_FULL:
        	fx_file_write(self->file, self->temp_buffer[0], AUDIO_CIRCULAR_BUFFER_SIZE * TEMP_BUF_HALF_BLOCK_LENGTH);

            //ToDo: Error Handling
            break;
        case AUDIO_BUF_STATE_FULL:
        	fx_file_write(self->file, self->temp_buffer[10], AUDIO_CIRCULAR_BUFFER_SIZE * TEMP_BUF_HALF_BLOCK_LENGTH);

            //ToDo: Error Handling
            break;
        case AUDIO_BUF_STATE_EMPTY:
            break;
        default:
            break;
    }
    return HAL_OK;
}
