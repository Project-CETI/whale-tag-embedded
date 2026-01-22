//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "recovery.h"

#include "cetiTag.h"
#include "device/iox.h"
#include "launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/config.h"
#include "utils/logging.h"
#include "utils/memory.h"
#include "utils/thread_error.h"
#include "utils/timing.h"

#include <ctype.h>
#include <fcntl.h>
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
#include <semaphore.h>
#include <stdbool.h>  //for bool
#include <string.h>   // for memset() and other string functions
#include <sys/mman.h> // for munmap()
#include <unistd.h>   // for usleep()

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
/* MACRO DEFINITIONS *********************************************************/
#define RECOVERY_UART_TIMEOUT_US 500000

/* TYPE DEFINITIONS **********************************************************/

/* GLOBAL/STATIC VARIABLES ******************************************************/
#define RECOVERY_WDT_ENABLED 0
#define RECOVERY_WDT_TRIGGER_TIME_MIN 10

int g_recovery_rx_thread_is_running = 0;
static FILE *recovery_data_file = NULL;
static char recovery_data_file_notes[256] = "";
static const char *recovery_data_file_headers[] = {
    "GPS",
};
static const int num_recovery_data_file_headers = sizeof(recovery_data_file_headers) / sizeof(*recovery_data_file_headers);
static int recovery_fd = PI_INIT_FAILED;
static int64_t s_recovery_hardware_start_time_us = -1;

typedef enum {
    REC_STATE_WAIT,
    REC_STATE_APRS,
    REC_STATE_BOOTLOADER,
    REC_STATE_SHUTDOWN,
} RecoveryBoardState;

static struct {
    RecoveryBoardState state;
} s_recovery_board_model = {
    .state = REC_STATE_APRS,
};

struct {
    struct {
        uint8_t valid;
        float value;
    } freq_Mhz;
    struct {
        struct {
            uint8_t valid;
            uint8_t value[7];
        } callsign;
        struct {
            uint8_t valid;
            uint8_t value;
        } ssid;
    } callsign;
    struct {
        struct {
            uint8_t valid;
            uint8_t value[7];
        } callsign;
        struct {
            uint8_t valid;
            uint8_t value;
        } ssid;
    } recipient;
    uint8_t pong;
} recovery_board = {};

static CetiRecoverySample *shm_nmea_sentence;
static sem_t *sem_nmea_sentence_ready;

/* FUCNTION DEFINITIONS ******************************************************/

/**
 * @brief Cuts power to recovery board
 *
 * @return WTResult
 */
WTResult wt_recovery_off(void) {
    return iox_write_pin(IOX_GPIO_3V3_RF_EN, 0);
}

/**
 * @brief Applies power to recovery board
 *
 * @return WTResult
 */
WTResult wt_recovery_on(void) {
    return iox_write_pin(IOX_GPIO_3V3_RF_EN, 1);
    return WT_OK;
}

/* STATIC
 * serial write method with guard case for uninitialized recovery board.
 */
static WTResult __recovery_write(const void *pkt, size_t size) {
#ifdef DEBUG
    char packet_debug[512];
    int len = 0;
    for (int i = 0; i < size; i++) {
        len += sprintf(&packet_debug[len], "%02x ", ((uint8_t *)pkt)[i]);
    }
    CETI_LOG("Pi->Rec: {%s}\n", packet_debug);
#endif
    PI_TRY(WT_DEV_RECOVERY, serWrite(recovery_fd, (char *)pkt, size));
    return WT_OK;
}

static WTResult __recovery_write_packet(const RecoveryPacket *pkt) {
    return __recovery_write(pkt, sizeof(RecPktHeader) + pkt->header.length);
}

