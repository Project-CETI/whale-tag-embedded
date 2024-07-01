//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "recovery.h"
#include "hal.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
/* MACRO DEFINITIONS *********************************************************/
#define RECOVERY_PACKET_KEY_VALUE '$'
#define RECOVERY_UART_TIMEOUT_US 50000

#define RECOVERY_WDT_ENABLED 0
#define RECOVERY_WDT_TRIGGER_TIME_MIN 10 
    

/* TYPE DEFINITIONS **********************************************************/
typedef enum recovery_commands_e {
    /* Set recovery state*/
    REC_CMD_START        = 0x01, //pi --> rec: sets rec into recovery state
    REC_CMD_STOP         = 0x02, //pi --> rec: sets rec into waiting state
    REC_CMD_COLLECT_ONLY = 0x03, //pi --> rec: sets rec into rx gps state
    REC_CMD_CRITICAL     = 0x04, //pi --> rec: sets rec into critical state

    /* recovery packet */
    REC_CMD_GPS_PACKET   = 0x10, // rec --> pi: raw gps packet
    REC_CMD_APRS_MESSAGE,        // pi --> rec: addressee and message for tx on APRS


    /* recovery configuration */
    REC_CMD_CONFIG_CRITICAL_VOLTAGE = 0x20,
    REC_CMD_CONFIG_VHF_POWER_LEVEL,  // 0x21,
    REC_CMD_CONFIG_APRS_FREQ,        // 0x22,
    REC_CMD_CONFIG_APRS_CALLSIGN,   // 0x23,
    REC_CMD_CONFIG_APRS_COMMENT,     // 0x24,
    REC_CMD_CONFIG_APRS_SSID,
    REC_CMD_CONFIG_MSG_RCPT_CALLSIGN,
    REC_CMD_CONFIG_MSG_RCPT_SSID,
    REC_CMD_CONFIG_HOSTNAME,

    /* recovery query */
    REC_CMD_QUERY_STATE             = 0x40,

    REC_CMD_QUERY_CRITICAL_VOLTAGE  = 0x60,
    REC_CMD_QUERY_VHF_POWER_LEVEL,  // 0x61,
    REC_CMD_QUERY_APRS_FREQ,        // 0x62,
    REC_CMD_QUERY_APRS_CALLSIGN,   // 0x63,
    REC_CMD_QUERY_APRS_MESSAGE,     // 0x64,
    REC_CMD_QUERY_APRS_SSID,
}RecoverCommand;

typedef enum __GPS_MESSAGE_TYPES {
	GPS_SIM = 0, //simulated GPS message
	GPS_GLL = 1,
	GPS_GGA = 2,
	GPS_RMC = 3,
	GPS_NUM_MSG_TYPES //Should always be the last element in the enum. Represents the number of message types. If you need to add a new type, put it before this element.
}GPS_MsgTypes;

typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    uint8_t key;        // $
    uint8_t type;       // RecoverCommand
    uint8_t length;     // packet_length
    uint8_t __res;      // currently unused. (ensures word alignment of message). May be used for msg CRC  or other error correction in the future
} RecPktHeader;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) __GPS_Data {

	float latitude; //h00-h03
	float longitude; //h04-h07

	uint16_t timestamp[3]; //h08- h0E //0 is hour, 1 is minute, 2 is second

	uint8_t is_dominica; //bool //h0e

	uint8_t is_valid_data; //bool //h0f

	uint8_t msg_type; //GPS_MsgTypes //h10

	uint32_t quality; //h11-h14

} GPS_Data;

/* configuration packets */
typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    RecPktHeader header;
    struct { uint8_t value; } msg;
} RecPkt_uint8_t;

typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    RecPktHeader header;
    struct { float value; } msg;
} RecPkt_float;

typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    RecPktHeader header;
    struct { char value[256]; } msg;
} RecPkt_string;

typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    uint8_t value;
} RecConfigTxLevelPkt;

typedef RecPktHeader RecNullPkt;
#define REC_EMPTY_PKT(cmd) (RecNullPkt){.key = RECOVERY_PACKET_KEY_VALUE, .type = cmd, .length = 0} 

typedef RecPkt_float    RecAPRSFrequencyPkt;    // APRS_FREQ
typedef RecPkt_string   RecAPRSCallSignPkt;     // APRS_CALL_SIGN        
 // APRS_MESSAGE     
 // APRS_TX_INTERVAL 

