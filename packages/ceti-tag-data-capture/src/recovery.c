//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "recovery.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
//private definitions
#define RECOVERY_PACKET_KEY_VALUE '$'

typedef enum recovery_commands_e {
        /* Set recovery state*/
    REC_CMD_START        = 0x01, //pi --> rec: sets rec into recovery state
    REC_CMD_STOP         = 0x02, //pi --> rec: sets rec into waiting state
    REC_CMD_COLLECT_ONLY = 0x03, //pi --> rec: sets rec into rx gps state
    REC_CMD_CRITICAL     = 0x04, //pi --> rec: sets rec into critical state

    /* recovery packet */
    REC_CMD_GPS_PACKET   = 0x10, //rec --> pi: raw gps packet

    /* recovery configuration */
    REC_CMD_CONFIG_CRITICAL_VOLTAGE = 0x20,
    REC_CMD_CONFIG_VHF_POWER_LEVEL = 0x21,
}RecoverCommand;

typedef enum __GPS_MESSAGE_TYPES {
	GPS_SIM = 0, //simulated GPS message
	GPS_GLL = 1,
	GPS_GGA = 2,
	GPS_RMC = 3,
	GPS_NUM_MSG_TYPES //Should always be the last element in the enum. Represents the number of message types. If you need to add a new type, put it before this element.
}GPS_MsgTypes;

typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    uint8_t key;    //$
    uint8_t type;   //RecoverCommand
    uint8_t length; //packet_length
}RecPktHeader;

typedef struct __attribute__((__packed__, scalar_storage_order("little-endian"))) __GPS_Data {

	float latitude; //h00-h03
	float longitude; //h04-h07

	uint16_t timestamp[3]; //h08- h0E //0 is hour, 1 is minute, 2 is second

	uint8_t is_dominica; //bool //h0e

	uint8_t is_valid_data; //bool //h0f

	uint8_t msg_type; //GPS_MsgTypes //h10

	uint32_t quality; //h11-h14

}GPS_Data;

/* configuration packets */
typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    float value;
}RecConfigCritVoltagePkt;


typedef struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
    uint8_t value;
}RecConfigTxLevelPkt;


// Global/static variables
int g_recovery_thread_is_running = 0;
static FILE* recovery_data_file = NULL;
static char recovery_data_file_notes[256] = "";
static const char* recovery_data_file_headers[] = {
  "GPS",
  };
static const int num_recovery_data_file_headers = 1;
static char gps_location[GPS_LOCATION_LENGTH];
static int recovery_fd = PI_INIT_FAILED;


/* FUCNTION DEFINITIONS ******************************************************/
int recovery_setCriticalVoltage(float voltage){
    struct __attribute__ ((__packed__, scalar_storage_order ("little-endian"))) {
        RecPktHeader header;
        RecConfigCritVoltagePkt msg;
    } pkt = {
        .header = {
            .key = RECOVERY_PACKET_KEY_VALUE,
            .type = REC_CMD_CONFIG_CRITICAL_VOLTAGE,
            .length = sizeof(RecConfigCritVoltagePkt)
        },
        .msg = {.value = voltage},
    };

    serWrite(recovery_fd, (char *)&pkt, sizeof(pkt));
    return 0;
}

int recovery_setPowerLevel(RecoveryPowerLevel power_level){
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
    serWrite(recovery_fd, (char *)&pkt, sizeof(pkt));
    return 0;
}

