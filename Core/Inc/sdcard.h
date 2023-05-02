/*
 * sdcard.h
 *
 *  Created on: Apr 27, 2023
 *      Author: mis5323
 */

#ifndef INC_SDCARD_H_
#define INC_SDCARD_H_
#include "app_filex.h"

const char audio_dir[] = "AUDIO";
const char sensor_dir[] = "SENSOR";

typedef struct {
    FX_MEDIA sdio_disk;
    FX_FILE  config_file;
    FX_FILE  audio_file;
    FX_FILE  sensor_file;
    FX_FILE  log_file;
}SDCard;

/* setup SDCard */
void SDCard_init(SDCard *sd);
#endif /* INC_SDCARD_H_ */
