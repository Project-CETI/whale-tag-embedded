/*
 * log.h
 *
 *  Created on: May 02, 2023
 *      Author: Michael Salino-Hugg (email: msalinohugg@seas.harvard.edu)
 *
 *  Description:
 *    This file logs events to a errors.log file on the SD card.
 */
#ifndef INC_LOG_H_
#define INC_LOG_H_
typedef enum {
    LOG_WARN_BUFFER_NOT_CLEARED,
}LogWarning;

typedef enum {
    LOG_ERR_SD_CARD_WRITE_ERROR,
}LogError;

void log_warn(LogWarning warning);
#endif //INC_LOG_H_