typedef union  {
    struct {
        RecPktHeader header;
        char msg[256];
    } common;
    RecNullPkt       null_packet;
    RecPkt_uint8_t   u8_packet;
    RecPkt_float     float_packet;
    RecPkt_string    string_packet;
} RecPkt;

/* GLOBAL/STATIC VARIABLES ******************************************************/

int g_recovery_thread_is_running = 0;
static int g_recovery_initialized = 0;
static FILE* recovery_data_file = NULL;
static char recovery_data_file_notes[256] = "";
static const char* recovery_data_file_headers[] = {
  "GPS",
  };
static const int num_recovery_data_file_headers = 1;
static char gps_location[GPS_LOCATION_LENGTH];
static int recovery_fd = PI_INIT_FAILED;

typedef enum {
    REC_STATE_WAIT,
    REC_STATE_APRS,
    REC_STATE_BOOTLOADER,
    REC_STATE_SHUTDOWN,
}RecoveryBoardState;

static struct {
    RecoveryBoardState state;
    pthread_mutex_t    state_lock; 
    time_t             wdt_time_us;
} s_recovery_board_model = {
    .state = REC_STATE_APRS,
    .wdt_time_us = 0,
};

/* FUCNTION DEFINITIONS ******************************************************/

/* STATIC
 * serial write method with guard case for uninitialized recovery board.
 */
static int __recovery_write(const void *pkt, size_t size) {
    if (!g_recovery_initialized) {
        CETI_ERR("Recovery board uninitialized.");
        return -20;
    } 
    #ifdef DEBUG
    char packet_debug[512];
    int len = 0;
    for (int i = 0; i < size; i++ ){
        len += sprintf(&packet_debug[len], "%02x ", ((uint8_t*)pkt)[i]);
    }
    CETI_LOG("Pi->Rec: {%s}\n", packet_debug);
    #endif
    return serWrite(recovery_fd, (char *)pkt, size);  
}

/* Get the next received communication packet from the recovery board.
 * Arguments: 
 *     RecPkt *packet   : ptr to destination packet union.
 *     time_t timout_us : timeout in microsecond, 0 = never.
 */
int recovery_get_packet(RecPkt *packet, time_t timeout_us) {
    bool header_complete = 0;
    int expected_bytes = sizeof(RecPktHeader);
    time_t start_time_us = get_global_time_us();

    packet->common.header.key = 0;
    do {
        int bytes_avail = serDataAvailable(recovery_fd);
        if (bytes_avail < 0) { // UART Error
            CETI_ERR("Recovery board UART error");
            return -1;
        }

        if (bytes_avail < expected_bytes) { // no bytes available
            usleep(1000); // wait a bit
            continue;     // try again
        }

        // scan bytes until start byte (skip if start byte already found)
        while ((bytes_avail) && (packet->common.header.key != RECOVERY_PACKET_KEY_VALUE)) {
            int result = serRead(recovery_fd, (char *)&(packet->common.header.key), 1); 
            if (result < 0) { // UART Error
                CETI_ERR("Recovery board UART error");
                return -1;
            }

            bytes_avail--;
        }

        if (packet->common.header.key != RECOVERY_PACKET_KEY_VALUE) {
            //packet key not
            continue;
        }

        if(!header_complete){
            // read rest of header
            expected_bytes = sizeof(RecPktHeader) - 1;
            if (bytes_avail < expected_bytes) // not enough bytes to complete header
                continue;                     // get more bytes

            int result = serRead(recovery_fd, (char *)&(packet->common.header.type), expected_bytes); 
            if (result < 0) { // UART Error
                CETI_ERR("Recovery board UART error");
                return -1;
            }
            bytes_avail -= expected_bytes;
            header_complete = 1;
        }

        // read message if any
        if(packet->common.header.length == 0)
            return 0;

        expected_bytes = packet->common.header.length;
        if (bytes_avail < expected_bytes){ // not enough bytes to complete msg
            continue;                      // get more bytes
        }

        int result = serRead(recovery_fd, packet->common.msg, expected_bytes);
        if (result < 0) { // UART Error
            CETI_ERR("Recovery board UART error");
            return -1;
        }

        return 0; // Success !!! 
    } while ((timeout_us == 0)  || ((get_global_time_us() - start_time_us) < timeout_us));

    //Timeout
    // CETI_ERR("Recovery board timeout"); // this may not be an error, data may just not be available
    return -2;
}