static int __recovery_query(RecoveryCommand query_command, uint8_t *pValid) {
    // assert(pVail != NULL)
    if (pValid == NULL) {
        CETI_ERR("No validation pointer provided.");
        return 0;
    }

    // send query packet
    RecPktHeader q_pkt = REC_EMPTY_PKT(query_command);
    WTResult write_result = __recovery_write_packet((const RecoveryPacket *)&q_pkt);
    if (write_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to send query to recovery board: %s", wt_strerror_r(write_result, err_str, sizeof(err_str)));
        return 0;
    }

    // get start time
    uint64_t start_time_us = get_monotonic_time_us();

    // invalidate old value
    *pValid = 0;

    // wait for pong message or timeout
    while (!*pValid && (get_monotonic_time_us() - start_time_us < RECOVERY_UART_TIMEOUT_US)) {
        ;
    }

    return *pValid;
}

/* Get the next received communication packet from the recovery board.
 * Arguments:
 *     RecPkt *packet   : ptr to destination packet union.
 *     time_t timout_us : timeout in microsecond, 0 = never.
 */
static WTResult __recovery_get_packet(RecoveryPacket *packet, bool (*term_condition)(void)) {
    bool header_complete = 0;
    int expected_bytes = sizeof(RecPktHeader);

    packet->header.key = 0;
    do {
        int bytes_avail = PI_TRY(WT_DEV_RECOVERY, serDataAvailable(recovery_fd));

        if (bytes_avail < expected_bytes) { // not enough bytes available
            usleep(1000);                   // wait a bit
            continue;                       // try again
        }

        // scan bytes until start byte (skip if start byte already found)
        while ((bytes_avail) && (packet->header.key != RECOVERY_PACKET_KEY_VALUE)) {
            PI_TRY(WT_DEV_RECOVERY, serRead(recovery_fd, (char *)&(packet->header.key), 1));
            bytes_avail--;
        }

        if (packet->header.key != RECOVERY_PACKET_KEY_VALUE) {
            // packet key not
            continue;
        }

        if (!header_complete) {
            // read rest of header
            expected_bytes = sizeof(RecPktHeader) - 1;
            if (bytes_avail < expected_bytes) // not enough bytes to complete header
                continue;                     // get more bytes

            PI_TRY(WT_DEV_RECOVERY, serRead(recovery_fd, (char *)&(packet->header.type), expected_bytes));
            bytes_avail -= expected_bytes;
            header_complete = 1;
        }

        // read message if any
        if (packet->header.length != 0) {
            expected_bytes = packet->header.length;
            if (bytes_avail < expected_bytes) { // not enough bytes to complete msg
                continue;                       // get more bytes
            }

            PI_TRY(WT_DEV_RECOVERY, serRead(recovery_fd, (char *)packet->data.raw, expected_bytes));
        }

        return WT_OK; // Success !!!
    } while ((term_condition == NULL) || !term_condition());

    // Timeout
    return WT_RESULT(WT_DEV_RECOVERY, WT_ERR_RECOVERY_TIMEOUT); // this may not be an error, data may just not be available
}

// NOTE: __ping* used internal to verify recovery board connection prior to recovery_rx_thread running
static uint64_t __ping_timeout;
static void __ping_start_timeout(void) { __ping_timeout = get_monotonic_time_us(); }
static bool __ping_check_timeout(void) { return get_monotonic_time_us() - __ping_timeout > RECOVERY_UART_TIMEOUT_US; }
static bool __ping(void) {
    RecPktHeader q_pkt = REC_EMPTY_PKT(REC_CMD_PING);
    RecoveryPacket r_pkt = {.header.type = -1};
    WTResult write_result = __recovery_write_packet((const RecoveryPacket *)&q_pkt);
    if (write_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to send ping to recovery board: %s", wt_strerror_r(write_result, err_str, sizeof(err_str)));
        return 0;
    }

    // try getting packet
    __ping_start_timeout();
    while (!__ping_check_timeout()) {
        WTResult result = __recovery_get_packet(&r_pkt, __ping_check_timeout);
        if (result != WT_OK) {
            return 0; // device error OR timeout
        }

        if (r_pkt.header.type == REC_CMD_PONG) {
            return 1; // success
        }
        // incorrect packet type
    }
    return 0;
}