int init_recovery() {
    // Open serial communication
    recovery_fd = serOpen("/dev/serial0",115200,0);
    if (recovery_fd < 0) {
        CETI_LOG("Failed to open the recovery board serial port");
        return (-1);
    } 

    // Open an output file to write data.
    if(init_data_file(recovery_data_file, RECOVERY_DATA_FILEPATH,
                        recovery_data_file_headers,  num_recovery_data_file_headers,
                        recovery_data_file_notes, "init_recovery()") < 0)
    return -1;

    CETI_LOG("Successfully initialized the recovery board");
    return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
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
        CETI_LOG("XXX Failed to set affinity to CPU %d", RECOVERY_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_recovery_thread_is_running = 1;
    while (!g_exit) {
      recovery_data_file = fopen(RECOVERY_DATA_FILEPATH, "at");
      if(recovery_data_file == NULL)
      {
        CETI_LOG("failed to open data output file: %s", RECOVERY_DATA_FILEPATH);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
      }
      else
      {
        // Acquire timing and sensor information as close together as possible.
        global_time_us = get_global_time_us();
        rtc_count = getRtcCount();
        if(getGpsLocation(gps_location) < 0)
          strcat(recovery_data_file_notes, "ERROR | ");

        // Write timing information.
        fprintf(recovery_data_file, "%lld", global_time_us);
        fprintf(recovery_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(recovery_data_file, ",%s", recovery_data_file_notes);
        strcpy(recovery_data_file_notes, "");
        // Write the sensor data.
        fprintf(recovery_data_file, "\"%s\"", gps_location);
        // Finish the row of data and close the file.
        fprintf(recovery_data_file, "\n");
        fclose(recovery_data_file);

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = RECOVERY_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - global_time_us;
        if(polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
      }
    }
    recoveryOff();
    serClose(recovery_fd);
    g_recovery_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Testing
//-----------------------------------------------------------------------------
int testRecoverySerial(void) {

//Tx a test message on UART, loopback to confirm FPGA and IO connectivity

    int i;

    char buf[4096];
    char testbuf[16]= "Hello Whales!\n";

    int fd;
    int bytes_avail;

    fd = serOpen("/dev/serial0",9600,0);

    if(fd < 0) {
        CETI_LOG("Failed to open the serial port");
        return (-1);
    }
    else {
        CETI_LOG("Successfully opened the serial port");
    }

    for(i=0;i<128;i++) {
        usleep (1000000);

        bytes_avail = (serDataAvailable(fd));
        CETI_LOG("%d bytes are available from the serial port",bytes_avail);
        serRead(fd,buf,bytes_avail);
        CETI_LOG("%s",buf);

        CETI_LOG("Trying to write to the serial port with pigpio");
        serWrite(fd,testbuf,14); //test transmission

    }


    return (0);
}

//-----------------------------------------------------------------------------
// On/Off
//-----------------------------------------------------------------------------

int recoveryOn(void) {
    RecPktHeader start_pkt = {
        .key = RECOVERY_PACKET_KEY_VALUE,
        .type = REC_CMD_START,
        .length = 0,
    };

    serWrite(recovery_fd, (char *)&start_pkt, sizeof(start_pkt));
    return 0;
}

int recoveryOff(void) {
    RecPktHeader stop_pkt = {
        .key = RECOVERY_PACKET_KEY_VALUE,
        .type = REC_CMD_STOP,
        .length = 0,
    };

    serWrite(recovery_fd, (char *)&stop_pkt, sizeof(stop_pkt));
    return 0;
}

int recoveryCritical(void){
     RecPktHeader critical_pkt = {
        .key = RECOVERY_PACKET_KEY_VALUE,
        .type = REC_CMD_CRITICAL,
        .length = 0,
    };

    serWrite(recovery_fd, (char *)&critical_pkt, sizeof(critical_pkt));

    return 0;
}

//-----------------------------------------------------------------------------
// Get GPS
//-----------------------------------------------------------------------------
int getGpsLocation(char gpsLocation[GPS_LOCATION_LENGTH]) {

//    The Recovery Board sends the GPS location sentence as an ASCII string
//    about once per second automatically via the serial
//    port. This function monitors the serial port and extracts the sentence for
//    reporting in the .csv record.
//
//    The Recovery Board may not be on at all times. Depending on the state
//    of the tag, it may be turned off to save power. See the state machine
//    function for current design. If the recovery board is off, the gps field
//    in the .csv file will indicate "No GPS update available"
//
//    The messages coming in from the Recovery Board are asynchronous relative to
//    the timing of execution of this function. So, partial messages will occur
//    if they are provided at a rate less than 2x the frequency of this
//    routine  - see SNS_SMPL_PERIOD which is nominally 1 second per cycle.

    static char buf[GPS_LOCATION_LENGTH];   //working buffer for serial data
    static char *pTempWr=buf;     //these need to be static in case of a partial message
    static int bytes_total=0;

    char *msg_start=NULL, *msg_end=NULL;
    int bytes_avail,i,complete=0,pending=0;

    // Check if any bytes are waiting in the UART buffer and store them temporarily.
    // It is possible to receive a partial message - no synch or handshaking in protocol
    bytes_avail = serDataAvailable(recovery_fd);
    bytes_total += bytes_avail; //keep a running count

    //copy available bytes into buffer
    if (bytes_avail && (bytes_total < GPS_LOCATION_LENGTH) ) {
        serRead(recovery_fd,pTempWr,bytes_avail);   //add the new bytes to the buffer
        pTempWr = pTempWr + bytes_avail;   //advance the write pointer
    } else { //defensive - something went wrong, reset the buffer
        memset(buf,0,GPS_LOCATION_LENGTH);         // reset entire buffer conents
        bytes_total = 0;                   // reset the static byte counter
        pTempWr = buf;                     // reset the static write pointer
    }

    
    //find message start
    char *gps_msg_ptr = NULL;
    size_t gps_msg_len = 0;
    for(char *i_ptr = buf; i_ptr < buf + sizeof(buf) - 3; i_ptr++){
        if(i_ptr[0] == RECOVERY_PACKET_KEY_VALUE && i_ptr[1] == REC_CMD_GPS_PACKET){
            gps_msg_len = i_ptr[2];
            gps_msg_ptr = i_ptr + 3;
            break;
        }
    }

    //no gps message found
    if(gps_msg_ptr == NULL){
        strcpy(gpsLocation,"No GPS update available");
        CETI_LOG("XXX No GPS update available");
        return -1;
    }

    //remove \n and \r at end of message
    char *end_ptr = gps_msg_ptr + gps_msg_len - 1;
    while((*end_ptr == '\n' || *end_ptr == '\r') && end_ptr >= gps_msg_ptr){
        end_ptr--;
    }

   //copy the message from the buffer
    memmove(gpsLocation, gps_msg_ptr, end_ptr - gps_msg_ptr);

    size_t bytes_used = ((gps_msg_ptr - buf) + gps_msg_len);
    if(bytes_used < bytes_total){
        memcpy(buf, buf + bytes_used, bytes_total - bytes_used);
        bytes_total -= bytes_used;
    }

    // // Scan the whole buffer for a GPS message - delimited by 'g' and '\n'
    // for (i=0; (i<=bytes_total && !complete); i++) {
    //     if( buf[i] == 'g' && !pending) {
    //             msg_start =  buf + i;
    //             pending = 1;
    //     }

    //     if ( buf[i] == '\n' && pending) {
    //             msg_end = buf + i - 1; //don't include the '/n'
    //             complete = 1;
    //     }
    // }

    // find message starting character
    // for(msg_start = buf; msg_start < &buf[GPS_LOCATION_LENGTH - (sizeof(GPS_Data) + 3 - 1)]; msg_start++){
    //     if(*msg_start == '$'){
    //         uint8_t msg_len = msg_start[2];
    //         if((msg_start[1] == REC_CMD_GPS_PACKET) && (msg_len == sizeof(GPS_Data))){
    //             complete = 1;
    //             msg_end = msg_start + msg_len;
    //             break;
    //         }
    //         else{
    //             msg_start += (3 + msg_len) - 1;
    //         }
    //     }
    // }

    // if(!complete){
    //     strcpy(gpsLocation,"No GPS update available");
    //     CETI_LOG("XXX No GPS update available");
    //     return -1;
    // }

    
    // memset(gpsLocation,0,GPS_LOCATION_LENGTH);
    // strncpy(gpsLocation,msg_start+1,(msg_end-buf));
    // memset(buf,0,GPS_LOCATION_LENGTH);             // reset buffer conents
    // bytes_total = 0;                        // reset the static byte counter
    // pTempWr = buf;                          // reset the static write pointer
    // gpsLocation[strcspn(gpsLocation, "\r\n")] = 0;


    return (0);
}