static int __recovery_get_aprs_callsign(char buffer[static 7]) {
    //send query cmd
    RecNullPkt q_pkt = REC_EMPTY_PKT(REC_CMD_QUERY_APRS_CALLSIGN);
    RecPkt ret_pkt = {};
    __recovery_write(&q_pkt, sizeof(q_pkt));


    //wait for response
    int64_t start_time_us = get_global_time_us();
    do {
        if (recovery_get_packet(&ret_pkt, RECOVERY_UART_TIMEOUT_US) == -1) {
            CETI_ERR("Recovery board packet reading error");
            return -1;
        }

        if(ret_pkt.common.header.type == REC_CMD_CONFIG_APRS_CALLSIGN) {
            size_t len = ret_pkt.string_packet.header.length;
            len = (6 < len) ? 6 : len;
            memcpy(buffer, ret_pkt.string_packet.msg.value, len);
            buffer[len] = 0;
            return 0;
        }
        
        //received incorrect packet type, keep reading
    } while ((get_global_time_us() - start_time_us) < RECOVERY_UART_TIMEOUT_US);

    //Timeout
    CETI_ERR("Recovery board timeout");
    return -2;
}

static int __recovery_get_aprs_ssid(uint8_t *ssid){
    RecNullPkt q_pkt = REC_EMPTY_PKT(REC_CMD_QUERY_APRS_SSID);
    RecPkt ret_pkt = {};

    __recovery_write(&q_pkt, sizeof(q_pkt));


    //wait for response
    int64_t start_time_us = get_global_time_us();
    do {
        if (recovery_get_packet(&ret_pkt, RECOVERY_UART_TIMEOUT_US) == -1) {
            CETI_ERR("Recovery board packet reading error");
            return -1;
        }

        if(ret_pkt.common.header.type == REC_CMD_CONFIG_APRS_SSID) {
            *ssid = ret_pkt.u8_packet.msg.value;
            return 0;
        }
        
        //received incorrect packet type, keep reading
    } while ((get_global_time_us() - start_time_us) < RECOVERY_UART_TIMEOUT_US);
    
    //Timeout
    CETI_ERR("Recovery board timeout");
    return -2;
}

int recovery_get_aprs_callsign(APRSCallsign *callsign){
    int result = 0;
    result |= __recovery_get_aprs_callsign(callsign->callsign);
    result |= __recovery_get_aprs_ssid(&callsign->ssid);
    return result;
}

int recovery_get_aprs_freq_mhz(float *p_freq_MHz){
    //send query cmd
    RecNullPkt q_pkt = REC_EMPTY_PKT(REC_CMD_QUERY_APRS_FREQ);
    RecPkt ret_pkt = {};
    __recovery_write(&q_pkt, sizeof(q_pkt));


    //wait for response
    int64_t start_time_us = get_global_time_us();
    do {
        if (recovery_get_packet(&ret_pkt, RECOVERY_UART_TIMEOUT_US) == -1) {
            CETI_ERR("Recovery board packet reading error");
            return -1;
        }

        if(ret_pkt.common.header.type == REC_CMD_CONFIG_APRS_FREQ) {
            *p_freq_MHz = ret_pkt.float_packet.msg.value; 
            return 0;
        }
        
        //received incorrect packet type, keep reading
    } while ((get_global_time_us() - start_time_us) < RECOVERY_UART_TIMEOUT_US);

    //Timeout
    CETI_ERR("Recovery board timeout");
    return -2;
}

static int __recovery_set_aprs_callsign(const char * callsign){
    size_t callsign_len = strlen(callsign);
    if (callsign_len > 6) {
        CETI_ERR("Callsign \"%s\" is too long", callsign);
        return -3;
    }

    RecPkt_string pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_CALLSIGN,
            .length = (uint8_t) callsign_len,
        },
    };
    memcpy(pkt.msg.value, callsign, callsign_len);
    return __recovery_write(&pkt, sizeof(RecPktHeader) + callsign_len);
}

static int __recovery_set_aprs_ssid(uint8_t ssid){
    RecPkt_uint8_t pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_SSID,
            .length = sizeof(uint8_t),
        },
        .msg = {.value = ssid}
    };

    if (ssid > 15) {
        CETI_ERR("APRS SSID (%d) outside allowable range (0-15)", ssid);
        return -4;
    }

    return __recovery_write(&pkt, sizeof(RecPkt_uint8_t));

}