/// @brief Initializes pi hardware to be able to control the recovery board
/// @param
/// @return WTResult
WTResult wt_recovery_init(void) {
    // Initialize iox pins.
    WT_TRY(iox_init());

    // Set 3v3_RF enable, keep recovery off.
    WT_TRY(iox_set_mode(IOX_GPIO_3V3_RF_EN, IOX_MODE_OUTPUT));
    WT_TRY(wt_recovery_off());
    usleep(50000);

    // Set boot0 pin output and pull low.
    WT_TRY(iox_set_mode(IOX_GPIO_BOOT0, IOX_MODE_OUTPUT));
    WT_TRY(iox_write_pin(IOX_GPIO_BOOT0, 0));
    usleep(5000);
    // Open serial communication
    recovery_fd = PI_TRY(WT_DEV_RECOVERY, serOpen("/dev/serial0", 115200, 0), wt_recovery_off());
    WT_TRY(wt_recovery_on());
    s_recovery_hardware_start_time_us = get_monotonic_time_us();
    return WT_OK;
}

/**
 * @brief Restarts recovery board by toggling power
 *
 * @return WTResult
 */
WTResult wt_recovery_restart(void) {
    WT_TRY(wt_recovery_off());
    usleep(1000);
    return wt_recovery_on();
}

/**
 * @brief Puts recovery board into bootloader state by pulling boot0 low while
 * restarting the board.
 *
 * @return WTResult
 */
WTResult wt_recovery_enter_bootloader(void) {
    // pull boot0 low
    WT_TRY(iox_write_pin(IOX_GPIO_BOOT0, 0));
    WT_TRY(wt_recovery_off());

    // pull boot0 high
    WT_TRY(iox_write_pin(IOX_GPIO_BOOT0, 1));
    usleep(1000);
    WT_TRY(wt_recovery_on());

    return WT_OK;
}

//-----------------------------------------------------------------------------
// APRS
//-----------------------------------------------------------------------------
#if RECOVERY_BOARD_TYPE_APRS == RECOVERY_BOARD_TYPE
static int __recovery_get_aprs_callsign(char buffer[static 7]) {
    if (!__recovery_query(REC_CMD_QUERY_APRS_CALLSIGN, &recovery_board.callsign.callsign.valid)) {
        return -1;
    }

    memcpy(buffer, recovery_board.callsign.callsign.value, 7);
    return 0;
}

static int __recovery_get_aprs_ssid(uint8_t *ssid) {
    if (!__recovery_query(REC_CMD_QUERY_APRS_SSID, &recovery_board.callsign.ssid.valid)) {
        return -1;
    }

    *ssid = recovery_board.callsign.ssid.value;
    return 0;
}

int recovery_get_aprs_callsign(APRSCallsign *callsign) {
    int result = 0;
    result |= __recovery_get_aprs_callsign(callsign->callsign);
    result |= __recovery_get_aprs_ssid(&callsign->ssid);
    return result;
}

int recovery_get_aprs_freq_mhz(float *p_freq_MHz) {
    if (!__recovery_query(REC_CMD_QUERY_APRS_FREQ, &recovery_board.freq_Mhz.valid)) {
        return -1;
    }

    *p_freq_MHz = recovery_board.freq_Mhz.value;
    return 0;
}

static int __recovery_set_aprs_callsign(const char *callsign) {
    size_t callsign_len = strlen(callsign);
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_CALLSIGN,
            .length = (uint8_t)callsign_len,
        },
    };

    if (callsign_len > 6) {
        CETI_ERR("Callsign \"%s\" is too long", callsign);
        return -3;
    }

    memcpy(pkt.data.raw, callsign, callsign_len);
    return __recovery_write_packet(&pkt);
}

