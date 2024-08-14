//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "imu.h"
#include "../cetiTag.h"
#include "../utils/memory.h"

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>

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
static bool imu_restarted_log[IMU_DATA_TYPE_COUNT] = {true, true, true, true};
static bool imu_new_log[IMU_DATA_TYPE_COUNT] = {true, true, true, true};

// ToDo: consider buffering samples for slower read. 
// latest sample pointers (shared memory)
static CetiImuQuatSample *imu_quaternion;
static CetiImuAccelSample *imu_accel_m_ss;
static CetiImuGyroSample *imu_gyro_rad_s;
static CetiImuMagSample *imu_mag_ut;

// semaphore to indicate that shared memory has bee updated
static sem_t *s_quat_ready;
static sem_t *s_accel_ready;
static sem_t *s_gyro_ready;
static sem_t *s_mag_ready;

int init_imu() {
  if (setupIMU(IMU_ALL_ENABLED) < 0) {
    CETI_ERR("Failed to set up the IMU");
    return -1;
  }

  for(int i = 0; i < IMU_DATA_TYPE_COUNT; i++) {
    imu_new_log[i] = true;
  }

  //setup shared memory regions
  imu_quaternion = create_shared_memory_region(IMU_QUAT_SHM_NAME, sizeof(CetiImuQuatSample));
  imu_accel_m_ss = create_shared_memory_region(IMU_ACCEL_SHM_NAME, sizeof(CetiImuAccelSample));
  imu_gyro_rad_s = create_shared_memory_region(IMU_GYRO_SHM_NAME, sizeof(CetiImuGyroSample));
  imu_mag_ut     = create_shared_memory_region(IMU_MAG_SHM_NAME, sizeof(CetiImuMagSample));

  //setup semaphores
  s_quat_ready = sem_open(IMU_QUAT_SEM_NAME, O_CREAT, 0644, 0);
  if(s_quat_ready == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create quaternion semaphore");
    return -1;
  }
  s_accel_ready = sem_open(IMU_ACCEL_SEM_NAME, O_CREAT, 0644, 0);
  if(s_accel_ready == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create acceleration semaphore");
    return -1;
  }
  s_gyro_ready = sem_open(IMU_GYRO_SEM_NAME, O_CREAT, 0644, 0);
  if(s_gyro_ready == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create gyroscope semaphore");
    return -1;
  }
  s_mag_ready = sem_open(IMU_MAG_SEM_NAME, O_CREAT, 0644, 0);
  if(s_mag_ready == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create magnetometer semaphore");
    return -1;
  }

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
  for(int i = 0 ; i < IMU_DATA_TYPE_COUNT; i++){
    imu_restarted_log[i] = true;
  }
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
void imu_quat_sample_to_csv(FILE *fp, CetiImuQuatSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
    fprintf(fp, ","); //notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_QUAT]) {
      fprintf(fp, "Restarted |");
      imu_restarted_log[IMU_DATA_TYPE_QUAT] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_QUAT]) {
      fprintf(fp, "New log file! | ");
      imu_new_log[IMU_DATA_TYPE_QUAT] = false;
    }
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    fprintf(fp, ",%d", pSample->i);
    fprintf(fp, ",%d", pSample->j);
    fprintf(fp, ",%d", pSample->k);
    fprintf(fp, ",%d", pSample->real);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void imu_accel_sample_to_csv(FILE *fp, CetiImuAccelSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); //notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_ACCEL]) {
      fprintf(fp, "Restarted |");
      imu_restarted_log[IMU_DATA_TYPE_ACCEL] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_ACCEL]) {
      fprintf(fp, "New log file! | ");
      imu_new_log[IMU_DATA_TYPE_ACCEL] = false;
    }
    // Write the sensor reading delay.
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    // Write accelerometer data
    fprintf(fp, ",%d", pSample->x);
    fprintf(fp, ",%d", pSample->y);
    fprintf(fp, ",%d", pSample->z);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void imu_gyro_sample_to_csv(FILE *fp, CetiImuGyroSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); //notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_GYRO]) {
      fprintf(fp, "Restarted |");
      imu_restarted_log[IMU_DATA_TYPE_GYRO] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_GYRO]) {
      fprintf(fp, "New log file! | ");
      imu_new_log[IMU_DATA_TYPE_GYRO] = false;
    }
    // Write the sensor reading delay.
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    // Write accelerometer data
    fprintf(fp, ",%d", pSample->x);
    fprintf(fp, ",%d", pSample->y);
    fprintf(fp, ",%d", pSample->z);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

