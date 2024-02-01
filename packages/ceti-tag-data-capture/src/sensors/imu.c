//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "imu.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
typedef enum imu_data_type_e {
  IMU_DATA_TYPE_QUAT,
  IMU_DATA_TYPE_ACCEL,
  IMU_DATA_TYPE_GYRO,
  IMU_DATA_TYPE_MAG,
  IMU_DATA_TYPE_COUNT,
}IMUDataType;

int g_imu_thread_is_running = 0;
#define IMU_MAX_FILEPATH_LENGTH 100
static char imu_data_filepath[IMU_DATA_TYPE_COUNT][IMU_MAX_FILEPATH_LENGTH];
static FILE *imu_data_file[IMU_DATA_TYPE_COUNT] = {
    [IMU_DATA_TYPE_QUAT] = NULL,
    [IMU_DATA_TYPE_ACCEL] = NULL,
    [IMU_DATA_TYPE_GYRO] = NULL,
    [IMU_DATA_TYPE_MAG] = NULL,
};
//These all compile to single continuous strings, aka why no comma between quotes.
static const char* imu_data_file_headers[IMU_DATA_TYPE_COUNT] = {
    [IMU_DATA_TYPE_QUAT] =
        "Sensor_delay_us"
        ",Quat_i"
        ",Quat_j"
        ",Quat_k"
        ",Quat_Re"
        ",Quat_accuracy",
    [IMU_DATA_TYPE_ACCEL] =
        "Sensor_delay_us"
        ",Accel_x_m_ss"
        ",Accel_y_m_ss"
        ",Accel_z_m_ss"
        ",Accel_accuracy",
    [IMU_DATA_TYPE_GYRO] =
        "Sensor_delay_us"
        ",Gyro_x_rad_s"
        ",Gyro_y_rad_s"
        ",Gyro_z_rad_s"
        ",Gyro_accuracy",
    [IMU_DATA_TYPE_MAG] =
        "Sensor_delay_us"
        ",Mag_x_uT"
        ",Mag_y_uT"
        ",Mag_z_uT"
        ",Mag_accuracy",
};
static const char* imu_data_names[IMU_DATA_TYPE_COUNT] = {
    [IMU_DATA_TYPE_QUAT] = "quat",
    [IMU_DATA_TYPE_ACCEL] = "accel",
    [IMU_DATA_TYPE_GYRO] = "gyro",
    [IMU_DATA_TYPE_MAG] = "mag",
};
static int imu_is_connected = 0;
static uint8_t imu_sequence_numbers[6] = {0}; // Each of the 6 channels has a sequence number (a message counter)
static Quaternion_i16 imu_quaternion; // i, j, k, real
static int16_t imu_accel_m_ss[3]; // x, y, z
static int16_t imu_gyro_rad_s[3]; // x, y, z
static int16_t imu_mag_ut[3];     // x, y, z
static int16_t imu_quaternion_accuracy; // rad
static int16_t imu_accel_accuracy; // a rating of 0-3
static int16_t imu_gyro_accuracy;  // a rating of 0-3
static int16_t imu_mag_accuracy;   // a rating of 0-3
static long imu_reading_delay_us = 0; // delay from sensor reading to data transmission
static bool imu_restarted_log = true;
static bool imu_new_log = true;

int init_imu() {
  if (setupIMU(IMU_ALL_ENABLED) < 0) {
    CETI_ERR("Failed to set up the IMU");
    return -1;
  }


  imu_new_log = true;
  // Open an output file to write data.
  if(imu_init_data_files() < 0)
    return -1;
  CETI_LOG("Successfully initialized the IMU");

  return 0;
}