static int __recovery_set_aprs_ssid(uint8_t ssid) {
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_SSID,
            .length = sizeof(uint8_t),
        },
        .data.u8 = ssid,
    };

    if (ssid > 15) {
        CETI_ERR("APRS SSID (%d) outside allowable range (0-15)", ssid);
        return -4;
    }

    return __recovery_write_packet(&pkt);
}

int recovery_set_aprs_callsign(const APRSCallsign *callsign) {
    int result;
    result = __recovery_set_aprs_callsign(callsign->callsign);
    result |= __recovery_set_aprs_ssid(callsign->ssid);
    return result;
}

int recovery_set_aprs_freq_mhz(float freq_MHz) {
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_FREQ,
            .length = sizeof(float)},
        .data.f32 = freq_MHz,
    };

    return __recovery_write_packet(&pkt);
}

static int __recovery_set_aprs_rx_callsign(const char *callsign) {
    size_t callsign_len = strlen(callsign);
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_MSG_RCPT_CALLSIGN,
            .length = (uint8_t)callsign_len,
        },
    };
    memcpy(pkt.data.raw, callsign, callsign_len);

    if (callsign_len > 6) {
        CETI_ERR("Callsign \"%s\" is too long", callsign);
        return -3;
    }

    return __recovery_write_packet(&pkt);
}

static int __recovery_set_aprs_rx_ssid(uint8_t ssid) {
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_MSG_RCPT_SSID,
            .length = sizeof(uint8_t),
        },
        .data.u8 = ssid,
    };

    if (ssid > 15) {
        CETI_ERR("APRS SSID (%d) outside allowable range (0-15)", ssid);
        return -4;
    }

    return __recovery_write_packet(&pkt);
}

int recovery_set_aprs_message_recipient(const APRSCallsign *callsign) {
    int result;
    result = __recovery_set_aprs_rx_callsign(callsign->callsign);
    result |= __recovery_set_aprs_rx_ssid(callsign->ssid);
    return result;
}

int recovery_set_aprs_comment(const char *message) {
    size_t len = strlen(message);
    if (len > 40) {
        len = 40;
    }

    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_COMMENT,
            .length = len,
        },
    };
    memcpy(pkt.data.raw, message, len);
    return __recovery_write_packet(&pkt);
}

int recovery_set_aprs_power_level(RecoveryPowerLevel power_level) {
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_VHF_POWER_LEVEL,
            .length = sizeof(uint8_t)},
        .data.u8 = power_level,
    };
    return __recovery_write_packet(&pkt);
}
#endif // RECOVERY_BOARD_TYPE_APRS

//-----------------------------------------------------------------------------
// Argos
//-----------------------------------------------------------------------------
#if RECOVERY_BOARD_TYPE_ARGOS == RECOVERY_BOARD_TYPE
struct {
    uint8_t valid;
    char value[33];
} s_secret_key = {.valid = 0};
struct {
    uint8_t valid;
    char value[9];
} s_address = {.valid = 0};
struct {
    uint8_t valid;
    char value[7];
} s_id = {.valid = 0};
struct {
    uint8_t valid;
    uint8_t value;
} s_modulation_scheme = {.valid = 0};

int recovery_get_argos_address(char address[static 9]) {
    if (!__recovery_query(REC_CMD_QUERY_ARGOS_ADDR, &s_address.valid)) {
        return -1;
    }
    memcpy(address, s_address.value, 8);
    return 0;
}

int recovery_get_argos_id(char address[static 7]) {
    if (!__recovery_query(REC_CMD_QUERY_ARGOS_ID, &s_id.valid)) {
        return -1;
    }
    memcpy(address, s_id.value, 6);
    return 0;
}

int recovery_get_argos_modulation(RecoveryArgoModulation *mod_scheme) {
    if (!__recovery_query(REC_CMD_QUERY_ARGOS_MODULATION, &s_modulation_scheme.valid)) {
        return -1;
    }
    if (mod_scheme != NULL) {
        *mod_scheme = s_modulation_scheme.value;
    }
    return 0;
}

