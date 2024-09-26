//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "error.h"

#include <errno.h>
#include <pigpio.h>
#include <stdio.h>
#include <string.h>


static const char *device_name[] = {
    [WT_DEV_NONE] = "",
    [WT_DEV_FPGA] = "FPGA",
    [WT_DEV_AUDIO] = "Audio",
    [WT_DEV_BMS] = "Battery management system",
    [WT_DEV_ECG_ADC] = "ECG ADC",
    [WT_DEV_IMU] = "IMU",
    [WT_DEV_IOX] = "IO expander",
    [WT_DEV_LIGHT] = "Ambient light sensor",
    [WT_DEV_PRESSURE] = "Pressure sensor",
    [WT_DEV_RECOVERY] = "Recovery board",
    [WT_DEV_RTC] = "Real-time clock",
    [WT_DEV_TEMPERATURE] = "Temperature sensor",
    [WT_DEV_BURNWIRE] = "Burnwire",
};

static const char *err_str[] = { 
    [-PI_INIT_FAILED] =      "gpioInitialise failed",
    [-PI_BAD_USER_GPIO] =    "GPIO not 0-31",
    [-PI_BAD_GPIO] =         "GPIO not 0-53",
    [-PI_BAD_MODE] =         "mode not 0-7",
    [-PI_BAD_LEVEL] =        "level not 0-1",
    [-PI_BAD_PUD] =          "pud not 0-2",
    [-PI_BAD_PULSEWIDTH] =   "pulsewidth not 0 or 500-2500",
    [-PI_BAD_DUTYCYCLE] =    "dutycycle outside set range",
    [-PI_BAD_TIMER] =        "timer not 0-9",
    [-PI_BAD_MS] =           "ms not 10-60000",
    [-PI_BAD_TIMETYPE] =     "timetype not 0-1",
    [-PI_BAD_SECONDS] =      "seconds < 0",
    [-PI_BAD_MICROS] =       "micros not 0-999999",
    [-PI_TIMER_FAILED] =     "gpioSetTimerFunc failed",
    [-PI_BAD_WDOG_TIMEOUT] = "timeout not 0-60000",
    [-PI_NO_ALERT_FUNC] =    "DEPRECATED",
    [-PI_BAD_CLK_PERIPH] =   "clock peripheral not 0-1",
    [-PI_BAD_CLK_SOURCE] =   "DEPRECATED",
    [-PI_BAD_CLK_MICROS] =   "clock micros not 1, 2, 4, 5, 8, or 10",
    [-PI_BAD_BUF_MILLIS] =   "buf millis not 100-10000",
    [-PI_BAD_DUTYRANGE] =    "dutycycle range not 25-40000",
    [-PI_BAD_DUTY_RANGE] =   "DEPRECATED (use PI_BAD_DUTYRANGE)",
    [-PI_BAD_SIGNUM] =       "signum not 0-63",
    [-PI_BAD_PATHNAME] =     "can't open pathname",
    [-PI_NO_HANDLE] =        "no handle available",
    [-PI_BAD_HANDLE] =       "unknown handle",
    [-PI_BAD_IF_FLAGS] =     "ifFlags > 4",
    [-PI_BAD_CHANNEL] =      "DMA channel not 0-15",
    [-PI_BAD_PRIM_CHANNEL] = "DMA primary channel not 0-15",
    [-PI_BAD_SOCKET_PORT] =  "socket port not 1024-32000",
    [-PI_BAD_FIFO_COMMAND] = "unrecognized fifo command",
    [-PI_BAD_SECO_CHANNEL] = "DMA secondary channel not 0-15",
    [-PI_NOT_INITIALISED] =  "function called before gpioInitialise",
    [-PI_INITIALISED] =      "function called after gpioInitialise",
    [-PI_BAD_WAVE_MODE] =    "waveform mode not 0-3",
    [-PI_BAD_CFG_INTERNAL] = "bad parameter in gpioCfgInternals call",
    [-PI_BAD_WAVE_BAUD] =    "baud rate not 50-250K(RX)/50-1M(TX)",
    [-PI_TOO_MANY_PULSES] =  "waveform has too many pulses",
    [-PI_TOO_MANY_CHARS] =   "waveform has too many chars",
    [-PI_NOT_SERIAL_GPIO] =  "no bit bang serial read on GPIO",
    [-PI_BAD_SERIAL_STRUC] = "bad (null) serial structure parameter",
    [-PI_BAD_SERIAL_BUF] =   "bad (null) serial buf parameter",
    [-PI_NOT_PERMITTED] =    "GPIO operation not permitted",
    [-PI_SOME_PERMITTED] =   "one or more GPIO not permitted",
    [-PI_BAD_WVSC_COMMND] =  "bad WVSC subcommand",
    [-PI_BAD_WVSM_COMMND] =  "bad WVSM subcommand",
    [-PI_BAD_WVSP_COMMND] =  "bad WVSP subcommand",
    [-PI_BAD_PULSELEN] =     "trigger pulse length not 1-100",
    [-PI_BAD_SCRIPT] =       "invalid script",
    [-PI_BAD_SCRIPT_ID] =    "unknown script id",
    [-PI_BAD_SER_OFFSET] =   "add serial data offset > 30 minutes",
    [-PI_GPIO_IN_USE] =      "GPIO already in use",
    [-PI_BAD_SERIAL_COUNT] = "must read at least a byte at a time",
    [-PI_BAD_PARAM_NUM] =    "script parameter id not 0-9",
    [-PI_DUP_TAG] =          "script has duplicate tag",
    [-PI_TOO_MANY_TAGS] =    "script has too many tags",
    [-PI_BAD_SCRIPT_CMD] =   "illegal script command",
    [-PI_BAD_VAR_NUM] =      "script variable id not 0-149",
    [-PI_NO_SCRIPT_ROOM] =   "no more room for scripts",
    [-PI_NO_MEMORY] =        "can't allocate temporary memory",
    [-PI_SOCK_READ_FAILED] = "socket read failed",
    [-PI_SOCK_WRIT_FAILED] = "socket write failed",
    [-PI_TOO_MANY_PARAM] =   "too many script parameters (> 10)",
    [-PI_NOT_HALTED] =       "DEPRECATED",
    [-PI_SCRIPT_NOT_READY] = "script initialising",
    [-PI_BAD_TAG] =          "script has unresolved tag",
    [-PI_BAD_MICS_DELAY] =   "bad MICS delay (too large)",
    [-PI_BAD_MILS_DELAY] =   "bad MILS delay (too large)",
    [-PI_BAD_WAVE_ID] =      "non existent wave id",
    [-PI_TOO_MANY_CBS] =     "No more CBs for waveform",
    [-PI_TOO_MANY_OOL] =     "No more OOL for waveform",
    [-PI_EMPTY_WAVEFORM] =   "attempt to create an empty waveform",
    [-PI_NO_WAVEFORM_ID] =   "no more waveforms",
    [-PI_I2C_OPEN_FAILED] =  "can't open I2C device",
    [-PI_SER_OPEN_FAILED] =  "can't open serial device",
    [-PI_SPI_OPEN_FAILED] =  "can't open SPI device",
    [-PI_BAD_I2C_BUS] =      "bad I2C bus",
    [-PI_BAD_I2C_ADDR] =     "bad I2C address",
    [-PI_BAD_SPI_CHANNEL] =  "bad SPI channel",
    [-PI_BAD_FLAGS] =        "bad i2c/spi/ser open flags",
    [-PI_BAD_SPI_SPEED] =    "bad SPI speed",
    [-PI_BAD_SER_DEVICE] =   "bad serial device name",
    [-PI_BAD_SER_SPEED] =    "bad serial baud rate",
    [-PI_BAD_PARAM] =        "bad i2c/spi/ser parameter",
    [-PI_I2C_WRITE_FAILED] = "i2c write failed",
    [-PI_I2C_READ_FAILED] =  "i2c read failed",
    [-PI_BAD_SPI_COUNT] =    "bad SPI count",
    [-PI_SER_WRITE_FAILED] = "ser write failed",
    [-PI_SER_READ_FAILED] =  "ser read failed",
    [-PI_SER_READ_NO_DATA] = "ser read no data available",
    [-PI_UNKNOWN_COMMAND] =  "unknown command",
    [-PI_SPI_XFER_FAILED] =  "spi xfer/read/write failed",
    [-PI_BAD_POINTER] =      "bad (NULL) pointer",
    [-PI_NO_AUX_SPI] =       "no auxiliary SPI on Pi A or B",
    [-PI_NOT_PWM_GPIO] =     "GPIO is not in use for PWM",
    [-PI_NOT_SERVO_GPIO] =   "GPIO is not in use for servo pulses",
    [-PI_NOT_HCLK_GPIO] =    "GPIO has no hardware clock",
    [-PI_NOT_HPWM_GPIO] =    "GPIO has no hardware PWM",
    [-PI_BAD_HPWM_FREQ] =    "invalid hardware PWM frequency",
    [-PI_BAD_HPWM_DUTY] =    "hardware PWM dutycycle not 0-1M",
    [-PI_BAD_HCLK_FREQ] =    "invalid hardware clock frequency",
    [-PI_BAD_HCLK_PASS] =    "need password to use hardware clock 1",
    [-PI_HPWM_ILLEGAL] =     "illegal, PWM in use for main clock",
    [-PI_BAD_DATABITS] =     "serial data bits not 1-32",
    [-PI_BAD_STOPBITS] =     "serial (half) stop bits not 2-8",
    [-PI_MSG_TOOBIG] =       "socket/pipe message too big",
    [-PI_BAD_MALLOC_MODE] =  "bad memory allocation mode",
    [-PI_TOO_MANY_SEGS] =    "too many I2C transaction segments",
    [-PI_BAD_I2C_SEG] =      "an I2C transaction segment failed",
    [-PI_BAD_SMBUS_CMD] =    "SMBus command not supported by driver",
    [-PI_NOT_I2C_GPIO] =     "no bit bang I2C in progress on GPIO",
    [-PI_BAD_I2C_WLEN] =     "bad I2C write length",
    [-PI_BAD_I2C_RLEN] =     "bad I2C read length",
    [-PI_BAD_I2C_CMD] =      "bad I2C command",
    [-PI_BAD_I2C_BAUD] =     "bad I2C baud rate, not 50-500k",
    [-PI_CHAIN_LOOP_CNT] =   "bad chain loop count",
    [-PI_BAD_CHAIN_LOOP] =   "empty chain loop",
    [-PI_CHAIN_COUNTER] =    "too many chain counters",
    [-PI_BAD_CHAIN_CMD] =    "bad chain command",
    [-PI_BAD_CHAIN_DELAY] =  "bad chain delay micros",
    [-PI_CHAIN_NESTING] =    "chain counters nested too deeply",
    [-PI_CHAIN_TOO_BIG] =    "chain is too long",
    [-PI_DEPRECATED] =       "deprecated function removed",
    [-PI_BAD_SER_INVERT] =   "bit bang serial invert not 0 or 1",
    [-PI_BAD_EDGE] =         "bad ISR edge value, not 0-2",
    [-PI_BAD_ISR_INIT] =     "bad ISR initialisation",
    [-PI_BAD_FOREVER] =      "loop forever must be last command",
    [-PI_BAD_FILTER] =       "bad filter parameter",
    [-PI_BAD_PAD] =          "bad pad number",
    [-PI_BAD_STRENGTH] =     "bad pad drive strength",
    [-PI_FIL_OPEN_FAILED] =  "file open failed",
    [-PI_BAD_FILE_MODE] =    "bad file mode",
    [-PI_BAD_FILE_FLAG] =    "bad file flag",
    [-PI_BAD_FILE_READ] =    "bad file read",
    [-PI_BAD_FILE_WRITE] =   "bad file write",
    [-PI_FILE_NOT_ROPEN] =   "file not open for read",
    [-PI_FILE_NOT_WOPEN] =   "file not open for write",
    [-PI_BAD_FILE_SEEK] =    "bad file seek",
    [-PI_NO_FILE_MATCH] =    "no files match pattern",
    [-PI_NO_FILE_ACCESS] =   "no permission to access file",
    [-PI_FILE_IS_A_DIR] =    "file is a directory",
    [-PI_BAD_SHELL_STATUS] = "bad shell return status",
    [-PI_BAD_SCRIPT_NAME] =  "bad script name",
    [-PI_BAD_SPI_BAUD] =     "bad SPI baud rate, not 50-500k",
    [-PI_NOT_SPI_GPIO] =     "no bit bang SPI in progress on GPIO",
    [-PI_BAD_EVENT_ID] =     "bad event id",
    [-PI_CMD_INTERRUPTED] =  "Used by Python",
    [-PI_NOT_ON_BCM2711] =   "not available on BCM2711",
    [-PI_ONLY_ON_BCM2711] =  "only available on BCM2711",
    [-WT_ERR_MALLOC] = "unable to allocate memory",
    
    [-WT_ERR_BAD_AUDIO_BITDEPTH] = "bad audio bitdepth",
    [-WT_ERR_BAD_AUDIO_FILTER] = "bade audio filter",
    [-WT_ERR_BAD_AUDIO_SAMPLE_RATE] = "bade audio sample rate",
    [-WT_ERR_BMS_WRITE_PROT_DISABLE_FAIL] = "failed to disable write protection",
    [-WT_ERR_BMS_WRITE_PROT_ENABLE_FAIL] = "failed to enable write protection",
    [-WT_ERR_BMS_BAD_CELL_INDEX] = "bad cell index",
    [-WT_ERR_BAD_ECG_DATA_RATE] = "bad ECG ADC data rate",
    [-WT_ERR_BAD_ECG_CHANNEL] = "bad ECG channel",
    [-WT_ERR_ECG_TIMEOUT] = "ECG timed out",
    [-WT_ERR_FPGA_N_DONE] = "fpga load did not finish",
    [-WT_ERR_IMU_BAD_PKT_SIZE] = "imu packet size <= 0",
    [-WT_ERR_IMU_UNEXPECTED_PKT_TYPE] = "received an unexpected packet type",
    [-WT_ERR_BAD_IOX_GPIO] = "bad io expander gpio",
    [-WT_ERR_BAD_IOX_MODE] = "bad io expander gpio mode",
    [-WT_ERR_RECOVERY_TIMEOUT] = "Recovery board timeout",
    [-WT_ERR_RECOVERY_BAD_CALLSIGN] = "bad callsign",
    [-WT_ERR_RECOVERY_BAD_FREQ] = "bad frequency",
    [-WT_ERR_RECOVERY_OVERSIZED_COMMENT] = "comment exceeds 40 char limit", 
    [-WT_ERR_RECOVERY_OVERSIZED_MESSAGE] = "message exceeds 67 char limit", 
    [-WT_ERR_RECOVERY_UNDERSIZED_GPS_BUFFER] = "destination gps buffer too small",
    [-WT_ERR_BMS_WRITE_PROT_DISABLE_FAIL] = "failed to disable write protection",
};