//Find next available filename
int imu_init_data_files(void) {
  // Append a number to the filename base until one is found that doesn't exist yet.
  static int data_file_postfix_count = 0; //static to retain number last file index
  int data_file_exists = 0;

  do {
    data_file_exists = 0;
    //check that all filenames don't already exist
    for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT && !data_file_exists; i_type++) {
      snprintf(imu_data_filepath[i_type], IMU_MAX_FILEPATH_LENGTH, IMU_DATA_FILEPATH_BASE "_%s_%02d.csv", imu_data_names[i_type], data_file_postfix_count);
      data_file_exists = (access(imu_data_filepath[i_type], F_OK) != -1);
    }
    data_file_postfix_count++;
  } while(data_file_exists);

  int init_data_file_success = 0;
  for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
    // Open the new files
    init_data_file_success |= init_data_file(imu_data_file[i_type], imu_data_filepath[i_type],
                                                &imu_data_file_headers[i_type],  1,
                                                NULL, "imu_init_data_files()");
  }
  imu_restarted_log = true;
  return init_data_file_success;
}

//close all imu data files
void imu_close_all_files(void) {
  for (int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
    if (imu_data_file[i] != NULL) {
      fclose(imu_data_file[i]);
      imu_data_file[i] = NULL;
    }
  }
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *imu_thread(void *paramPtr) {
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_imu_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(IMU_CPU >= 0) {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(IMU_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("Successfully set affinity to CPU %d", IMU_CPU);
      else
        CETI_ERR("Failed to set affinity to CPU %d", IMU_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    IMUDataType report_type;
    int report_id_updated = -1;
    long long global_time_us = get_global_time_us();
    long long start_global_time_us = get_global_time_us();
    int rtc_count;
    long long imu_last_data_file_flush_time_us = get_global_time_us();
    g_imu_thread_is_running = 1;
    //open all files
    for(int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++)
      imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");

    while (!g_stopAcquisition) {
      //check that all imu data files are open
      int unopened_file = false;
      for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT && !unopened_file; i_type++) {
        if (imu_data_file[i_type] == NULL) {
          CETI_ERR("Failed to open data output file: %s", imu_data_filepath[i_type]);
          imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");
          unopened_file = (imu_data_file[i_type] == NULL);
        }
      }
      if (unopened_file) {
        // A file failed to open
        usleep(100000); // Sleep a bit before retrying.
        continue; //restart loop
      }

      if (!imu_is_connected){
        // If there was an error, try to reset the IMU.
        // Ignore the initial few seconds though, since sometimes it takes a little bit to get in sync.
        if (get_global_time_us() - start_global_time_us > 10000000) {
          CETI_ERR("Unable to connect to IMU");
          usleep(5000000);
          setupIMU(IMU_ALL_ENABLED);
          start_global_time_us = get_global_time_us();
        }
        continue; //restart loop
      }

      report_id_updated = imu_read_data();
      
      // no data received yet
      if(report_id_updated == -1) {
        // timeout reached
        if((get_global_time_us() - global_time_us > 5000000) && (get_global_time_us() - start_global_time_us > 10000000)) {
          CETI_ERR("Unable to reading from IMU");
          usleep(5000000);
          setupIMU(IMU_ALL_ENABLED);
          start_global_time_us = get_global_time_us();
        }
        usleep(2000); // Note that we are streaming 4 reports at 50 Hz, so we expect data to be available at about 200 Hz
        continue; //restart loop
      }

      // Acquire timing information for this sample.
      global_time_us = get_global_time_us();
      rtc_count = getRtcCount();

      // Check whether data was actually received,
      //  or whether the loop exited due to other conditions.
      if(report_id_updated < 0) {
        continue;  //restart loop
      }

      switch (report_id_updated){
        case IMU_SENSOR_REPORTID_ROTATION_VECTOR: report_type = IMU_DATA_TYPE_QUAT; break;
        case IMU_SENSOR_REPORTID_ACCELEROMETER: report_type = IMU_DATA_TYPE_ACCEL; break;
        case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: report_type = IMU_DATA_TYPE_GYRO; break;
        case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: report_type = IMU_DATA_TYPE_MAG; break;
        default: report_type = -1; break;
      }

      if(report_type == -1){
        //unknown report type
        CETI_WARN("Unknown IMU report type: 0x%02x", report_id_updated);
        continue;
      }

      FILE *cur_file = imu_data_file[report_type];
      // Write timing information.
      fprintf(cur_file, "%lld", global_time_us);
      fprintf(cur_file, ",%d", rtc_count);
      // Write any notes, then clear them so they are only written once.
      fprintf(cur_file, ","); //notes seperator
      if (imu_restarted_log) {
        fprintf(cur_file, "Restarted |");
        imu_restarted_log = false;
      }
      if (imu_new_log) {
        fprintf(cur_file, "New log file! | ");
        imu_new_log = false;
      }

      // Write the sensor reading delay.
      fprintf(cur_file, ",%ld", imu_reading_delay_us);

      switch (report_id_updated) {
        case IMU_SENSOR_REPORTID_ROTATION_VECTOR: 
          // Write quaternion data
          fprintf(cur_file, ",%d", imu_quaternion.i);
          fprintf(cur_file, ",%d", imu_quaternion.j);
          fprintf(cur_file, ",%d", imu_quaternion.k);
          fprintf(cur_file, ",%d", imu_quaternion.real);
          fprintf(cur_file, ",%d", imu_quaternion_accuracy);
          break;

        case IMU_SENSOR_REPORTID_ACCELEROMETER: 
          // Write accelerometer data
          fprintf(cur_file, ",%d", imu_accel_m_ss[0]);
          fprintf(cur_file, ",%d", imu_accel_m_ss[1]);
          fprintf(cur_file, ",%d", imu_accel_m_ss[2]);
          fprintf(cur_file, ",%d", imu_accel_accuracy);
          break;

        case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: 
          // Write gyroscope data
          fprintf(cur_file, ",%d", imu_gyro_rad_s[0]);
          fprintf(cur_file, ",%d", imu_gyro_rad_s[1]);
          fprintf(cur_file, ",%d", imu_gyro_rad_s[2]);
          fprintf(cur_file, ",%d", imu_gyro_accuracy);
          break;

        case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED:
          // Write magnetometer data
          fprintf(cur_file, ",%d", imu_mag_ut[0]);
          fprintf(cur_file, ",%d", imu_mag_ut[1]);
          fprintf(cur_file, ",%d", imu_mag_ut[2]);
          fprintf(cur_file, ",%d", imu_mag_accuracy);
          break;
        
        default:
          break;
      }
      fprintf(cur_file, "\n");

      // Flush the files periodically.
      if ((get_global_time_us() - imu_last_data_file_flush_time_us >= IMU_DATA_FILE_FLUSH_PERIOD_US)) {
        size_t imu_data_file_size_b = 0;
        // Check the file sizes and close the files.
        for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
          fseek(imu_data_file[i_type], 0L, SEEK_END);
          size_t i_size = ftell(imu_data_file[i_type]);
          fflush(imu_data_file[i_type]);
          if (i_size > imu_data_file_size_b)
            imu_data_file_size_b = i_size;
        }

        // If any file size limit has been reached, start a new file.
        if ((imu_data_file_size_b >= (long)(IMU_MAX_FILE_SIZE_MB)*1024L*1024L || imu_data_file_size_b < 0)  && !g_stopAcquisition){
          imu_close_all_files();
          imu_init_data_files();

          // Open the files again.
          for (int i_type = 0; i_type < IMU_DATA_TYPE_COUNT; i_type++) {
            imu_data_file[i_type] = fopen(imu_data_filepath[i_type], "at");
          }
        }

        // Record the flush time.
        imu_last_data_file_flush_time_us = get_global_time_us();
      }

      // Note that no delay is added here to set the polling frequency,
      //  since the IMU feature reports will control the sampling rate.
    }
    resetIMU(); // seems nice to stop the feature reports
    bbI2CClose(IMU_BB_I2C_SDA);
    imu_is_connected = 0;
    imu_close_all_files();
    g_imu_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// BNO080 Interface
//-----------------------------------------------------------------------------

int resetIMU() {
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
int setupIMU(uint8_t enabled_features) {
  // Reset the IMU
  if (imu_is_connected)
    bbI2CClose(IMU_BB_I2C_SDA);
  imu_is_connected = 0;
  usleep(10000);
  resetIMU();

  // Open an I2C connection.
  int retval = bbI2COpen(IMU_BB_I2C_SDA, IMU_BB_I2C_SCL, 200000);
  if (retval < 0) {
      CETI_ERR("Failed to connect to the IMU\n");
      imu_is_connected = 0;
      return -1;
  }
  imu_is_connected = 1;
  CETI_LOG("IMU connection opened\n");

  // Reset the message counters for each channel.
  for(int channel_index = 0; channel_index < sizeof(imu_sequence_numbers)/sizeof(uint8_t); channel_index++)
    imu_sequence_numbers[channel_index] = 0;

  // Enable desired feature reports.
  if(enabled_features & IMU_QUAT_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_ROTATION_VECTOR, 1000000);
    usleep(100000);
  }
  
  if(enabled_features & IMU_ACCEL_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_ACCELEROMETER, IMU_SAMPLING_PERIOD_9DOF_US);
    usleep(100000);
  }
  if(enabled_features & IMU_GYRO_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED, IMU_SAMPLING_PERIOD_9DOF_US);
    usleep(100000);
  }
  if(enabled_features & IMU_MAG_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED, IMU_SAMPLING_PERIOD_9DOF_US);
    usleep(100000);
  }

  return 0;
}