int recovery_get_argos_secret_key(char secret_key[static 32]) {
    if (!__recovery_query(REC_CMD_QUERY_ARGOS_SECKEY, &s_secret_key.valid)) {
        return -1;
    }
    memcpy(secret_key, s_secret_key.value, 32);
    return 0;
}

int recovery_set_argos_address(const char *address, size_t address_len) {
    if (8 != address_len) {
        return -1;
    }
    for (int i = 0; i < 8; i++) {
        if (!isxdigit(address[i])) {
            return -1;
        }
    }

    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_ARGOS_ADDR,
            .length = 8,
        }};
    memcpy(pkt.data.raw, address, 8);
    return __recovery_write_packet(&pkt);
    return 0;
}

int recovery_set_argos_id(const char *id, size_t id_len) {
    for (int i = 0; i < id_len; i++) {
        if (!isdigit(id[i])) {
            return -1;
        }
    }

    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_ARGOS_ID,
            .length = id_len,
        }};
    memcpy(pkt.data.raw, id, id_len);
    return __recovery_write_packet(&pkt);
    return 0;
}

int recovery_set_argos_modulation(RecoveryArgoModulation mod_scheme) {
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_ARGOS_MODULATION,
            .length = 1,
        },
        .data.u8 = (uint8_t)mod_scheme,
    };
    return __recovery_write_packet(&pkt);
}

int recovery_set_argos_secret_key(const char *secret_key, size_t secret_key_len) {
    if (32 != secret_key_len) {
        return -1;
    }
    for (int i = 0; i < 32; i++) {
        if (!isxdigit(secret_key[i])) {
            return -1;
        }
    }
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_ARGOS_SECKEY,
            .length = 32,
        }};
    memcpy(pkt.data.raw, secret_key, 32);
    return __recovery_write_packet(&pkt);
}
#endif // RECOVERY_BOARD_TYPE_ARGOS

//-----------------------------------------------------------------------------
// General commands
//-----------------------------------------------------------------------------
int recovery_set_critical_voltage(float voltage) {
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_CRITICAL_VOLTAGE,
            .length = sizeof(float)},
        .data.f32 = voltage,
    };
    return __recovery_write_packet(&pkt);
}

#if RECOVERY_BOARD_TYPE_APRS == RECOVERY_BOARD_TYPE
#define RECOVERY_MOARD_MAX_MSG_LENGTH 67
#elif RECOVERY_BOARD_TYPE_ARGOS == RECOVERY_BOARD_TYPE
#define RECOVERY_MOARD_MAX_MSG_LENGTH 24
#endif
int recovery_message(const char *message) {
    size_t message_len = strlen(message);
    if (message_len > RECOVERY_MOARD_MAX_MSG_LENGTH) {
        return -1;
    }

    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_MESSAGE,
            .length = message_len,
        }};
    memcpy(pkt.data.raw, message, message_len);
    return __recovery_write_packet(&pkt);
}

int recovery_ping(void) {
    return __recovery_query(REC_CMD_PING, &recovery_board.pong);
}

//-----------------------------------------------------------------------------
// On/Off
//-----------------------------------------------------------------------------
WTResult recovery_sync_time(void) {
    // get system time as date time
    time_t current_time;
    time(&current_time);
    struct tm *now_tm = gmtime(&current_time);

    // parse system time
    RecoveryPacket pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_SET_RTC_TIME_OF_DAY,
            .length = 6,
        },
    };

    pkt.data.raw[0] = (uint8_t)(now_tm->tm_year - 100); // year since 2000
    pkt.data.raw[1] = (uint8_t)(now_tm->tm_mon + 1);
    pkt.data.raw[2] = (uint8_t)now_tm->tm_mday;
    pkt.data.raw[3] = (uint8_t)now_tm->tm_hour;
    pkt.data.raw[4] = (uint8_t)now_tm->tm_min;
    pkt.data.raw[5] = (uint8_t)now_tm->tm_sec;

    // send systemtime to recovery board
    return __recovery_write_packet(&pkt);
}