/**
 * @brief returns a string pointer corresponding to the error associated with
 * the associated whale tag error code. 
 * 
 * @param errnum 
 * @return const char* 
 */
const char *wt_strerror(WTResult errnum){
    static char __wt_errstr[512] = {};
    int device = (errnum >> 16) & 0xFFFF;
    int error = ((int16_t)(errnum & 0xFFFF));

    //construct error string
    if ( device == 0) { //no device
        if(error > 0) {
            //system error
            strncpy(__wt_errstr, strerror(errno), sizeof(__wt_errstr) - 1);
        } else if((error <= PI_INIT_FAILED) && (-error < sizeof(err_str)/sizeof(char *))) {
            //device error
            strncpy(__wt_errstr, err_str[-error], sizeof(__wt_errstr) - 1); 
        } else {
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "unkown error # %d", error);
        }
    } else if ((device < sizeof(device_name)/sizeof(char *))) { //device
        if(error > 0) {
            //system error
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "%s : %s", device_name[device], strerror(errno));
        } else if((error <= PI_INIT_FAILED) && (-error < sizeof(err_str)/sizeof(char *))) {
            //device error
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "%s : %s", device_name[device], err_str[-error]);
        } else {
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "%s : unknown error # %d", device_name[device], error);
        }
    } else { //unknown edvice
        if(error > 0) {
            //system error
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "%s : %s", device_name[device], strerror(errno));
        } else if((error <= PI_INIT_FAILED) && (-error < sizeof(err_str)/sizeof(char *))) {
            //device error
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "unknown_device # %d : %s", device, err_str[-error]); 
        } else {
            snprintf(__wt_errstr, sizeof(__wt_errstr) - 1, "unknown_device # %d : unkown error # %d", device, error);
        } 
    }
    return __wt_errstr;
}


const char *wt_strerror_device_name(WTResult errnum) {
    int device = (errnum >> 16) & 0xFFFF;
    return device_name[device];
}