int imu_enable_feature_report(int report_id, uint32_t report_interval_us) {
  char setFeatureCommand[21] = {0};
  char shtpHeader[4] = {0};
  char writeCmdBuf[28] = {0};
  char readCmdBuf[10] = {0};

  uint16_t feature_command_length = 21;

  // Populate the SHTP header (see 2.2.1 of "Sensor Hub Transport Protocol")
  setFeatureCommand[0] = feature_command_length & 0xFF; // Packet length LSB
  setFeatureCommand[1] = feature_command_length >> 8;   // Packet length MSB
  setFeatureCommand[2] = IMU_CHANNEL_CONTROL;
  setFeatureCommand[3] = imu_sequence_numbers[IMU_CHANNEL_CONTROL]++; // sequence number for this channel

  // Populate the Set Feature Command (see 6.5.4 of "SH-2 Reference Manual")
  setFeatureCommand[4] = IMU_SHTP_REPORT_SET_FEATURE_COMMAND;
  setFeatureCommand[5] = report_id;
  setFeatureCommand[6] = 0; // feature flags
  setFeatureCommand[7] = 0; // change sensitivity LSB
  setFeatureCommand[8] = 0; // change sensitivity MSB
  // Set the report interval in microseconds, as 4 bytes with LSB first
  for (int interval_byte_index = 0; interval_byte_index < 4; interval_byte_index++)
    setFeatureCommand[9 + interval_byte_index] = (report_interval_us >> (8 * interval_byte_index)) & 0xFF;
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
      CETI_ERR("I2C write failed enabling report %d: %d", report_id, retval);
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
      CETI_ERR("I2C read failed enabling report %d: %d", report_id, retval);
      return -1;
  }
  CETI_LOG("Enabled report %d.  Header is 0x%02X  0x%02X  0x%02X  0x%02X",
           report_id, shtpHeader[0], shtpHeader[1], shtpHeader[2], shtpHeader[3]);

  return 0;
}

