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

typedef enum {
    THREAD_ERR_FAILED_FILE_CREATE = 1,
    THREAD_ERR_FAILED_SHM_CREATE = 2,
    THREAD_ERR_FAILED_SEM_CREATE = 3,
} ThreadErrorKind;

/* ThreadError is actually a struct defined as such
{
    uint8_t thread_id;
    uint8_t device_id;
    uint16_t error_code;
}

The convinient thing here is that errors as a number can be easily compared
and passed. This is also compatible with the device error type `WTResult`
found in `utils/error.h` as the thread ID can simply be concatenated to the
value as the MSB.
*/
typedef uint32_t ThreadError;

#define THREAD_OK (0)
#define THREAD_ERR(thread, error) (((thread & 0xFF) << 24) | ((uint32_t)error & 0x00FFFFFF))
#define THREAD_ERR_GET_THREAD(thread_error) ((thread_error >> 24) & 0xFF)
#define THREAD_ERR_GET_DEVICE(thread_error) ((thread_error >> 16) & 0xFF)
#define THREAD_ERR_GET_ERROR(thread_error) (thread_error & 0xFFFF)

#endif