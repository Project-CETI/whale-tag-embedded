//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//              Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
// === Private Local Headers ===
#include "battery.h"
#include "cetiTag.h"
#include "utils/memory.h"
 
// === Private System Libraries ===
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

//-----------------------------------------------------------------------------
// Global/static variables
//-----------------------------------------------------------------------------
int g_battery_thread_is_running = 0;
static FILE* battery_data_file = NULL;
static char battery_data_file_notes[256] = "";
static const char* battery_data_file_headers[] = {
  "Battery V1 [V]",
  "Battery V2 [V]",
  "Battery I [mA]",
  "Battery T1 [C]",
  "Battery T2 [C]",
  "Status",
  "Protection Alerts",
  };
static const int num_battery_data_file_headers = sizeof(battery_data_file_headers)/sizeof(*battery_data_file_headers);
CetiBatterySample *g_battery = NULL;
static sem_t* s_battery_data_ready;
static int charging_disabled, discharging_disabled;
// Store global versions of the latest readings since the state machine will use them.
#if MAX17320 == 1
  MAX17320_HandleTypeDef dev;
#endif



//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_battery() {
  
  #if MAX17320 == 1
    int ret = max17320_init(&dev);  
    if (ret < 0){
      CETI_ERR("Failed to connect to MAX17320 Fuel Gauge");
      return ret;
    }
    #else

    int fd;
    if((fd=i2cOpen(1,ADDR_BATT_GAUGE,0)) < 0) {
      CETI_ERR("Failed to connect to the battery gauge");
      return (-1);
    }
    else {
      i2cWriteByteData(fd,BATT_CTL,BATT_CTL_VAL); //establish undervoltage cutoff
      i2cWriteByteData(fd,BATT_OVER_VOLTAGE,BATT_OV_VAL); //establish undervoltage cutoff
    }
  #endif  
  // Open an output file to write data.
  CETI_LOG("Successfully initialized the battery gauge");
  if(init_data_file(battery_data_file, BATTERY_DATA_FILEPATH,
                    battery_data_file_headers,  num_battery_data_file_headers,
                    battery_data_file_notes, "init_battery()") < 0)
    return -1;

  //setup shared memory
  g_battery = create_shared_memory_region(BATTERY_SHM_NAME, sizeof(CetiBatterySample));

  //setup semaphore
  s_battery_data_ready = sem_open(BATTERY_SEM_NAME, O_CREAT, 0644, 0);
  if(s_battery_data_ready == SEM_FAILED){
    perror("sem_open");
    CETI_ERR("Failed to create semaphore");
    return -1;
  }
  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------

// PA | POR | dSOCi | Imn | Imx | Vmn | Vmx | Tmn | Tmx | Smn | Smx
static char status_string[72] = "";
static const char *__status_to_str(uint16_t raw) {
    static uint16_t previous_status = 0;
    char* flags [11] = {};
    int flag_count = 0;

    // mask do not care bits
    raw &= 0xF7C6;

    //don't do work we don't have to do
    if (raw == previous_status) {
      return status_string;
    }

    // check if no flags
    if (raw == 0) {
      status_string[0] = '\0';
      previous_status = 0;
      return status_string;
    }

    if(_RSHIFT(raw, 15, 1)) flags[flag_count++] = "PA";
    if(_RSHIFT(raw, 1, 1)) flags[flag_count++] = "POR";
    if(_RSHIFT(raw, 7, 1)) flags[flag_count++] = "dSOCi";
    if(_RSHIFT(raw, 2, 1)) flags[flag_count++] = "Imn";
    if(_RSHIFT(raw, 6, 1)) flags[flag_count++] = "Imx";
    if(_RSHIFT(raw, 8, 1)) flags[flag_count++] = "Vmn";
    if(_RSHIFT(raw, 12, 1)) flags[flag_count++] = "Vmx";
    if(_RSHIFT(raw, 9, 1)) flags[flag_count++] = "Tmn";
    if(_RSHIFT(raw, 13, 1)) flags[flag_count++] = "Tmx";
    if(_RSHIFT(raw, 10, 1)) flags[flag_count++] = "Smn";
    if(_RSHIFT(raw, 14, 1)) flags[flag_count++] = "Smx";

    // generate string
    for(int j = 0; j < flag_count; j++){
        if (j != 0) {
            strncat(status_string, " | ", sizeof(status_string)-1);
        }
        strncat(status_string, flags[j], sizeof(status_string)-1);
    }
    previous_status = raw;

    return status_string;
}

