//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __LIB_WHALE_TAG_THREAD_ERROR_H__
#define __LIB_WHALE_TAG_THREAD_ERROR_H__

#include "error.h"
#include <stdint.h>

typedef enum {
    THREAD_MAIN = 0,
    THREAD_COMMAND_PROCESS = 1,
    THREAD_MISSION = 2,
    THREAD_RTC_ACQ = 3,
    THREAD_IMU_ACQ = 4,
    THREAD_ALS_ACQ = 5,
    THREAD_PRESSURE_ACQ = 6,
    THREAD_BMS_ACQ = 7,
    THREAD_GPS_ACQ = 8,
    THREAD_ECG_LOD_ACQ = 9,
    THREAD_ECG_ACQ = 10,
    THREAD_META_ACQ = 11,
    THREAD_AUDIO_ACQ = 12,
    THREAD_AUDIO_LOG = 13,
    THREAD_ECG_LOG = 14,
} ThreadKind;

#define THREAD_OK 0
#define THREAD_ERR_SHM_FAILED (1 << 0)
#define THREAD_ERR_SEM_FAILED (1 << 1)
#define THREAD_ERR_DATA_FILE_FAILED (1 << 2)
#define THREAD_ERR_HW (1 << 3)

void update_thread_device_status(ThreadKind thread_id, WTResult device_status, const char *thread_name);

#endif