int recovery_set_aprs_callsign(const APRSCallsign *callsign){
    int result;
    result = __recovery_set_aprs_callsign(callsign->callsign);
    result |= __recovery_set_aprs_ssid(callsign->ssid);
    return result;
}

int recovery_set_aprs_freq_mhz(float freq_MHz){
    RecPkt_float pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_FREQ,
            .length = sizeof(float)
        }, 
        .msg = { .value = freq_MHz}
    };

    return __recovery_write(&pkt, sizeof(pkt));
}

static int __recovery_set_aprs_rx_callsign(const char * callsign){
    size_t callsign_len = strlen(callsign);
    if (callsign_len > 6) {
        CETI_ERR("Callsign \"%s\" is too long", callsign);
        return -3;
    }

    RecPkt_string pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_MSG_RCPT_CALLSIGN,
            .length = (uint8_t) callsign_len,
        },
    };
    memcpy(pkt.msg.value, callsign, callsign_len);

    return __recovery_write(&pkt, sizeof(RecPktHeader) + callsign_len);
}

static int __recovery_set_aprs_rx_ssid(uint8_t ssid){
    RecPkt_uint8_t pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_MSG_RCPT_SSID,
            .length = sizeof(uint8_t),
        },
        .msg = {.value = ssid}
    };

    if (ssid > 15) {
        CETI_ERR("APRS SSID (%d) outside allowable range (0-15)", ssid);
        return -4;
    }

    return __recovery_write(&pkt, sizeof(RecPkt_uint8_t));
}

int recovery_set_aprs_message_recipient(const APRSCallsign *callsign){
    int result;
    result = __recovery_set_aprs_rx_callsign(callsign->callsign);
    result |= __recovery_set_aprs_rx_ssid(callsign->ssid);
    return result;
}

int recovery_set_comment(const char *message){
    size_t len = strlen(message);
    if (len > 40) {
        len = 40;
    }

    RecPkt_string pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_APRS_COMMENT,
            .length = len,
        }
    };
    memcpy(pkt.msg.value, message, len);
    return __recovery_write(&pkt, sizeof(pkt.header) + len);
}

int recovery_set_critical_voltage(float voltage){
    RecPkt_float pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_CRITICAL_VOLTAGE,
            .length = sizeof(float)
        }, 
        .msg = { .value = voltage}
    };
    return __recovery_write(&pkt, sizeof(pkt));
}

int recovery_set_power_level(RecoveryPowerLevel power_level){
    struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
        RecPktHeader header;
        RecConfigTxLevelPkt msg;
    } pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_VHF_POWER_LEVEL,
            .length = sizeof(RecConfigTxLevelPkt)
        },
        .msg = {.value = power_level},
    };
    __recovery_write(&pkt, sizeof(pkt));
    return 0;
}

int recovery_message(const char *message){
    size_t message_len = strlen(message);
    if (message_len > 67) {
        return -1;
    }

    RecPkt pkt = {
        .common = {
            .header = {
                .key = RECOVERY_PACKET_KEY_VALUE,
                .type = REC_CMD_APRS_MESSAGE,
                .length = message_len,
            }
        }
    };
    memcpy(pkt.string_packet.msg.value, message, message_len);
    return __recovery_write(&pkt, sizeof(RecPktHeader) + message_len);
}

//-----------------------------------------------------------------------------
// On/Off
//-----------------------------------------------------------------------------

// initializes recovery board from powered off state
int recovery_init(void) {
    if (pthread_mutex_init(&s_recovery_board_model.state_lock, NULL) != 0) { 
        printf("\n state mutex init has failed\n"); 
        return -2; 
    } 

    WTResult hal_result = wt_recovery_init();
    if (hal_result != WT_OK) {
        CETI_ERR("Failed to initialize recovery board: %s", wt_strerror(hal_result));
        return (-1);
    }

    //turn on recovery
    hal_result = wt_recovery_restart(); //turn on recovery
    if (hal_result != WT_OK) {
        CETI_ERR("Failed to restart recovery board: %s", wt_strerror(hal_result));
        return (-1);
    }

    pthread_mutex_lock(&s_recovery_board_model.state_lock);
    s_recovery_board_model.state = REC_STATE_APRS;
    pthread_mutex_unlock(&s_recovery_board_model.state_lock);

    usleep(50000); // sleep a bit

    // Open serial communication
    recovery_fd = serOpen("/dev/serial0", 115200,0);
    if (recovery_fd < 0) {
        CETI_ERR("Failed to open the recovery board serial port");
        wt_recovery_off();
        return (-1);
    }

    g_recovery_initialized = true;
    APRSCallsign callsign;
    // // // get call sign to ensure uart
    if (recovery_get_aprs_callsign(&callsign) == 0){
        CETI_LOG("Callsign: \"%s-%d\".", callsign.callsign, callsign.ssid);
    } else {
        g_recovery_initialized = false;
        CETI_ERR("Did not receive recovery board response.");
        serClose(recovery_fd);
        wt_recovery_off();
        return (-2);
    }
    recovery_on();

    CETI_LOG("Successfully initialized the recovery board");
    return 0;
}

