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
static char imu_data_filepath[100];
static FILE* imu_data_file = NULL;
static char imu_data_file_notes[256] = "";
static const char* imu_data_file_headers[] = {
  "Sensor_delay_us",
  "Quat_i",
  "Quat_j",
  "Quat_k",
  "Quat_Re",
  "Quat_accuracy",
  "Accel_x_m_ss",
  "Accel_y_m_ss",
  "Accel_z_m_ss",
  "Accel_accuracy",
  "Gyro_x_rad_s",
  "Gyro_y_rad_s",
  "Gyro_z_rad_s",
  "Gyro_accuracy",
  "Mag_x_uT",
  "Mag_y_uT",
  "Mag_z_uT",
  "Mag_accuracy",
  };
static const int num_imu_data_file_headers = 18;
static int imu_is_connected = 0;
static uint8_t imu_sequence_numbers[6] = {0}; // Each of the 6 channels has a sequence number (a message counter)
static int16_t imu_quaternion[4]; // i, j, k, real
static int16_t imu_accel_m_ss[3]; // x, y, z
static int16_t imu_gyro_rad_s[3]; // x, y, z
static int16_t imu_mag_ut[3];     // x, y, z
static int16_t imu_quaternion_accuracy; // rad
static int16_t imu_accel_accuracy; // a rating of 0-3
static int16_t imu_gyro_accuracy;  // a rating of 0-3
static int16_t imu_mag_accuracy;   // a rating of 0-3
static long imu_reading_delay_us = 0; // delay from sensor reading to data transmission

int init_imu() {
  if(setupIMU() < 0) {
    CETI_LOG("init_imu(): XXXX Failed to set up the IMU XXXX");
    return -1;
  }

  CETI_LOG("init_imu(): Successfully initialized the IMU");

  // Open an output file to write data.
  if(init_imu_data_file(1) < 0)
    return -1;

  return 0;
}