void imu_mag_sample_to_csv(FILE *fp, CetiImuMagSample *pSample) {
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ","); //notes seperator
    if (imu_restarted_log[IMU_DATA_TYPE_MAG]) {
      fprintf(fp, "Restarted |");
      imu_restarted_log[IMU_DATA_TYPE_MAG] = false;
    }
    if (imu_new_log[IMU_DATA_TYPE_MAG]) {
      fprintf(fp, "New log file! | ");
      imu_new_log[IMU_DATA_TYPE_MAG] = false;
    }
    // Write the sensor reading delay.
    fprintf(fp, ",%ld", pSample->reading_delay_us);
    // Write accelerometer data
    fprintf(fp, ",%d", pSample->x);
    fprintf(fp, ",%d", pSample->y);
    fprintf(fp, ",%d", pSample->z);
    fprintf(fp, ",%d", pSample->accuracy);
    fprintf(fp, "\n");
}

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
      
      // A file failed to open
      if (unopened_file) {
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

      //buffer data for other applications
      switch(report_type) {
        case IMU_DATA_TYPE_QUAT: {
            //copy data to shared buffer
            imu_quat_sample_to_csv(imu_data_file[IMU_DATA_TYPE_QUAT], imu_quaternion);
            break;
          }

          case IMU_DATA_TYPE_ACCEL: {
            //ToDo: buffer samples?
            imu_accel_sample_to_csv(imu_data_file[IMU_DATA_TYPE_ACCEL], imu_accel_m_ss);
          }
          break;
        case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: {
            //ToDo: buffer samples?
            imu_gyro_sample_to_csv(imu_data_file[IMU_DATA_TYPE_GYRO], imu_gyro_rad_s);
          }
          break;

        case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: {
            //ToDo: buffer samples?
            imu_mag_sample_to_csv(imu_data_file[IMU_DATA_TYPE_MAG], imu_mag_ut);
          }
          break;

        default:
          break;
      }

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
    
    sem_close(s_quat_ready);
    sem_close(s_accel_ready);
    sem_close(s_gyro_ready);
    sem_close(s_mag_ready);

    munmap(imu_quaternion, sizeof(CetiImuQuatSample));
    munmap(imu_accel_m_ss, sizeof(CetiImuAccelSample));
    munmap(imu_gyro_rad_s, sizeof(CetiImuGyroSample));
    munmap(imu_mag_ut, sizeof(CetiImuMagSample));
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
    imu_enable_feature_report(IMU_SENSOR_REPORTID_ROTATION_VECTOR, IMU_QUATERNION_SAMPLE_PERIOD_US);
    usleep(100000);
  }
  
  if(enabled_features & IMU_ACCEL_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_ACCELEROMETER, IMU_9DOF_SAMPLE_PERIOD_US);
    usleep(100000);
  }
  if(enabled_features & IMU_GYRO_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED, IMU_9DOF_SAMPLE_PERIOD_US);
    usleep(100000);
  }
  if(enabled_features & IMU_MAG_ENABLED) {
    imu_enable_feature_report(IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED, IMU_9DOF_SAMPLE_PERIOD_US);
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

  if ((retval <= 0) || (numBytesAvail <= 0))
    return -1;
  
  // Adjust the read command for the amount of data available.
  commandBuffer[5] = numBytesAvail & 0xff;        // LSB
  commandBuffer[6] = (numBytesAvail >> 8) & 0xff; // MSB
  // Read the data.
  retval = bbI2CZip(IMU_BB_I2C_SDA, commandBuffer, 9, pktBuff, numBytesAvail);
  // Parse the data.
  if ((retval <= 0) || pktBuff[2] != IMU_CHANNEL_REPORTS) { // make sure we have the right channel
    return -1;
  }

  long long global_time_us = get_global_time_us();
  int rtc_count = getRtcCount();
  uint8_t report_id = pktBuff[9];
  int32_t timestamp_delay_us = (int32_t)(((uint32_t)pktBuff[8] << (8 * 3)) | ((uint32_t)pktBuff[7] << (8 * 2)) | ((uint32_t)pktBuff[6] << (8 * 1)) | ((uint32_t)pktBuff[5] << (8 * 0)));
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
      imu_quaternion->sys_time_us = global_time_us;
      imu_quaternion->rtc_time_s = rtc_count;
      imu_quaternion->reading_delay_us = timestamp_delay_us;
      memcpy(&imu_quaternion->i, data, 3*sizeof(int16_t));
      imu_quaternion->real = (uint16_t)pktBuff[20] << 8 | pktBuff[19];
      imu_quaternion->accuracy = (uint16_t)pktBuff[22] << 8 | pktBuff[21];
      sem_post(s_quat_ready);
      break;
    }
    case IMU_SENSOR_REPORTID_ACCELEROMETER: {
      imu_accel_m_ss->sys_time_us = global_time_us;
      imu_accel_m_ss->rtc_time_s = rtc_count;
      imu_accel_m_ss->reading_delay_us = timestamp_delay_us;
      memcpy(&imu_accel_m_ss->x, data, 3*sizeof(int16_t));
      imu_accel_m_ss->accuracy = status;
      sem_post(s_accel_ready);
      break;
    }
    case IMU_SENSOR_REPORTID_GYROSCOPE_CALIBRATED: {
      imu_gyro_rad_s->sys_time_us = global_time_us;
      imu_gyro_rad_s->rtc_time_s = rtc_count;
      imu_gyro_rad_s->reading_delay_us = timestamp_delay_us;
      memcpy(&imu_gyro_rad_s->x, data, 3*sizeof(int16_t));
      imu_gyro_rad_s->accuracy = status;
      sem_post(s_gyro_ready);
      break;
    }
    case IMU_SENSOR_REPORTID_MAGNETIC_FIELD_CALIBRATED: {
      imu_mag_ut->sys_time_us = global_time_us;
      imu_mag_ut->rtc_time_s = rtc_count;
      imu_mag_ut->reading_delay_us = timestamp_delay_us;
      memcpy(&imu_mag_ut->x, data, 3*sizeof(int16_t));
      imu_mag_ut->accuracy = status;
      sem_post(s_mag_ready);
      break;
    }
  }
  return (int)report_id;
}