//-----------------------------------------------------------------------------
int imu_read_data() {
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

  if (retval > 0 && numBytesAvail > 0) {
    // Adjust the read command for the amount of data available.
    commandBuffer[5] = numBytesAvail & 0xff;        // LSB
    commandBuffer[6] = (numBytesAvail >> 8) & 0xff; // MSB
    // Read the data.
    retval = bbI2CZip(IMU_BB_I2C_SDA, commandBuffer, 9, pktBuff, numBytesAvail);
    // Parse the data.
    if (retval > 0 && pktBuff[2] == IMU_CHANNEL_REPORTS) { // make sure we have the right channel
      uint8_t report_id = pktBuff[9];
      uint32_t timestamp_delay_us = ((uint32_t)pktBuff[8] << (8 * 3)) | ((uint32_t)pktBuff[7] << (8 * 2)) | ((uint32_t)pktBuff[6] << (8 * 1)) | ((uint32_t)pktBuff[5] << (8 * 0));
      imu_reading_delay_us = (int32_t)timestamp_delay_us;
      // uint8_t sequence_number = pktBuff[10];
      // CETI_LOG("IMU REPORT %d  seq %3d  timestamp_delay_us %ld\n",
      //   report_id, sequence_number, (int32_t)timestamp_delay_us);
      // for (int i = 0; i < 4; i++)
      //   CETI_LOG("    H%2d: %d\n", i, shtpHeader[i]);
      // for (int i = 0; i < numBytesAvail; i++)
      //   CETI_LOG("    D%2d: %d\n", i, pktBuff[i]);

      // Parse the data in the report.
      uint8_t status = pktBuff[11] & 0x03; // Get status bits
      uint16_t data[3] = {
        ((uint16_t)pktBuff[14] << 8 | pktBuff[13]),
        ((uint16_t)pktBuff[16] << 8 | pktBuff[15]),
        ((uint16_t)pktBuff[18] << 8 | pktBuff[17])
      };

      switch(report_id){
        case IMU_SENSOR_REPORTID_ROTATION_VECTOR: {
          memcpy(&imu_quaternion, data, 3*sizeof(int16_t));
          imu_quaternion.real = (uint16_t)pktBuff[20] << 8 | pktBuff[19];
          imu_quaternion_accuracy = (uint16_t)pktBuff[22] << 8 | pktBuff[21];
          break;
        }
        case IMU_SENSOR_REPORTID_ACCELEROMETER: {
          memcpy(imu_accel_m_ss, data, 3*sizeof(int16_t));
          imu_accel_accuracy = status;
          break;
        }
        case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: {
          memcpy(imu_gyro_rad_s, data, 3*sizeof(int16_t));
          imu_gyro_accuracy = status;
          break;
        }
        case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: {
          memcpy(imu_mag_ut, data, 3*sizeof(int16_t));
          imu_mag_accuracy = status;
          break;
        }
      return (int)report_id;
    }
  }
  return -1;
}

void quat2eul(EulerAngles_f64 *e, Quaternion_i16 *q){
  double re = ((double) q->real) / (1 << 14);
  double i = ((double) q->i) / (1 << 14);
  double j = ((double) q->j) / (1 << 14);
  double k = ((double) q->k) / (1 << 14);

  double sinr_cosp = 2 * ((re * i) + (j * k));
  double cosr_cosp = 1 - 2 * ((i * i) + (j * j));
  e->pitch = atan2(sinr_cosp, cosr_cosp);

  double sinp = sqrt(1 + 2 * ((re * j) - (i * k)));
  double cosp = sqrt(1 - 2 * ((re * j) - (i * k)));
  e->roll = (2.0 * atan2(sinp, cosp)) - (M_PI / 2.0);

  double siny_cosp = 2 * ((re * k) + (i * j));
  double cosy_cosp = 1 - 2 * ((j * j) + (k * k));
  e->yaw = atan2(siny_cosp, cosy_cosp);
}

int imu_get_euler_angles(EulerAngles_f64 *e){
  int report_id_updated = imu_read_data();
  if (report_id_updated != IMU_SENSOR_REPORTID_ROTATION_VECTOR){
    return -1;
  } 

  quat2eul(e, &imu_quaternion); 
  return 0;
}
