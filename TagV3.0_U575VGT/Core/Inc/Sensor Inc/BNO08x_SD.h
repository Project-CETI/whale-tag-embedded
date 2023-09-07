/*
 * BNO08x_SD.h
 *
 *  Created on: Aug 8, 2023
 *      Author: Kaveet
 *
 *  The thread responsible for writing the IMU data collected by the IMU Data collection driver (BNO08x.h) to the SD card.
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

#ifndef INC_SENSOR_INC_BNO08X_SD_H_
#define INC_SENSOR_INC_BNO08X_SD_H_

#include "Sensor Inc/BNO08x.h"
#include "tx_api.h"

void imu_sd_thread_entry(ULONG thread_input);

#endif /* INC_SENSOR_INC_BNO08X_SD_H_ */