// Determine a new IMU data filename that does not already exist, and open a file for it.
int init_imu_data_file(int restarted_program)
{
  // Append a number to the filename base until one is found that doesn't exist yet.
  int data_file_postfix_count = 0;
  int data_file_exists = 0;
  do
  {
    sprintf(imu_data_filepath, "%s_%02d.csv", IMU_DATA_FILEPATH_BASE, data_file_postfix_count);
    data_file_exists = (access(imu_data_filepath, F_OK) != -1);
    data_file_postfix_count++;
  } while(data_file_exists);

  // Open the new file.
  int init_data_file_success = init_data_file(imu_data_file, imu_data_filepath,
                                              imu_data_file_headers,  num_imu_data_file_headers,
                                              imu_data_file_notes, "init_imu_data_file()");
  // Change the note from restarted to new file if this is not the first initialization.
  if(!restarted_program)
    strcpy(imu_data_file_notes, "New log file! | ");
  return init_data_file_success;
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
    int report_id_updated = -1;
    long imu_data_file_size_b = 0;
    long long global_time_us = get_global_time_us();
    long long start_global_time_us = get_global_time_us();
    int rtc_count;
    long long imu_last_data_file_flush_time_us = get_global_time_us();
    g_imu_thread_is_running = 1;
    imu_data_file = fopen(imu_data_filepath, "at");
    while(!g_exit)
    {
      if(imu_data_file == NULL)
      {
        CETI_LOG("imu_thread(): failed to open data output file: %s", imu_data_filepath);
        // Sleep a bit before retrying.
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
        imu_data_file = fopen(imu_data_filepath, "at");
      }
      else
      {
        // Wait for the IMU to have data available.
        // Note that it is a bit stochastic as to which feature report will be ready next.
        if(imu_is_connected)
        {
          do
          {
            report_id_updated = imu_read_data();
            if(report_id_updated == -1) // no data received yet
              usleep(2000); // Note that we are streaming 4 reports at 50 Hz, so we expect data to be available at about 200 Hz
            else
              usleep(50);  // Seems nice to limit the polling frequency a bit to manage CPU usage
          } while(report_id_updated == -1 // no data received yet
                  && get_global_time_us() - global_time_us < 5000000 // timeout not reached
                  && !g_exit); // program not quitting
        }
        else
          report_id_updated = -1;
        // Acquire timing information for this sample.
        global_time_us = get_global_time_us();
        rtc_count = getRtcCount();

        // Check whether data was actually received,
        //  or whether the loop exited due to other conditions.
        if(report_id_updated >= 0)
        {
          // Write timing information.
          fprintf(imu_data_file, "%lld", global_time_us);
          fprintf(imu_data_file, ",%d", rtc_count);
          // Write any notes, then clear them so they are only written once.
          fprintf(imu_data_file, ",%s", imu_data_file_notes);
          strcpy(imu_data_file_notes, "");
          // Write the sensor reading delay.
          fprintf(imu_data_file, ",%ld", imu_reading_delay_us);
          // Write quaternion data if it was the appropriate feature report.
          if(report_id_updated == IMU_SENSOR_REPORTID_ROTATION_VECTOR)
          {
            fprintf(imu_data_file, ",%d", imu_quaternion[0]);
            fprintf(imu_data_file, ",%d", imu_quaternion[1]);
            fprintf(imu_data_file, ",%d", imu_quaternion[2]);
            fprintf(imu_data_file, ",%d", imu_quaternion[3]);
            fprintf(imu_data_file, ",%d", imu_quaternion_accuracy);
          }
          else
            fprintf(imu_data_file, ",,,,,");
          // Write accelerometer data if it was the appropriate feature report.
          if(report_id_updated == IMU_SENSOR_REPORTID_ACCELEROMETER)
          {
            fprintf(imu_data_file, ",%d", imu_accel_m_ss[0]);
            fprintf(imu_data_file, ",%d", imu_accel_m_ss[1]);
            fprintf(imu_data_file, ",%d", imu_accel_m_ss[2]);
            fprintf(imu_data_file, ",%d", imu_accel_accuracy);
          }
          else
            fprintf(imu_data_file, ",,,,");
          // Write gyroscope data if it was the appropriate feature report.
          if(report_id_updated == IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED)
          {
            fprintf(imu_data_file, ",%d", imu_gyro_rad_s[0]);
            fprintf(imu_data_file, ",%d", imu_gyro_rad_s[1]);
            fprintf(imu_data_file, ",%d", imu_gyro_rad_s[2]);
            fprintf(imu_data_file, ",%d", imu_gyro_accuracy);
          }
          else
            fprintf(imu_data_file, ",,,,");
          // Write magnetometer data if it was the appropriate feature report.
          if(report_id_updated == IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED)
          {
            fprintf(imu_data_file, ",%d", imu_mag_ut[0]);
            fprintf(imu_data_file, ",%d", imu_mag_ut[1]);
            fprintf(imu_data_file, ",%d", imu_mag_ut[2]);
            fprintf(imu_data_file, ",%d", imu_mag_accuracy);
          }
          else
            fprintf(imu_data_file, ",,,,");
          // Finish the row of data.
          fprintf(imu_data_file, "\n");

          // Flush the file periodically.
          if(get_global_time_us() - imu_last_data_file_flush_time_us >= IMU_DATA_FILE_FLUSH_PERIOD_US)
          {
            // Check the file size and close the file.
            fseek(imu_data_file, 0L, SEEK_END);
            imu_data_file_size_b = ftell(imu_data_file);
            fclose(imu_data_file);

            // If the file size limit has been reached, start a new file.
            if((imu_data_file_size_b >= (long)(IMU_MAX_FILE_SIZE_MB)*1024L*1024L || imu_data_file_size_b < 0) && !g_exit)
              init_imu_data_file(0);

            // Open the file again.
            imu_data_file = fopen(imu_data_filepath, "at");

            // Record the flush time.
            imu_last_data_file_flush_time_us = get_global_time_us();
          }
        }
        // If there was an error, try to reset the IMU.
        // Ignore the initial few seconds though, since sometimes it takes a little bit to get in sync.
        if(report_id_updated == -1 && !g_exit && get_global_time_us() - start_global_time_us > 10000000)
        {
          CETI_LOG("imu_thread(): XXX Error reading from IMU. Waiting and then attempting to reconnect.");
          usleep(5000000);
          setupIMU();
          start_global_time_us = get_global_time_us();
        }

        // Note that no delay is added here to set the polling frequency,
        //  since the IMU feature reports will control the sampling rate.
      }
    }
    resetIMU(); // seems nice to stop the feature reports
    bbI2CClose(IMU_BB_I2C_SDA);
    imu_is_connected = 0;
    if(imu_data_file != NULL)
      fclose(imu_data_file);
    g_imu_thread_is_running = 0;
    CETI_LOG("imu_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// BNO080 Interface
//-----------------------------------------------------------------------------

int resetIMU()
{
    gpioSetMode(IMU_N_RESET, PI_OUTPUT);
    usleep(10000);
    gpioWrite(IMU_N_RESET, PI_HIGH);
    usleep(100000);
    gpioWrite(IMU_N_RESET, PI_LOW); // reset the device (active low reset)
    usleep(100000);
    gpioWrite(IMU_N_RESET, PI_HIGH);
    usleep(500000); // if this is about 150000 or less, the first feature report fails to start

    return 0;
}

//-----------------------------------------------------------------------------
int setupIMU()
{
    // Reset the IMU
    if(imu_is_connected)
      bbI2CClose(IMU_BB_I2C_SDA);
    imu_is_connected = 0;
    usleep(10000);
    resetIMU();

    // Open an I2C connection.
    int retval = bbI2COpen(IMU_BB_I2C_SDA, IMU_BB_I2C_SCL, 200000);
    if (retval < 0) {
        CETI_LOG("setupIMU(): XXX Failed to connect to the IMU XXX\n");
        imu_is_connected = 0;
        return -1;
    }
    imu_is_connected = 1;
    CETI_LOG("setupIMU(): IMU connection opened\n");

    // Reset the message counters for each channel.
    for(int channel_index = 0; channel_index < sizeof(imu_sequence_numbers)/sizeof(uint8_t); channel_index++)
      imu_sequence_numbers[channel_index] = 0;

    // Enable desired feature reports.
    imu_enable_feature_report(IMU_SENSOR_REPORTID_ROTATION_VECTOR, IMU_SAMPLING_PERIOD_QUAT_US);
    usleep(100000);
    imu_enable_feature_report(IMU_SENSOR_REPORTID_ACCELEROMETER, IMU_SAMPLING_PERIOD_9DOF_US);
    usleep(100000);
    imu_enable_feature_report(IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED, IMU_SAMPLING_PERIOD_9DOF_US);
    usleep(100000);
    imu_enable_feature_report(IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED, IMU_SAMPLING_PERIOD_9DOF_US);
    usleep(100000);

    return 0;
}

int imu_enable_feature_report(int report_id, uint32_t report_interval_us)
{
  char setFeatureCommand[21] = {0};
  char shtpHeader[4] = {0};
  char writeCmdBuf[28] = {0};
  char readCmdBuf[10] = {0};

  uint16_t feature_command_length = 21;

  // Populate the SHTP header (see 2.2.1 of "Sensor Hub Transport Protocol")
  setFeatureCommand[0] = feature_command_length & 0xFF; // Packet length LSB
  setFeatureCommand[1] = feature_command_length >> 8; // Packet length MSB
  setFeatureCommand[2] = IMU_CHANNEL_CONTROL;
  setFeatureCommand[3] = imu_sequence_numbers[IMU_CHANNEL_CONTROL]++; // sequence number for this channel

  // Populate the Set Feature Command (see 6.5.4 of "SH-2 Reference Manual")
  setFeatureCommand[4] = IMU_SHTP_REPORT_SET_FEATURE_COMMAND;
  setFeatureCommand[5] = report_id;
  setFeatureCommand[6] = 0; // feature flags
  setFeatureCommand[7] = 0; // change sensitivity LSB
  setFeatureCommand[8] = 0; // change sensitivity MSB
  // Set the report interval in microseconds, as 4 bytes with LSB first
  for(int interval_byte_index = 0; interval_byte_index < 4; interval_byte_index++)
    setFeatureCommand[9+interval_byte_index] = (report_interval_us >> (8*interval_byte_index)) & 0xFF;
  setFeatureCommand[13] = 0; // batch interval LSB
  setFeatureCommand[14] = 0;
  setFeatureCommand[15] = 0;
  setFeatureCommand[16] = 0; // batch interval MSB
  setFeatureCommand[17] = 0; // sensor-specific configuration word LSB
  setFeatureCommand[18] = 0;
  setFeatureCommand[19] = 0;
  setFeatureCommand[20] = 0; // sensor-specific configuration word MSB

  // Write the command to enable the feature report.
  writeCmdBuf[0] = 0x04; // set address
  writeCmdBuf[1] = ADDR_IMU;
  writeCmdBuf[2] = 0x02; // start
  writeCmdBuf[3] = 0x07; // write
  writeCmdBuf[4] = 0x15; // length
  memcpy(&writeCmdBuf[5], setFeatureCommand, feature_command_length);
  writeCmdBuf[26] = 0x03; // stop
  writeCmdBuf[27] = 0x00; // end
  int retval = bbI2CZip(IMU_BB_I2C_SDA, writeCmdBuf, 28, NULL, 0);
  if (retval < 0) {
      CETI_LOG("imu_enable_feature_report(): XXX I2C write failed enabling report %d: %d", report_id, retval);
      return -1;
  }

  // Read a response header.
  readCmdBuf[0] = 0x04; // set address
  readCmdBuf[1] = ADDR_IMU;
  readCmdBuf[2] = 0x02; // start
  readCmdBuf[3] = 0x01; // escape
  readCmdBuf[4] = 0x06; // read
  readCmdBuf[5] = 0x04; // length lsb
  readCmdBuf[6] = 0x00; // length msb
  readCmdBuf[7] = 0x03; // stop
  readCmdBuf[8] = 0x00; // end
  retval = bbI2CZip(IMU_BB_I2C_SDA, readCmdBuf, 9, shtpHeader, 4);
  if (retval < 0) {
      CETI_LOG("imu_enable_feature_report(): XXX I2C read failed enabling report %d: %d", report_id, retval);
      return -1;
  }
  CETI_LOG("imu_enable_feature_report(): Enabled report %d.  Header is 0x%02X  0x%02X  0x%02X  0x%02X",
         report_id, shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);

  return 0;
}

//-----------------------------------------------------------------------------
int imu_read_data()
{
  int numBytesAvail;
  char pktBuff[256] = {0};
  char shtpHeader[4] = {0};
  char commandBuffer[10] = {0};
  int retval;

  // Read a header to see how many bytes are available.
  commandBuffer[0] = 0x04; // set address
  commandBuffer[1] = ADDR_IMU;
  commandBuffer[2] = 0x02; // start
  commandBuffer[3] = 0x01; // escape
  commandBuffer[4] = 0x06; // read
  commandBuffer[5] = 0x04; // #bytes lsb (read a 4-byte header)
  commandBuffer[6] = 0x00; // #bytes msb
  commandBuffer[7] = 0x03; // stop
  commandBuffer[8] = 0x00; // end
  retval = bbI2CZip(IMU_BB_I2C_SDA, commandBuffer, 9, shtpHeader, 4);
  numBytesAvail = fmin(256, ((shtpHeader[1] << 8) | shtpHeader[0]) & 0x7FFF); // msb is "continuation bit", not part of count

  if(retval > 0 && numBytesAvail > 0)
  {
    // Adjust the read command for the amount of data available.
    commandBuffer[5] = numBytesAvail & 0xff; // LSB
    commandBuffer[6] = (numBytesAvail >> 8) & 0xff; // MSB
    // Read the data.
    retval = bbI2CZip(IMU_BB_I2C_SDA, commandBuffer, 9, pktBuff, numBytesAvail);
    // Parse the data.
    if(retval > 0 && pktBuff[2] == IMU_CHANNEL_REPORTS) // make sure we have the right channel
    {
      uint8_t report_id = pktBuff[9];
      uint32_t timestamp_delay_us = ((uint32_t)pktBuff[8] << (8 * 3)) | ((uint32_t)pktBuff[7] << (8 * 2)) | ((uint32_t)pktBuff[6] << (8 * 1)) | ((uint32_t)pktBuff[5] << (8 * 0));
      imu_reading_delay_us = (long)timestamp_delay_us;
      //uint8_t sequence_number = pktBuff[10];
//      CETI_LOG("IMU REPORT %d  seq %3d  timestamp_delay_us %ld\n",
//        report_id, sequence_number, (long)timestamp_delay_us);
//      for(int i = 0; i < 4; i++)
//        CETI_LOG("    H%2d: %d\n", i, shtpHeader[i]);
//      for(int i = 0; i < numBytesAvail; i++)
//        CETI_LOG("    D%2d: %d\n", i, pktBuff[i]);

      // Parse the data in the report.
      uint8_t status = pktBuff[11] & 0x03; // Get status bits
      uint16_t data1 = (uint16_t)pktBuff[14] << 8 | pktBuff[13];
      uint16_t data2 = (uint16_t)pktBuff[16] << 8 | pktBuff[15];
      uint16_t data3 = (uint16_t)pktBuff[18] << 8 | pktBuff[17];
      uint16_t data4 = 0;
      uint16_t data5 = 0; //We would need to change this to uin32_t to capture time stamp value on Raw Accel/Gyro/Mag reports
      //uint16_t data6 = 0;
      if(numBytesAvail > 18)
        data4 = (uint16_t)pktBuff[20] << 8 | pktBuff[19];
      if(numBytesAvail > 20)
        data5 = (uint16_t)pktBuff[22] << 8 | pktBuff[21];
      //if(numBytesAvail > 22)
      //  data6 = (uint16_t)pktBuff[24] << 8 | pktBuff[23];

      if(report_id == IMU_SENSOR_REPORTID_ROTATION_VECTOR)
      {
        imu_quaternion[0] = data1;//(int)((((uint16_t)pktBuff[14]) << 8) + (uint16_t)pktBuff[13]);
        imu_quaternion[1] = data2;//(int)((((uint16_t)pktBuff[16]) << 8) + (uint16_t)pktBuff[15]);
        imu_quaternion[2] = data3;//(int)((((uint16_t)pktBuff[18]) << 8) + (uint16_t)pktBuff[17]);
        imu_quaternion[3] = data4;//(int)((((uint16_t)pktBuff[20]) << 8) + (uint16_t)pktBuff[19]);
        imu_quaternion_accuracy = data5;//(int)((((uint16_t)pktBuff[22]) << 8) + (uint16_t)pktBuff[21]);
      }
      else if(report_id == IMU_SENSOR_REPORTID_ACCELEROMETER)
      {
        imu_accel_m_ss[0] = data1;//(int)((((uint16_t)pktBuff[14]) << 8) + (uint16_t)pktBuff[13]);
        imu_accel_m_ss[1] = data2;//(int)((((uint16_t)pktBuff[16]) << 8) + (uint16_t)pktBuff[15]);
        imu_accel_m_ss[2] = data3;//(int)((((uint16_t)pktBuff[18]) << 8) + (uint16_t)pktBuff[17]);
        imu_accel_accuracy = status;
      }
      else if(report_id == IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED)
      {
        imu_gyro_rad_s[0] = data1;//(int)((((uint16_t)pktBuff[14]) << 8) + (uint16_t)pktBuff[13]);
        imu_gyro_rad_s[1] = data2;//(int)((((uint16_t)pktBuff[16]) << 8) + (uint16_t)pktBuff[15]);
        imu_gyro_rad_s[2] = data3;//(int)((((uint16_t)pktBuff[18]) << 8) + (uint16_t)pktBuff[17]);
        imu_gyro_accuracy = status;
      }
      else if(report_id == IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED)
      {
        imu_mag_ut[0] = data1;//(int)((((uint16_t)pktBuff[14]) << 8) + (uint16_t)pktBuff[13]);
        imu_mag_ut[1] = data2;//(int)((((uint16_t)pktBuff[16]) << 8) + (uint16_t)pktBuff[15]);
        imu_mag_ut[2] = data3;//(int)((((uint16_t)pktBuff[18]) << 8) + (uint16_t)pktBuff[17]);
        imu_mag_accuracy = status;
      }
      return (int)report_id;
    }
  }
  return -1;
}

//-----------------------------------------------------------------------------
////-----------------------------------------------------------------------------
//int getRotation(rotation_t *pRotation)
//{
//    // parse the IMU rotation vector input report
//    int numBytesAvail = 0;
//    char pktBuff[256] = {0};
//    char shtpHeader[4] = {0};
//    int fd = i2cOpen(BUS_IMU, ADDR_IMU, 0);
//
//    if (fd < 0) {
//        CETI_LOG("getRotation(): XXX Failed to connect to the IMU\n");
//        return -1;
//    }
//
//    CETI_LOG("getRotation(): IMU connection opened\n");
//
//    // Byte   0    1    2    3    4   5   6    7    8      9     10     11
//    //       |     HEADER      |            TIME       |  ID    SEQ   STATUS....
//    while (1) {
//        i2cReadDevice(fd, shtpHeader, 4);
//
//        // msb is "continuation bit, not part of count"
//        numBytesAvail = fmin(256, ((shtpHeader[1] << 8) | shtpHeader[0]) & 0x7FFF);
//
//        if (numBytesAvail) {
//            i2cReadDevice(fd, pktBuff, numBytesAvail);
//            // make sure we have the right channel
//            if (pktBuff[2] == 0x03) {
//                // testing, should come back 0x05
//                pRotation->reportID = pktBuff[9];
//                pRotation->sequenceNum = pktBuff[10];
//                CETI_LOG("getRotation(): report ID 0x%02X  sequ 0x%02X i "
//                       "%02X%02X j %02X%02X k %02X%02X r %02X%02X\n",
//                       pktBuff[9], pktBuff[10], pktBuff[14], pktBuff[13],
//                       pktBuff[16], pktBuff[15], pktBuff[18], pktBuff[17],
//                       pktBuff[20], pktBuff[19]);
//            }
//        } else {
//            setupIMU(); // restart the sensor, reports somehow stopped coming
//        }
//        usleep(800000);
//    }
//    i2cClose(fd);
//    return 0;
//}

//int learnIMU() {
//    char shtpHeader[4] = {0};
//    char shtpData[256] = {0};
//    int fd;
//    uint16_t numBytesAvail;
//    if ((fd = i2cOpen(BUS_IMU, ADDR_IMU, 0)) < 0) {
//        CETI_LOG("learnIMU(): Failed to connect to the IMU");
//        fprintf(stderr, "learnIMU(): XXX Failed to connect to the IMU\n");
//        return -1;
//    }
//
//    printf("learnIMU(): OK, have the IMU connected!\n");
//
//    // Get the shtp header
//    i2cReadDevice(fd, shtpHeader, 4); // pigpio i2c read raw
//    printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n",
//           shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);
//
//    // Figure out how many bytes are waiting
//    numBytesAvail = (shtpHeader[1] << 8 | shtpHeader[0]);
//    printf("learnIMU(): Bytes Avail are 0x%04X \n", numBytesAvail);
//
//    // Get all bytes that are waiting (flush it out)
//    while (numBytesAvail > 0) {
//        i2cReadDevice(fd, shtpData, numBytesAvail);
//        i2cReadDevice(fd, shtpHeader, 4);
//        printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n",
//               shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);
//    }
//
//    // Ask for product ID report
//    shtpData[0] = 0x06; // header - packet length LSB
//    shtpData[1] = 0x00; // header - packet length MSB
//    shtpData[2] = 0x02; // header - command channel
//    shtpData[3] = 0x00; // header - sequence no
//    shtpData[4] = IMU_SHTP_REPORT_PRODUCT_ID_REQUEST;
//    shtpData[5] = 0x00; // reserved
//
//    i2cWriteDevice(fd, shtpData, 6);
//
//    i2cReadDevice(fd, shtpHeader, 4);
//    printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n",
//           shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);
//
//    i2cClose(fd);
//
//    return 0;
//}