// // sets recovery board into "arps" state
int recovery_wake(void) {
    RecPktHeader start_pkt = REC_EMPTY_PKT(REC_CMD_START);
    WTResult tx_result = __recovery_write(&start_pkt, sizeof(start_pkt));
    if (tx_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to wake board: %s", wt_strerror_r(tx_result, err_str, sizeof(err_str)));
        return -1;
    }
    s_recovery_board_model.state = REC_STATE_APRS;
    return 0;
}

int recovery_sleep(void) {
    RecPktHeader start_pkt = REC_EMPTY_PKT(REC_CMD_STOP);
    WTResult tx_result = __recovery_write(&start_pkt, sizeof(start_pkt));
    if (tx_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to put board to sleep: %s", wt_strerror_r(tx_result, err_str, sizeof(err_str)));
        return -1;
    }
    s_recovery_board_model.state = REC_STATE_APRS;
    return 0;
}

int recovery_gps_only(void) {
    RecPktHeader start_pkt = REC_EMPTY_PKT(REC_CMD_COLLECT_ONLY);
    WTResult tx_result = __recovery_write(&start_pkt, sizeof(start_pkt));
    if (tx_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to put board to gps only: %s", wt_strerror_r(tx_result, err_str, sizeof(err_str)));
        return -1;
    }
    s_recovery_board_model.state = REC_STATE_APRS;
    return 0;
}

/**
 * @brief Applys power to recovery board.
 *
 * @return 0 on success, -1 on failure
 */
int recovery_on(void) {
    WTResult hw_result = wt_recovery_on();
    if (hw_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed power recovery board: %s", wt_strerror_r(hw_result, err_str, sizeof(err_str)));
        return -1;
    }
    return 0;
};

/**
 * @brief Cuts power to recovery board.
 *
 * @return 0 on success, -1 on failure
 */