// ChgWDT | TooHotC | Full | TooColdC | OVP | OCCP | Qovflw | PrepF | Imbalance | PermFail | DieHot | TooHotD | UVP | ODCP | ResDFault | LDet
static char protAlrt_string[160] = "";
static const char *__protAlrt_to_str(uint16_t raw) {
    static uint16_t previous_protAlrt = 0;
    char* flags [16] = {};
    int flag_count = 0;

    //don't do work we don't have to do
    if (raw == previous_protAlrt) {
      return protAlrt_string;
    }

    // check if no flags
    if (raw == 0) {
      protAlrt_string[0] = '\0';
      previous_protAlrt = 0;
      return protAlrt_string;
    }

    if(_RSHIFT(raw, 15, 1)) flags[flag_count++] = "ChgWDT";
    if(_RSHIFT(raw, 14, 1)) flags[flag_count++] = "TooHotC";
    if(_RSHIFT(raw, 13, 1)) flags[flag_count++] = "Full";
    if(_RSHIFT(raw, 12, 1)) flags[flag_count++] = "TooColdC";
    if(_RSHIFT(raw, 11, 1)) flags[flag_count++] = "OVP";
    if(_RSHIFT(raw, 10, 1)) flags[flag_count++] = "OCCP";
    if(_RSHIFT(raw, 9, 1)) flags[flag_count++] = "Qovflw";
    if(_RSHIFT(raw, 8, 1)) flags[flag_count++] = "PrepF";
    if(_RSHIFT(raw, 7, 1)) flags[flag_count++] = "Imbalance";
    if(_RSHIFT(raw, 6, 1)) flags[flag_count++] = "PermFail";
    if(_RSHIFT(raw, 5, 1)) flags[flag_count++] = "DieHot";
    if(_RSHIFT(raw, 4, 1)) flags[flag_count++] = "TooHotD";
    if(_RSHIFT(raw, 3, 1)) flags[flag_count++] = "UVP";
    if(_RSHIFT(raw, 2, 1)) flags[flag_count++] = "ODCP";
    if(_RSHIFT(raw, 1, 1)) flags[flag_count++] = "ResDFault";
    if(_RSHIFT(raw, 0, 1)) flags[flag_count++] = "LDet";

    // generate string
    for(int j = 0; j < flag_count; j++){
        if (j != 0) {
            strncat(protAlrt_string, " | ", sizeof(protAlrt_string)-1);
        }
        strncat(protAlrt_string, flags[j], sizeof(protAlrt_string)-1);
    }
    previous_protAlrt = raw;

    return protAlrt_string;
}

void battery_update_sample(void) {
    // create sample 
    g_battery->sys_time_us = get_global_time_us();
    g_battery->rtc_time_s = getRtcCount();
    g_battery->error = getBatteryData(&g_battery->cell_voltage_v[0], &g_battery->cell_voltage_v[1], &g_battery->current_mA);
    if(!g_battery->error) g_battery->error = max17320_get_cell_temperature_c(0, &g_battery->cell_temperature_c[0]);
    if(!g_battery->error) g_battery->error = max17320_get_cell_temperature_c(1, &g_battery->cell_temperature_c[1]);
    if(!g_battery->error) g_battery->error = max17320_read(MAX17320_REG_STATUS, &g_battery->status);
    if(!g_battery->error) g_battery->error = max17320_read(MAX17320_REG_PROTALRT, &g_battery->protection_alert);

    // push semaphore to indicate to user applications that new data is available
    sem_post(s_battery_data_ready);
}

void battery_sample_to_csv(FILE *fp, CetiBatterySample *pSample){
      if (pSample->error != WT_OK) {
        strcat(battery_data_file_notes, "ERROR | ");
      }
      if (pSample->cell_voltage_v[0] < 0 
          || pSample->cell_voltage_v[1] < 0
          || pSample->cell_temperature_c[0] < -80
          || pSample->cell_temperature_c[1] < -80
      ) { // it seems to return -0.01 for voltages and -5.19 for current when no sensor is connected
        CETI_WARN("readings are likely invalid");
        strcat(battery_data_file_notes, "INVALID? | ");
      }

        // Write timing information.
      fprintf(fp, "%ld", pSample->sys_time_us);
      fprintf(fp, ",%d", pSample->rtc_time_s);
      // Write any notes, then clear them so they are only written once.
      fprintf(fp, ",%s", battery_data_file_notes);
      strcpy(battery_data_file_notes, "");
      // Write the sensor data.
      fprintf(fp, ",%.3f", pSample->cell_voltage_v[0]);
      fprintf(fp, ",%.3f", pSample->cell_voltage_v[1]);
      fprintf(fp, ",%.3f", pSample->current_mA);
      fprintf(fp, ",%.3f", pSample->cell_temperature_c[0]);
      fprintf(fp, ",%.3f", pSample->cell_temperature_c[1]);
      fprintf(fp, ",%s", __status_to_str(pSample->status));
      fprintf(fp, ",%s", __protAlrt_to_str(pSample->protection_alert));

      // Finish the row of data and close the file.
      fprintf(fp, "\n");
}

