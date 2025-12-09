#ifndef LIB_CET_RECOVERY_H
#define LIB_CET_RECOVERY_H

#include <stdint.h>

#define RECOVERY_PACKET_KEY_VALUE '$'

typedef enum recovery_commands_e {
    /* Set recovery state*/
    REC_CMD_START            = 0x01, //pi --> rec: sets rec into active state (GPS logging + Argos transmissions)
    REC_CMD_STOP             = 0x02, //pi --> rec: sets rec into inactive state
    REC_CMD_COLLECT_ONLY     = 0x03, //pi --> rec: sets rec into rx gps state (just GPS logging)
    REC_CMD_PROGRAM_ARRIBADA = 0x04,
    // free: 0x05 - 0x0F

    /* recovery packet */
    REC_CMD_NMEA_PACKET   = 0x10, //rec --> pi: raw gps packet
    REC_CMD_MESSAGE       = 0x11,
    REC_CMD_PING          = 0x12,
    REC_CMD_PONG          = 0x13,
    // free: 0x14 - 0x1F

    REC_CMD_CONFIG_CRITICAL_VOLTAGE       = 0x20,
    /* APRS configuration */
    REC_CMD_CONFIG_APRS_VHF_POWER_LEVEL   = 0x21,
    REC_CMD_CONFIG_APRS_FREQ              = 0x22,
    REC_CMD_CONFIG_APRS_CALLSIGN          = 0x23,
    REC_CMD_CONFIG_APRS_COMMENT           = 0x24,
    REC_CMD_CONFIG_APRS_SSID              = 0x25,
    REC_CMD_CONFIG_APRS_MSG_RCPT_CALLSIGN = 0x26,
    REC_CMD_CONFIG_APRS_MSG_RCPT_SSID     = 0x27,
    REC_CMD_CONFIG_APRS_HOSTNAME          = 0x28,

    /* Arribada/argos configuration */
    REC_CMD_CONFIG_ARGOS_ID         = 0x29,
    REC_CMD_CONFIG_ARGOS_ADDR       = 0x2A,
    REC_CMD_CONFIG_ARGOS_SECKEY     = 0x2B,
    REC_CMD_CONFIG_ARGOS_MODULATION = 0x2C,
    // free: 0x2D - 0x2F
    
    /* RTC Config*/
    REC_CMD_SET_RTC_TIME_OF_DAY = 0x30, 
    /*  uint8_t data[3] = {
            year(0..99), month (1..12), day(1..31), 
            hour(0..23), minute(0..59), second(0..59)
        }; 
    */
    // free: 0x31 - 0x3F


    /* Arribada/argos query */
    REC_CMD_QUERY_STATE                  = 0x40,
    // free: 0x41 - 0x3F

    /* recovery query */
    REC_CMD_QUERY_CRITICAL_VOLTAGE       = 0x60,
    REC_CMD_QUERY_APRS_VHF_POWER_LEVEL   = 0x61,
    REC_CMD_QUERY_APRS_FREQ              = 0x62,
    REC_CMD_QUERY_APRS_CALLSIGN          = 0x63,
    REC_CMD_QUERY_APRS_COMMENT           = 0x64,
    REC_CMD_QUERY_APRS_SSID              = 0x65,
    REC_CMD_QUERY_APRS_MSG_RCPT_CALLSIGN = 0x66,
    REC_CMD_QUERY_APRS_MSG_RCPT_SSID     = 0x67,
    REC_CMD_QUERY_APRS_HOSTNAME          = 0x68,
	REC_CMD_QUERY_ARGOS_ID               = 0x69,
	REC_CMD_QUERY_ARGOS_ADDR             = 0x6A,
	REC_CMD_QUERY_ARGOS_SECKEY           = 0x6B,
	REC_CMD_QUERY_ARGOS_MODULATION       = 0x6C,
    // free: 0x6D - 0xFF
} RecoverCommand;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    uint8_t key;    // $
    uint8_t type;   // RecoverCommand
    uint8_t length; // packet_length
    uint8_t __res;  // currently unused. (ensures word alignment of message). May be used for msg CRC  or other error correction in the future
} RecPktHeader;

typedef RecPktHeader RecNullPkt;
#define REC_EMPTY_PKT(cmd) \
    (RecNullPkt) { .key = RECOVERY_PACKET_KEY_VALUE, .type = cmd, .length = 0 }

typedef struct
    __attribute__((__packed__, scalar_storage_order("little-endian"))) {
    RecPktHeader header;
    union {
        uint8_t raw[256];
        uint8_t u8;
        float f32;
        char string[256];
    } data;
} RecoveryPacket;

#endif // LIB_CET_RECOVERY_H
