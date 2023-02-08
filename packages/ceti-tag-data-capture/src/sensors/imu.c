//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "imu.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_imu_thread_is_running = 0;
static FILE* imu_data_file = NULL;
static char imu_data_file_notes[256] = "";
static const char* imu_data_file_headers[] = {
  "Quat_i",
  "Quat_j",
  "Quat_k",
  "Quat_Re",
  };
static const int num_imu_data_file_headers = 4;
static int16_t imu_quaternion[4];

int init_imu() {
  if(resetIMU() < 0) {
    CETI_LOG("init_imu(): XXXX Failed to initialize the IMU XXXX");
    return -1;
  }
  CETI_LOG("init_imu(): Successfully initialized the IMU");

  // Open an output file to write data.
  if(init_data_file(imu_data_file, IMU_DATA_FILEPATH,
                     imu_data_file_headers,  num_imu_data_file_headers,
                     imu_data_file_notes, "init_imu()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* imu_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_imu_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(IMU_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(IMU_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("imu_thread(): Successfully set affinity to CPU %d", IMU_CPU);
      else
        CETI_LOG("imu_thread(): XXX Failed to set affinity to CPU %d", IMU_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("imu_thread(): Starting loop to periodically acquire data");
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_imu_thread_is_running = 1;
    while (!g_exit) {
      imu_data_file = fopen(IMU_DATA_FILEPATH, "at");
      if(imu_data_file == NULL)
      {
        CETI_LOG("imu_thread(): failed to open data output file: %s", IMU_DATA_FILEPATH);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
      }
      else
      {
        // Acquire timing and sensor information as close together as possible.
        global_time_us = get_global_time_us();
        rtc_count = getRtcCount();
        if(getQuaternion(imu_quaternion) < 0)
          strcat(imu_data_file_notes, "ERROR | ");

        // Write timing information.
        fprintf(imu_data_file, "%lld", global_time_us);
        fprintf(imu_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(imu_data_file, ",%s", imu_data_file_notes);
        strcpy(imu_data_file_notes, "");
        // Write the sensor data.
        fprintf(imu_data_file, ",%d", imu_quaternion[0]);
        fprintf(imu_data_file, ",%d", imu_quaternion[1]);
        fprintf(imu_data_file, ",%d", imu_quaternion[2]);
        fprintf(imu_data_file, ",%d", imu_quaternion[3]);
        // Finish the row of data and close the file.
        fprintf(imu_data_file, "\n");
        fclose(imu_data_file);

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = IMU_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - global_time_us;
        if(polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
      }
    }
    g_imu_thread_is_running = 0;
    CETI_LOG("imu_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// BNO080 Interface
//-----------------------------------------------------------------------------

int resetIMU()
{
    gpioWrite(IMU_N_RESET, 1);  //reset the device
    usleep(1000);
    gpioWrite(IMU_N_RESET, 0);
    usleep(1000);
    gpioWrite(IMU_N_RESET, 1);

    return 0;
}

//-----------------------------------------------------------------------------
// This enables just the rotation vector feature
int setupIMU()
{
    char setFeatureCommand[21] = {0};
    char shtpHeader[4] = {0};
    char writeCmdBuf[28] = {0};
    char readCmdBuf[10] = {0};
    int retval = bbI2COpen(IMU_BB_I2C_SDA, IMU_BB_I2C_SCL, 200000);

    if (retval < 0) {
        CETI_LOG("setupIMU(): Failed to connect to the IMU\n");
        return -1;
    }

    CETI_LOG("setupIMU(): IMU connection opened\n");

    setFeatureCommand[0] = 0x15; // Packet length LSB (21 bytes)
    setFeatureCommand[2] = IMU_CHANNEL_CONTROL;
    setFeatureCommand[4] = IMU_SHTP_REPORT_SET_FEATURE_COMMAND;
    setFeatureCommand[5] = IMU_SENSOR_REPORTID_ROTATION_VECTOR;

    setFeatureCommand[9] = 0x20;  // Report interval LS (microseconds)
    setFeatureCommand[10] = 0xA1; // 0x0007A120 is 0.5 seconds
    setFeatureCommand[11] = 0x07;
    setFeatureCommand[12] = 0x00; // Report interval MS

    writeCmdBuf[0] = 0x04; // set address
    writeCmdBuf[1] = ADDR_IMU;
    writeCmdBuf[2] = 0x02; // start
    writeCmdBuf[3] = 0x07; // write
    writeCmdBuf[4] = 0x15; // length
    memcpy(&writeCmdBuf[5], setFeatureCommand, 21);
    writeCmdBuf[26] = 0x03; // stop
    writeCmdBuf[27] = 0x00; // end

    readCmdBuf[0] = 0x04; // set address
    readCmdBuf[1] = ADDR_IMU;
    readCmdBuf[2] = 0x02; // start
    readCmdBuf[3] = 0x01; // escape
    readCmdBuf[4] = 0x06; // read
    readCmdBuf[5] = 0x04; // length lsb
    readCmdBuf[6] = 0x00; // length msb
    readCmdBuf[7] = 0x03; // stop
    readCmdBuf[8] = 0x00; // end

    retval = bbI2CZip(IMU_BB_I2C_SDA, writeCmdBuf, 28, NULL, 0);
    if (retval < 0) {
        CETI_LOG("setupIMU(): XXX I2C write failed: %d", retval);
    }
    retval = bbI2CZip(IMU_BB_I2C_SDA, readCmdBuf, 9, shtpHeader, 4);
    if (retval < 0) {
        CETI_LOG("setupIMU(): XXX I2C read failed: %d", retval);
    }
    CETI_LOG("setupIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X",
           shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);
    bbI2CClose(IMU_BB_I2C_SDA);

    return 0;
}

//-----------------------------------------------------------------------------
int getRotation(rotation_t *pRotation)
{
    // parse the IMU rotation vector input report
    int numBytesAvail = 0;
    char pktBuff[256] = {0};
    char shtpHeader[4] = {0};
    int fd = i2cOpen(BUS_IMU, ADDR_IMU, 0);

    if (fd < 0) {
        CETI_LOG("getRotation(): XXX Failed to connect to the IMU\n");
        return -1;
    }

    CETI_LOG("getRotation(): IMU connection opened\n");

    // Byte   0    1    2    3    4   5   6    7    8      9     10     11
    //       |     HEADER      |            TIME       |  ID    SEQ   STATUS....
    while (1) {
        i2cReadDevice(fd, shtpHeader, 4);

        // msb is "continuation bit, not part of count"
        numBytesAvail = fmin(256, ((shtpHeader[1] << 8) | shtpHeader[0]) & 0x7FFF);

        if (numBytesAvail) {
            i2cReadDevice(fd, pktBuff, numBytesAvail);
            // make sure we have the right channel
            if (pktBuff[2] == 0x03) {
                // testing, should come back 0x05
                pRotation->reportID = pktBuff[9];
                pRotation->sequenceNum = pktBuff[10];
                CETI_LOG("getRotation(): report ID 0x%02X  sequ 0x%02X i "
                       "%02X%02X j %02X%02X k %02X%02X r %02X%02X\n",
                       pktBuff[9], pktBuff[10], pktBuff[14], pktBuff[13],
                       pktBuff[16], pktBuff[15], pktBuff[18], pktBuff[17],
                       pktBuff[20], pktBuff[19]);
            }
        } else {
            setupIMU(); // restart the sensor, reports somehow stopped coming
        }
        usleep(800000);
    }
    i2cClose(fd);
    return 0;
}

//-----------------------------------------------------------------------------
int getQuaternion(short *quaternion) {

    int numBytesAvail;
    char pktBuff[256] = {0};
    char shtpHeader[4] = {0};
    char commandBuffer[10] = {0};
    int retval;

    // parse the IMU quaternion vector input report

    if ((bbI2COpen(IMU_BB_I2C_SDA, IMU_BB_I2C_SCL, 200000)) < 0) {
        CETI_LOG("getQuaternion(): Failed to connect to the IMU");
        return -1;
    }

    // Byte   0    1    2    3    4   5   6    7    8      9     10     11
    //       |     HEADER      |            TIME       |  ID    SEQ
    //       STATUS....

    commandBuffer[0] = 0x04; // set address
    commandBuffer[1] = ADDR_IMU;
    commandBuffer[2] = 0x02; // start
    commandBuffer[3] = 0x01; // escape
    commandBuffer[4] = 0x06; // read
    commandBuffer[5] = 0x04; // #bytes lsb
    commandBuffer[6] = 0x00; // #bytes msb
    commandBuffer[7] = 0x03; // stop
    commandBuffer[8] = 0x00; // end

    retval = bbI2CZip(IMU_BB_I2C_SDA, commandBuffer, 9, shtpHeader, 4);
    // msb is "continuation bit, not part of count"
    numBytesAvail = fmin(256, ((shtpHeader[1] << 8) | shtpHeader[0]) & 0x7FFF);

    if (retval > 0 && numBytesAvail) {
        commandBuffer[5] = numBytesAvail & 0xff;
        commandBuffer[6] = (numBytesAvail >> 8) & 0xff;
        bbI2CZip(IMU_BB_I2C_SDA, commandBuffer, 9, pktBuff, numBytesAvail);
        if (retval > 0 && pktBuff[2] == 0x03) { // make sure we have the right channel
            quaternion[0] = (pktBuff[14] << 8) + pktBuff[13];
            quaternion[1] = (pktBuff[16] << 8) + pktBuff[15];
            quaternion[2] = (pktBuff[18] << 8) + pktBuff[17];
            quaternion[3] = (pktBuff[20] << 8) + pktBuff[19];
        }
        bbI2CClose(IMU_BB_I2C_SDA);
    } else {
        bbI2CClose(IMU_BB_I2C_SDA);
        setupIMU();
        CETI_LOG("getQuaternion(): XXX Failed to get quaternion from the IMU");
        return -1;
    }

    return (0);
}

//-----------------------------------------------------------------------------
int learnIMU() {
    char shtpHeader[4] = {0};
    char shtpData[256] = {0};
    int fd;
    uint16_t numBytesAvail;
    if ((fd = i2cOpen(BUS_IMU, ADDR_IMU, 0)) < 0) {
        CETI_LOG("learnIMU(): Failed to connect to the IMU");
        fprintf(stderr, "learnIMU(): XXX Failed to connect to the IMU\n");
        return -1;
    }

    printf("learnIMU(): OK, have the IMU connected!\n");

    // Get the shtp header
    i2cReadDevice(fd, shtpHeader, 4); // pigpio i2c read raw
    printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n",
           shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);

    // Figure out how many bytes are waiting
    numBytesAvail = (shtpHeader[1] << 8 | shtpHeader[0]);
    printf("learnIMU(): Bytes Avail are 0x%04X \n", numBytesAvail);

    // Get all bytes that are waiting (flush it out)
    while (numBytesAvail > 0) {
        i2cReadDevice(fd, shtpData, numBytesAvail);
        i2cReadDevice(fd, shtpHeader, 4);
        printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n",
               shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);
    }

    // Ask for product ID report
    shtpData[0] = 0x06; // header - packet length LSB
    shtpData[1] = 0x00; // header - packet length MSB
    shtpData[2] = 0x02; // header - command channel
    shtpData[3] = 0x00; // header - sequence no
    shtpData[4] = IMU_SHTP_REPORT_PRODUCT_ID_REQUEST;
    shtpData[5] = 0x00; // reserved

    i2cWriteDevice(fd, shtpData, 6);

    i2cReadDevice(fd, shtpHeader, 4);
    printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n",
           shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);

    i2cClose(fd);

    return 0;
}