// sets recovery board into "arps" state
int recovery_on(void) {
    if(!g_recovery_initialized){
        CETI_ERR("Recovery board uninitialized.");
        return -1;
    }

    RecPktHeader start_pkt = {
        .key = RECOVERY_PACKET_KEY_VALUE,
        .type = REC_CMD_START,
        .length = 0,
    };

    pthread_mutex_lock(&s_recovery_board_model.state_lock);
    __recovery_write(&start_pkt, sizeof(start_pkt));
    s_recovery_board_model.state = REC_STATE_APRS;
    pthread_mutex_unlock(&s_recovery_board_model.state_lock);

    return 0;
}

//Note: lock state mutex prior to calling
static int __recovery_off (void) {
    if(!g_recovery_initialized){
        CETI_ERR("Recovery board uninitialized.");
        return -1;
    }

    RecPktHeader stop_pkt = {
        .key = RECOVERY_PACKET_KEY_VALUE,
        .type = REC_CMD_STOP,
        .length = 0,
    };

    __recovery_write(&stop_pkt, sizeof(stop_pkt));
    s_recovery_board_model.state = REC_STATE_WAIT;
    return 0;
}

// sets recovery board into "wait" state
// command must be sent every 10 minutes to remain in this state
int recovery_off(void) {
    if(!g_recovery_initialized){
        CETI_ERR("Recovery board uninitialized.");
        return -1;
    }

    pthread_mutex_lock(&s_recovery_board_model.state_lock);
    __recovery_off();
    pthread_mutex_unlock(&s_recovery_board_model.state_lock);
    return 0;
}

// sets recovery board into "shutdown" state
// recovery board must be reset to exit state
int recovery_shutdown(void){
    if(!g_recovery_initialized){
        CETI_ERR("Recovery board uninitialized.");
        return -1;
    }
    
    RecPktHeader critical_pkt = {
        .key = RECOVERY_PACKET_KEY_VALUE,
        .type = REC_CMD_CRITICAL,
        .length = 0,
    };

    pthread_mutex_lock(&s_recovery_board_model.state_lock);
    __recovery_write(&critical_pkt, sizeof(critical_pkt));
    s_recovery_board_model.state = REC_STATE_SHUTDOWN;
    pthread_mutex_unlock(&s_recovery_board_model.state_lock);

    return 0;
}

// kills recovery board but trying to shut it down then cutting
// power the to board
void recovery_kill(void){
    recovery_shutdown();
    serClose(recovery_fd); // close serial port
    wt_recovery_off();
}

// kills recovery board, then re initializes
int recovery_restart(void) {
    recovery_kill();
    usleep(50000);
    return recovery_init();
}

