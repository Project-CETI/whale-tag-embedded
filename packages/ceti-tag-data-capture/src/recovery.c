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

// Global/static variables
int g_recovery_thread_is_running = 0;
static FILE* recovery_data_file = NULL;
static char recovery_data_file_notes[256] = "";
static const char* recovery_data_file_headers[] = {
  "GPS",
  };
static const int num_recovery_data_file_headers = 1;
static char gps_location[GPS_LOCATION_LENGTH];

int init_recovery() {
  CETI_LOG("init_recovery(): Successfully initialized the recovery board [did nothing]");

  // Open an output file to write data.
  if(init_data_file(recovery_data_file, RECOVERY_DATA_FILEPATH,
                     recovery_data_file_headers,  num_recovery_data_file_headers,
                     recovery_data_file_notes, "init_recovery()") < 0)
    return -1;

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
        CETI_LOG("recovery_thread(): Successfully set affinity to CPU %d", RECOVERY_CPU);
      else
        CETI_LOG("recovery_thread(): XXX Failed to set affinity to CPU %d", RECOVERY_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("recovery_thread(): Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_recovery_thread_is_running = 1;
    while (!g_exit) {
      recovery_data_file = fopen(RECOVERY_DATA_FILEPATH, "at");
      if(recovery_data_file == NULL)
      {
        CETI_LOG("recovery_thread(): failed to open data output file: %s", RECOVERY_DATA_FILEPATH);
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
    g_recovery_thread_is_running = 0;
    CETI_LOG("recovery_thread(): Done!");
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
        CETI_LOG("testRecoverySerial(): Failed to open the serial port");
        return (-1);
    }
    else {
        CETI_LOG("testRecoverySerial(): Successfully opened the serial port");
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

    int fd, result;

    if ( (fd = i2cOpen(1,ADDR_MAINTAG_IOX,0)) < 0 ) {
        CETI_LOG("rcvryOn(): XXXX Failed to open I2C connection for IO Expander XXXX");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result & (~RCVRY_RP_nEN & ~nRCVRY_SWARM_nEN & ~nRCVRY_VHF_nEN);
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}

int recoveryOff(void) {

    int fd, result;

    if ( (fd = i2cOpen(1,ADDR_MAINTAG_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOn(): XXXX Failed to open I2C connection for IO Expander XXXX");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result | (RCVRY_RP_nEN | nRCVRY_SWARM_nEN | nRCVRY_VHF_nEN);
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}

//-----------------------------------------------------------------------------
// Get GPS
//-----------------------------------------------------------------------------
int getGpsLocation(char *gpsLocation) {

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

    static int fd=PI_INIT_FAILED;  //handle for serial port. The port is left open for the life of the app
    static char buf[GPS_LOCATION_LENGTH];   //working buffer for serial data
    static char *pTempWr=buf;     //these need to be static in case of a partial message
    static int bytes_total=0;

    char *msg_start=NULL, *msg_end=NULL;
    int bytes_avail,i,complete=0,pending=0;

    // Open the serial port using the pigpio lib

    if(fd <= PI_INIT_FAILED) {
        fd = serOpen("/dev/serial0",115200,0);
        if (fd < 0) {
            CETI_LOG("getGpsLocation(): Failed to open the serial port");
            return (-1);
        }
        CETI_LOG("getGpsLocation(): Successfully opened the serial port");
    }

    // Check if any bytes are waiting in the UART buffer and store them temporarily.
    // It is possible to receive a partial message - no synch or handshaking in protocol

    bytes_avail = serDataAvailable(fd);
    bytes_total += bytes_avail; //keep a running count

    if (bytes_avail && (bytes_total < GPS_LOCATION_LENGTH) ) {
        serRead(fd,pTempWr,bytes_avail);   //add the new bytes to the buffer
        pTempWr = pTempWr + bytes_avail;   //advance the write pointer
    } else { //defensive - something went wrong, reset the buffer
        memset(buf,0,GPS_LOCATION_LENGTH);         // reset entire buffer conents
        bytes_total = 0;                   // reset the static byte counter
        pTempWr = buf;                     // reset the static write pointer
    }

    // Scan the whole buffer for a GPS message - delimited by 'g' and '\n'
    for (i=0; (i<=bytes_total && !complete); i++) {
        if( buf[i] == 'g' && !pending) {
                msg_start =  buf + i;
                pending = 1;
        }

        if ( buf[i] == '\n' && pending) {
                msg_end = buf + i - 1; //don't include the '/n'
                complete = 1;
        }
    }

    if (complete) { //copy the message from the buffer
        memset(gpsLocation,0,GPS_LOCATION_LENGTH);
        strncpy(gpsLocation,msg_start+1,(msg_end-buf));
        memset(buf,0,GPS_LOCATION_LENGTH);             // reset buffer conents
        bytes_total = 0;                        // reset the static byte counter
        pTempWr = buf;                          // reset the static write pointer
        gpsLocation[strcspn(gpsLocation, "\r\n")] = 0;
    } else {  //No complete sentence was available on this cycle, try again next time through
        strcpy(gpsLocation,"No GPS update available");
        CETI_LOG("getGpsLocation(): XXX No GPS update available");
        return -1;
    }

    return (0);
}