//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef IMU_LOG_H
#define IMU_LOG_H

int imu_init_data_files(void);
void *imu_log_thread(void *paramPtr);

#endif // IMU_LOG_H