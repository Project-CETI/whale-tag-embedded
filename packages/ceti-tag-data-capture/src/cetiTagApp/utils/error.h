//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __LIB_WHALE_TAG_ERROR_H__
#define __LIB_WHALE_TAG_ERROR_H__

#include <stdint.h>

typedef enum wt_device_id_e {
    WT_DEV_NONE = 0,
    WT_DEV_FPGA,
    WT_DEV_AUDIO,
    WT_DEV_BMS,
    WT_DEV_ECG_ADC,
    WT_DEV_IMU,
    WT_DEV_IOX,
    WT_DEV_LIGHT,
    WT_DEV_PRESSURE,
    WT_DEV_RECOVERY,
    WT_DEV_RTC,
    WT_DEV_TEMPERATURE,
    WT_DEV_BURNWIRE,
} WTDeviceID;

/**
 * @brief Packs WtResult error code based on device enum and error value.
 */
#define WT_RESULT(dev, status) ((((dev) & ((1 << 16) - 1)) << 16) | (((status) & ((1 << 16) - 1)) << 0))
#define WT_OK 0

/* positive number indicating to check errno*/
#define WT_ERR_FILE_OPEN 1
#define WT_ERR_FILE_READ 2
#define WT_ERR_FILE_WRITE 3
#define WT_ERR_MALLOC -147


#define WT_ERR_AUDIO_START -148
#define WT_ERR_BAD_AUDIO_BITDEPTH (WT_ERR_AUDIO_START - 0)
#define WT_ERR_BAD_AUDIO_FILTER   (WT_ERR_AUDIO_START - 1)
#define WT_ERR_BAD_AUDIO_SAMPLE_RATE   (WT_ERR_AUDIO_START - 2)

#define WT_ERR_BMS_START  (WT_ERR_AUDIO_START - 3)
#define WT_ERR_BMS_WRITE_PROT_DISABLE_FAIL (WT_ERR_BMS_START - 0)
#define WT_ERR_BMS_BAD_CELL_INDEX (WT_ERR_BMS_START - 1)

#define WT_ERR_ECG_START  (WT_ERR_BMS_START - 2)
#define WT_ERR_BAD_ECG_DATA_RATE (WT_ERR_ECG_START - 0)
#define WT_ERR_BAD_ECG_CHANNEL  (WT_ERR_ECG_START - 1)
#define WT_ERR_ECG_TIMEOUT  (WT_ERR_ECG_START - 2)

#define WT_ERR_FPGA_START  (WT_ERR_ECG_START - 3)
#define WT_ERR_FPGA_N_DONE (WT_ERR_FPGA_START - 0)

#define WT_ERR_IMU_START  (WT_ERR_FPGA_START - 1)
#define WT_ERR_IMU_BAD_PKT_SIZE    (WT_ERR_IMU_START - 0)
#define WT_ERR_IMU_UNEXPECTED_PKT_TYPE  (WT_ERR_IMU_START - 1)

#define WT_ERR_IOX_START (WT_ERR_IMU_START - 2)
#define WT_ERR_BAD_IOX_GPIO (WT_ERR_IOX_START - 0)
#define WT_ERR_BAD_IOX_MODE (WT_ERR_IOX_START - 1)

#define WT_ERR_RECOVERY_START  (WT_ERR_IOX_START - 2)
#define WT_ERR_RECOVERY_TIMEOUT  (WT_ERR_RECOVERY_START - 0)
#define WT_ERR_RECOVERY_BAD_CALLSIGN  (WT_ERR_RECOVERY_START - 1)
#define WT_ERR_RECOVERY_BAD_FREQ  (WT_ERR_RECOVERY_START - 2)
#define WT_ERR_RECOVERY_OVERSIZED_COMMENT  (WT_ERR_RECOVERY_START - 3)
#define WT_ERR_RECOVERY_OVERSIZED_MESSAGE  (WT_ERR_RECOVERY_START - 4)
#define WT_ERR_RECOVERY_UNDERSIZED_GPS_BUFFER  (WT_ERR_RECOVERY_START - 5)

/**
 * @brief WTResult is a 32-bit error code the 16 upper bits are the device id
 *      number, and the 16 lower bits are the error code. Error codes are taken
 *      directly from pigpio, unless defined in "libwhaletag/error.h".
 */
typedef uint32_t WTResult;


/**
 * @brief Returns a pointer to an error description string based on the errno. Note consectuive calls to
 *      `wt_strerror` will modify the contents inside the returned string pointer.
 * 
 * @param errno 
 * @return const char* 
 */
const char *wt_strerror(WTResult errno);

/**
 * @brief Macro that tries to perform action that may return on error in the
 * form of a WtResult. On failure, `__VA_OPT__` instuctions are performed prior
 * to return being called in the calling function.
 */
#define WT_TRY(body, ...) {WTResult result = (body); if( result != WT_OK) {__VA_OPT__((__VA_ARGS__);) return result;}}

/**
 * @brief Macro that tries to perform the pigpio instruction `body`.  
 * On success the macro has a value of the `body` output.  
 * On failure, `__VA_OPT__` instuctions are performed prior
 * to a `WtResult` associated with `dev` and error being returned by the calling function.
 */
#define PI_TRY(dev, body, ...) ({        \
    int result = (body);                 \
    if( result < 0 ){                    \
        __VA_OPT__((__VA_ARGS__);)       \
        return WT_RESULT((dev), result); \
    }                                    \
    result;                              \
})

#endif // __LIB_WHALE_TAG_ERROR_H__
 