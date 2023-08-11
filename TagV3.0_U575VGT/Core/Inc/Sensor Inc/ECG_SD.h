/*
 * ECG_SD.h
 *
 *  Created on: Aug 8, 2023
 *      Author: Kaveet
 *
 *  The thread responsible for writing the ECG data collected by the ECG Data collection driver (ECG.h) to the SD card.
 *
 *  The two threads share a buffer split in half, with each half protected by a mutex.
 *
 *  When the data collection driver finishes filling up one half, this thread will begin writing that thread to the SD card.
 *
 *  This avoids missing data during the write period.
 *
 *  This process is continuously repeated through a circular buffer.
 *
 *  Essentially, this tries to "mock" a circular DMA buffer using RTOS threads and a mutex
 */
#ifndef INC_SENSOR_INC_ECG_SD_H_
#define INC_SENSOR_INC_ECG_SD_H_

#include "Sensor Inc/ECG.h"
#include "tx_api.h"

//Our thread entry for the ECG sd card writing thread
void ecg_sd_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_ECG_SD_H_ */
