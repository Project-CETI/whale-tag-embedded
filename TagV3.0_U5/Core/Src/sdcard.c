/*
 * sdcard.c
 *
 *  Created on: Apr 27, 2023
 *      Author: mis5323
 */
//#include "sdcard.h"
//
///* Buffer for FileX FX_MEDIA sector cache. */
//ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);
//
//void SDCard_init(SDCard *sd){
//    uint32_t result;
//    result = fx_media_open(&sd->sdio_disk, "", fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *) fx_sd_media_memory, sizeof(fx_sd_media_memory));
//    if(result != FX_SUCCESS){
//        return result;
//    }
//
//    result = fx_directory_create(&sd->sdio_disk, audio_dir);
//    if((result != FX_SUCCESS) && (result != FX_ALREADY_CREATED)){
//        return result;
//    }
//
//    fx_file_open(&sd->sdio_disk, &sd->config_file, "config.txt")
//
//}