//-----------------------------------------------------------------------------
// Get GPS
//-----------------------------------------------------------------------------
int recovery_get_gps_data(char gpsLocation[static GPS_LOCATION_LENGTH], time_t timeout_us){
    RecPkt pkt;
    int result = 0;

    //wait for GPS packet
    do {
        result = recovery_get_packet(&pkt, timeout_us);
        switch (result) {
            case -1:
                CETI_ERR("Recovery board communication error");
                return result;

            case -2:
                // this is not an error, just no data available
                // CETI_LOG("Recovery board pkt timeout");
                return result;

            default:
                break;
        }
    } while(pkt.common.header.type != REC_CMD_GPS_PACKET);

    //copy packet into buffer
    uint_fast8_t len = pkt.string_packet.header.length;
    memcpy(gpsLocation, pkt.string_packet.msg.value, len);
    return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
int recovery_thread_init(void) {
    int result = recovery_init();
    if(result != 0){
        CETI_ERR("Failed to initalize recovery board hardware");
        return result;
    }


    // Open an output file to write data.
    if(init_data_file(recovery_data_file, RECOVERY_DATA_FILEPATH,
                        recovery_data_file_headers,  num_recovery_data_file_headers,
                        recovery_data_file_notes, "init_data_file()") < 0) {
        CETI_LOG("Failed to initialize recovery board thread");              
        return -1;
    }

    CETI_LOG("Successfully initialized recovery board thread");
    return 0;
}

void* recovery_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_recovery_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(RECOVERY_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(RECOVERY_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("Successfully set affinity to CPU %d", RECOVERY_CPU);
      else
        CETI_ERR("Failed to set affinity to CPU %d", RECOVERY_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    g_recovery_thread_is_running = 1;

    if (!g_recovery_initialized){
        for (int retry_count = 0; recovery_init() != 0; retry_count++){
            if(g_exit){
                break;
            }
            
            if(retry_count == 5) {
                CETI_ERR("Unable to initialize recovery board");
                return NULL;
            }
        }
    }

    while (!g_exit) {
        pthread_mutex_lock(&s_recovery_board_model.state_lock); //prevents recovery board turning off mid communication
        switch(s_recovery_board_model.state) {
            case REC_STATE_WAIT: {
                #if RECOVERY_WDT_ENABLED
                //check if watchdog needs reset
                const int wdt_resend_us = (RECOVERY_WDT_TRIGGER_TIME_MIN*60)*1000000;
                if ((get_global_time_us() - s_recovery_board_model.wdt_time_us) > (wdt_resend_us - 1000000)){
                    __recovery_off(); //we already have mutex
                    s_recovery_board_model.wdt_time_us = get_global_time_us();
                }
                #endif
                pthread_mutex_unlock(&s_recovery_board_model.state_lock);
                sleep(1);
                break; 
            }

            case REC_STATE_APRS: { 
                long long global_time_us;
                int rtc_count;
                int result;
                long long polling_sleep_duration_us;

                recovery_data_file = fopen(RECOVERY_DATA_FILEPATH, "at");
                if(recovery_data_file == NULL) {
                    pthread_mutex_unlock(&s_recovery_board_model.state_lock); //release recovery state
                    CETI_ERR("Failed to open data output file: %s", RECOVERY_DATA_FILEPATH);
                    // Sleep a bit before retrying.
                    for(int i = 0; i < 10 && !g_exit; i++) {
                        usleep(100000);
                    }
                    break;
                }

                // Acquire timing and sensor information as close together as possible.
                global_time_us = get_global_time_us();
                rtc_count = getRtcCount();
                result = recovery_get_gps_data(gps_location, 1000000);
                pthread_mutex_unlock(&s_recovery_board_model.state_lock); //release recovery state

                if (result == -2) { //timeout
                    // no gps packet available
                    // nothing to do
                    fclose(recovery_data_file);
                    break;
                } else if (result == -1) {
                    fclose(recovery_data_file);
                    CETI_ERR("Recovery board communication Error");
                    recovery_restart();
                    sleep(10);
                    break;
                }   

                // Write timing information.
                fprintf(recovery_data_file, "%lld", global_time_us);
                fprintf(recovery_data_file, ",%d", rtc_count);
                // Write any notes, then clear them so they are only written once.
                fprintf(recovery_data_file, ",%s", recovery_data_file_notes);
                strcpy(recovery_data_file_notes, "");
                // Write the sensor data.
                fprintf(recovery_data_file, ",\"%s\"", gps_location);
                // Finish the row of data and close the file.
                fprintf(recovery_data_file, "\n");
                fclose(recovery_data_file);

                // Delay to implement a desired sampling rate.
                // Take into account the time it took to acquire/save data.
                polling_sleep_duration_us = RECOVERY_SAMPLING_PERIOD_US;
                polling_sleep_duration_us -= get_global_time_us() - global_time_us;
                if(polling_sleep_duration_us > 0){
                    usleep(polling_sleep_duration_us);
                }
                break;
            }

            default: { //Do nothing
                pthread_mutex_unlock(&s_recovery_board_model.state_lock);
                sleep(1);
                break;
            }
        }
    }
    recovery_kill();
    serClose(recovery_fd);
    g_recovery_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}