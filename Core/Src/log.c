/*
 * log.c
 *
 *  Created on: May 02, 2023
 *      Author: Michael Salino-Hugg (email: msalinohugg@seas.harvard.edu)
 *
 *  Description:
 *    This file logs events to a errors.log file on the SD card.
 */
#include "log.h"
#include "stdio.h"
#include "app_filex.h"

static FX_FILE log_file;

void log_warn(LogWarning warning){
            //ToDo: inlcude timestamp "%d: warn: "
    switch (warning){
        case LOG_WARN_BUFFER_NOT_CLEARED:
            fx_file_write(&log_file, "Back buffer was not cleared! Potential data loss!", 50);
            break;

    }
}

void log_error(LogError error){
            //ToDo: inlcude timestamp "%d:  ERR: "
    switch (error){
        case LOG_ERR_SD_CARD_WRITE_ERROR:
            fx_file_write(&log_file, "Failed to write to the SD card", 31);
            break;

    }
}