void* battery_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_battery_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(BATTERY_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(BATTERY_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("Successfully set affinity to CPU %d", BATTERY_CPU);
      else
        CETI_ERR("Failed to set affinity to CPU %d", BATTERY_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    long long polling_sleep_duration_us;
    g_battery_thread_is_running = 1;
    while (!g_stopAcquisition) {     
      battery_update_sample();

    // ******************   Battery Temperature Checks *************************
    for(int i_cell = 0; i_cell < 2; i_cell++){
        if( (g_battery->cell_temperature_c[i_cell] > MAX_CHARGE_TEMP) ||  (g_battery->cell_temperature_c[i_cell] < MIN_CHARGE_TEMP) ) {          
            if (!charging_disabled){  
              disableCharging();
              charging_disabled = 1;
              CETI_WARN("Battery charging disabled, cell %d outside thermal limits: %.3f C", i_cell + 1, g_battery->cell_temperature_c[i_cell]);
            }
        }

        if( (g_battery->cell_temperature_c[i_cell] > MAX_DISCHARGE_TEMP) ) {
            if (!discharging_disabled){  
              disableDischarging();
              discharging_disabled = 1;
              CETI_WARN("Battery discharging disabled, cell %d outside thermal limit: %.3f C", i_cell + 1, g_battery->cell_temperature_c[i_cell]);
            }     
        }
    }

    
      // ******************   End Battery Temperature Checks *********************


      /* ToDo: move to seperate logging app. 
       * Daemon should only handle sample acq not storage
       */
      battery_data_file = fopen(BATTERY_DATA_FILEPATH, "at");
      if(battery_data_file == NULL) {
        CETI_WARN("failed to open data output file: %s", BATTERY_DATA_FILEPATH);
      } else {
        battery_sample_to_csv(battery_data_file, g_battery);
        fclose(battery_data_file);
      }

      // Delay to implement a desired sampling rate.
      // Take into account the time it took to acquire/save data.
      polling_sleep_duration_us = BATTERY_SAMPLING_PERIOD_US;
      polling_sleep_duration_us -= get_global_time_us() - g_battery->sys_time_us;
      if(polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);

    }
    g_battery_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Battery gauge interface
//-----------------------------------------------------------------------------
int getBatteryData(double* battery_v1_v, double* battery_v2_v,
                   double* battery_i_mA) {
    #if MAX17320 == 1
      int ret = max17320_get_voltages(&dev);
      if (battery_v1_v != NULL)
        *battery_v1_v = dev.cell_1_voltage;
      if (battery_v2_v != NULL)
        *battery_v2_v = dev.cell_2_voltage;
      ret |= max17320_get_battery_current(&dev);
      if (battery_i_mA != NULL)
        *battery_i_mA = dev.battery_current;
      return ret;
    #else
      int fd, result;
      signed short voltage, current;

      if ((fd = i2cOpen(1, ADDR_BATT_GAUGE, 0)) < 0) {
          CETI_ERR("Failed to connect to the fuel gauge");
          return (-1);
      }

      result = i2cReadByteData(fd, BATT_CELL_1_V_MS);
      voltage = result << 3;
      result = i2cReadByteData(fd, BATT_CELL_1_V_LS);
      voltage = (voltage | (result >> 5));
      voltage = (voltage | (result >> 5));
      if (battery_v1_v != NULL)
        *battery_v1_v = 4.883e-3 * voltage;

      result = i2cReadByteData(fd, BATT_CELL_2_V_MS);
      voltage = result << 3;
      result = i2cReadByteData(fd, BATT_CELL_2_V_LS);
      voltage = (voltage | (result >> 5));
      voltage = (voltage | (result >> 5));
      if (battery_v2_v != NULL)
        *battery_v2_v = 4.883e-3 * voltage;

      result = i2cReadByteData(fd, BATT_I_MS);
      current = result << 8;
      result = i2cReadByteData(fd, BATT_I_LS);
      current = current | result;
      if (battery_i_mA != NULL)
        *battery_i_mA = 1000 * current * (1.5625e-6 / BATT_R_SENSE);

      i2cClose(fd);
      return (0);
    #endif
}

//-----------------------------------------------------------------------------
// Charge and Discharge Enabling
//-----------------------------------------------------------------------------
int enableCharging(void) {
  #if MAX17320 == 1
    int ret = max17320_enable_charging(&dev);
    return ret;
  #else
    int fd, temp;
    if((fd=i2cOpen(1,ADDR_BATT_GAUGE,0)) < 0) {
      CETI_LOG("Failed to connect to the BMS IC on I2C");
      return (-1);
    }
    else {
      if ( (temp = i2cReadByteData(fd,BATT_PROTECT))  >= 0 ) {
        if ( (i2cWriteByteData(fd,BATT_PROTECT, (temp | CE))) == 0  ) { 
          CETI_LOG("I2C write succeeded, enabled charging");
          i2cClose(fd);
          return(0);
        }
        else {
          CETI_LOG("I2C write to BMS register failed");
          i2cClose(fd);
          return(-1);        
        }
      }
      else {
        CETI_LOG("Failed to read BMS register");
        i2cClose(fd);
        return(-1);
      }
    }
  #endif
}

int disableCharging(void) {
  #if MAX17320 == 1
    int ret = max17320_disable_charging(&dev);
    return ret;
  #else
    int fd, temp;
    if((fd=i2cOpen(1,ADDR_BATT_GAUGE,0)) < 0) {
      CETI_LOG("Failed to connect to the BMS IC on I2C");
      return (-1);
    }
    else {
      if ( (temp = i2cReadByteData(fd,BATT_PROTECT))  >= 0 ) {
        if ( (i2cWriteByteData(fd,BATT_PROTECT, (temp & ~CE))) == 0  ) { 
          CETI_LOG("I2C write succeeded, disabled charging");
          i2cClose(fd);
          return(0);
        }
        else {
          CETI_LOG("I2C write to BMS register failed");
          i2cClose(fd);
          return(-1);        
        }
      }
      else {
        CETI_LOG("Failed to read BMS register");
        i2cClose(fd);
        return(-1);
      }
    }
  #endif
}


int enableDischarging(void) {
  #if MAX17320 == 1
    int ret = max17320_enable_discharging(&dev);
    return ret;
  #else
    int fd, temp;
    if((fd=i2cOpen(1,ADDR_BATT_GAUGE,0)) < 0) {
      CETI_LOG("Failed to connect to the BMS IC on I2C");
      return (-1);
    }
    else {
      if ( (temp = i2cReadByteData(fd,BATT_PROTECT))  >= 0 ) {
        if ( (i2cWriteByteData(fd,BATT_PROTECT, (temp | DE))) == 0  ) { 
          CETI_LOG("I2C write succeeded, enabled discharging");
          i2cClose(fd);
          return(0);
        }
        else {
          CETI_LOG("I2C write to BMS register failed");
          i2cClose(fd);
          return(-1);        
        }
      }
      else {
        CETI_LOG("Failed to read BMS register");
        i2cClose(fd);
        return(-1);
      }
    }
  #endif
}

int disableDischarging(void) {
  #if MAX17320 == 1
    int ret = max17320_disable_discharging(&dev);
    return ret;
  #else
    int fd, temp;
    if((fd=i2cOpen(1,ADDR_BATT_GAUGE,0)) < 0) {
      CETI_LOG("Failed to connect to the BMS IC on I2C");
      return (-1);
    }
    else {
      if ( (temp = i2cReadByteData(fd,BATT_PROTECT))  >= 0 ) {
        if ( (i2cWriteByteData(fd,BATT_PROTECT, (temp & ~DE))) == 0  ) { 
          CETI_LOG("I2C write succeeded, disabled discharging");
          i2cClose(fd);
          return(0);
        }
        else {
          CETI_LOG("I2C write to BMS register failed");
          i2cClose(fd);
          return(-1);        
        }
      }
      else {
        CETI_LOG("Failed to read BMS register");
        i2cClose(fd);
        return(-1);
      }
    }
  #endif
}

int resetBattTempFlags(void) {
    discharging_disabled = 0;
    charging_disabled = 0;
    enableDischarging();
    enableCharging();
    return(0);
}