int recovery_off(void) {
    WTResult hw_result = wt_recovery_off();
    if (hw_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed cut power to recovery board: %s", wt_strerror_r(hw_result, err_str, sizeof(err_str)));
        return -1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
int recovery_thread_init(TagConfig *pConfig) {
    char err_str[512];
    int t_result = THREAD_OK;
    WTResult hw_result = WT_OK;
    // test connection
    while (!__ping()) {
        // check for timeout timeout occured
        if (s_recovery_hardware_start_time_us >= 10 * 1000000) {
            return WT_RESULT(WT_DEV_RECOVERY, WT_ERR_RECOVERY_TIMEOUT);
        }
    }

#if RECOVERY_BOARD_TYPE_APRS == RECOVERY_BOARD_TYPE
    if (hw_result == WT_OK)
        hw_result = recovery_set_aprs_freq_mhz(pConfig->recovery.freq_MHz);
    if (hw_result == WT_OK)
        hw_result = recovery_set_aprs_callsign(&pConfig->recovery.callsign);
    if (hw_result == WT_OK)
        hw_result = recovery_set_aprs_message_recipient(&pConfig->recovery.recipient);
#elif RECOVERY_BOARD_TYPE_ARGOS == RECOVERY_BOARD_TYPE
    if ((WT_OK == hw_result) && timing_has_syncronized_to_ntp())
        hw_result = recovery_sync_time();
#endif
    if (hw_result == WT_OK)
        hw_result = recovery_set_critical_voltage(2.0 * pConfig->critical_voltage_v);
    if (hw_result != WT_OK) {
        CETI_ERR("Failed to initalize recovery board hardware: %s", wt_strerror_r(hw_result, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_HW;
    }
    if (!__ping()) {
        t_result |= THREAD_ERR_HW;
    }

    s_recovery_board_model.state = REC_STATE_APRS;

    // create shared memory region for recovery board
    shm_nmea_sentence = create_shared_memory_region(RECOVERY_SHM_NAME, sizeof(CetiRecoverySample));
    if (shm_nmea_sentence == NULL) {
        CETI_ERR("Failed to create shared memory region");
        t_result |= THREAD_ERR_SHM_FAILED;
    }
    // setup semaphores
    sem_nmea_sentence_ready = sem_open(RECOVERY_SEM_NAME, O_CREAT, 0644, 0);
    if (sem_nmea_sentence_ready == SEM_FAILED) {
        CETI_ERR("Failed to create recovery semaphore");
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    // Open an output file to write data.
    if (init_data_file(RECOVERY_DATA_FILEPATH,
                       recovery_data_file_headers, num_recovery_data_file_headers,
                       recovery_data_file_notes, "init_data_file()") < 0) {
        CETI_LOG("Failed to initialize recovery board thread");
        t_result |= THREAD_ERR_DATA_FILE_FAILED;
    }

    CETI_LOG("Successfully initialized recovery board thread");
    return t_result;
}

static void __recovery_sample_to_csv(CetiRecoverySample *pSample) {
    recovery_data_file = fopen(RECOVERY_DATA_FILEPATH, "at");
    fprintf(recovery_data_file, "%ld, %d, %s, %s\n", pSample->sys_time_us, pSample->rtc_time_s, recovery_data_file_notes, pSample->nmea_sentence);
    fclose(recovery_data_file);
    recovery_data_file_notes[0] = '\0'; // clear notes
}

static bool __recovery_rx_thread_should_exit() {
    return g_stopAcquisition;
}

void *recovery_rx_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_recovery_rx_thread_tid = gettid();

    // ping board

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    g_recovery_rx_thread_is_running = 1;

    CETI_LOG("Starting loop to periodically acquire data");
    while (!__recovery_rx_thread_should_exit()) {
        RecoveryPacket pkt;
        // wait for recovery board packet
        WTResult result = __recovery_get_packet(&pkt, __recovery_rx_thread_should_exit);
        if (result == WT_RESULT(WT_DEV_RECOVERY, WT_ERR_RECOVERY_TIMEOUT))
            break;             // normal termination condition reacted
        if (result != WT_OK) { // actual error occured
            char err_str[512];
            CETI_ERR("%s", wt_strerror_r(result, err_str, sizeof(err_str))); // print error
            // TODO actually handle error.
            continue;
        }

        // handle return packet based on type
        switch (pkt.header.type) {
            case REC_CMD_NMEA_PACKET:
                // TODO check message length
                shm_nmea_sentence->sys_time_us = get_global_time_us();
                shm_nmea_sentence->rtc_time_s = getRtcCount();
                memcpy(shm_nmea_sentence->nmea_sentence, pkt.data.raw, pkt.header.length);
                shm_nmea_sentence->nmea_sentence[pkt.header.length] = '\0';
                // truncate carriage returns and new lines
                while ((shm_nmea_sentence->nmea_sentence[pkt.header.length] == '\0') || (shm_nmea_sentence->nmea_sentence[pkt.header.length] == '\r') || (shm_nmea_sentence->nmea_sentence[pkt.header.length] == '\n')) {
                    shm_nmea_sentence->nmea_sentence[pkt.header.length] = '\0';
                    pkt.header.length--;
                }
                sem_post(sem_nmea_sentence_ready);

                // TODO: buffer write
                if (!g_stopLogging) {
                    __recovery_sample_to_csv(shm_nmea_sentence);
                }
                break;

            case REC_CMD_PONG:
                recovery_board.pong = 1;
                break;
#if RECOVERY_BOARD_TYPE_APRS == RECOVERY_BOARD_TYPE
            case REC_CMD_CONFIG_APRS_CALLSIGN:
                if (pkt.header.length > 6) {
                    CETI_WARN("Received APRS callsign that is too long. Ignoring.");
                    break;
                }

                memcpy(recovery_board.callsign.callsign.value, pkt.data.raw, pkt.header.length);
                recovery_board.callsign.callsign.value[pkt.header.length] = 0;
                recovery_board.callsign.callsign.valid = 1;
                break;

            case REC_CMD_CONFIG_APRS_SSID:
                if (pkt.header.length != 1) {
                    CETI_WARN("Received APRS ssid packet that is an incorrect size. Ignoring.");
                    break;
                }
                if (pkt.data.u8 > 15) {
                    CETI_WARN("Received APRS ssid packet that is too large. Ignoring.");
                    break;
                }
                recovery_board.callsign.ssid.value = pkt.data.u8;
                recovery_board.callsign.ssid.valid = 1;
                break;

            case REC_CMD_CONFIG_APRS_FREQ:
                if (pkt.header.length != sizeof(float)) {
                    CETI_WARN("Received APRS frequency packet that is an incorrect size. Ignoring.");
                    break;
                }
                // TODO check frequency range
                recovery_board.freq_Mhz.value = pkt.data.f32;
                recovery_board.freq_Mhz.valid = 1;
                break;

            case REC_CMD_CONFIG_APRS_MSG_RCPT_CALLSIGN:
                if (pkt.header.length > 6) {
                    CETI_WARN("Received APRS recipient callsign that is too long. Ignoring.");
                    break;
                }

                memcpy(recovery_board.recipient.callsign.value, pkt.data.raw, pkt.header.length);
                recovery_board.recipient.callsign.value[pkt.header.length] = 0;
                recovery_board.recipient.callsign.valid = 1;
                break;

            case REC_CMD_CONFIG_APRS_MSG_RCPT_SSID:
                if (pkt.header.length != 1) {
                    CETI_WARN("Received APRS recipient ssid packet that is an incorrect size. Ignoring.");
                    break;
                }
                if (pkt.data.u8 > 15) {
                    CETI_WARN("Received APRS recipient ssid packet that is too large. Ignoring.");
                    break;
                }
                recovery_board.recipient.ssid.value = pkt.data.u8;
                recovery_board.recipient.ssid.valid = 1;
                break;
#endif // RECOVERY_BOARD_TYPE_APRS == RECOVERY_BOARD_TYPE
#if RECOVERY_BOARD_TYPE_ARGOS == RECOVERY_BOARD_TYPE
            case REC_CMD_CONFIG_ARGOS_ADDR:
                if (pkt.header.length != 8) {
                    CETI_WARN("Received ARGOS MAC address packet that is an incorrect size. Ignoring.");
                    break;
                }
                memcpy(s_address.value, pkt.data.raw, 8);
                s_address.valid = 1;
                break;

            case REC_CMD_CONFIG_ARGOS_ID:
                if (pkt.header.length != 6) {
                    CETI_WARN("Received ARGOS ID packet that is an incorrect size. Ignoring.");
                    break;
                }
                memcpy(s_id.value, pkt.data.raw, 6);
                s_id.valid = 1;
                break;

            case REC_CMD_CONFIG_ARGOS_MODULATION:
                if (pkt.header.length != 1) {
                    CETI_WARN("Received ARGOS ID packet that is an incorrect size. Ignoring.");
                    break;
                }
                s_modulation_scheme.value = pkt.data.u8;
                s_modulation_scheme.valid = 1;
                break;

            case REC_CMD_CONFIG_ARGOS_SECKEY:
                if (pkt.header.length != 32) {
                    CETI_WARN("Received ARGOS secret key packet that is an incorrect size. Ignoring.");
                    break;
                }
                memcpy(s_secret_key.value, pkt.data.raw, 32);
                s_secret_key.valid = 1;
                break;
#endif // RECOVERY_BOARD_TYPE_ARGOS == RECOVERY_BOARD_TYPE

            default: // unknown packet type
                CETI_LOG("Received packet type 0x%02X", pkt.header.type);
                break;
        }
    }
    sem_close(sem_nmea_sentence_ready);
    sem_unlink(RECOVERY_SEM_NAME);
    munmap(shm_nmea_sentence, sizeof(CetiRecoverySample));
    shm_unlink(RECOVERY_SHM_NAME);
    g_recovery_rx_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
