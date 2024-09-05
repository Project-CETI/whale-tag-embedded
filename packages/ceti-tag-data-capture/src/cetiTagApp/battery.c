//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//              Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "battery.h"

// === Private Local Headers ===
#include "launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "max17320.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/logging.h"
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
CetiBatterySample *shm_battery = NULL;
static sem_t* sem_battery_data_ready;
static int charging_disabled, discharging_disabled;
// Store global versions of the latest readings since the state machine will use them.
MAX17320_HandleTypeDef dev;



//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_battery() {
    int ret = max17320_init(&dev);  
    if (ret < 0){
      CETI_ERR("Failed to connect to MAX17320 Fuel Gauge");
      return ret;
    }
 
    // Open an output file to write data.
    CETI_LOG("Successfully initialized the battery gauge");
    if(init_data_file(battery_data_file, BATTERY_DATA_FILEPATH,
                      battery_data_file_headers,  num_battery_data_file_headers,
                      battery_data_file_notes, "init_battery()") < 0)
        return -1;

    //setup shared memory
    shm_battery = create_shared_memory_region(BATTERY_SHM_NAME, sizeof(CetiBatterySample));

    //setup semaphore
    sem_battery_data_ready = sem_open(BATTERY_SEM_NAME, O_CREAT, 0644, 0);
    if(sem_battery_data_ready == SEM_FAILED){
        perror("sem_open");
        CETI_ERR("Failed to create semaphore");
        return -1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------

/**
 * @brief converts the raw value of the BMS status register to a human
 * readable string.
 * 
 * @param raw raw status register value.
 * @return const char*
 */
static const char *__status_to_str(uint16_t raw) {
    static char status_string[72] = ""; //max string length is 65
    static uint16_t previous_status = 0;
    char* flags [11] = {};
    int flag_count = 0;

    // mask do not care bits
    raw &= 0xF7C6;

    //don't do work we don't have to do
    if (raw == previous_status) {
      return status_string;
    }

    //clear old string
    status_string[0] = '\0';

    // check if no flags
    if (raw == 0) {
      previous_status = 0;
      return status_string;
    }

    //detect flags
    if(_RSHIFT(raw, 15, 1)) flags[flag_count++] = "PA";
    if(_RSHIFT(raw, 1, 1)) flags[flag_count++] = "POR";
    // if(_RSHIFT(raw, 7, 1)) flags[flag_count++] = "dSOCi"; //ignored indicates interger change in SoC
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

/**
 * @brief converts the raw value of the BMS protAlert register to a human
 * readable string.
 * 
 * @param raw raw protAlert register value.
 * @return const char*
 */
static const char *__protAlrt_to_str(uint16_t raw) {
    static char protAlrt_string[160] = "";
    static uint16_t previous_protAlrt = 0;
    char* flags [16] = {};
    int flag_count = 0;

    //don't do work we don't have to do
    if (raw == previous_protAlrt) {
      return protAlrt_string;
    }

    //clear old string
    protAlrt_string[0] = '\0';

    // check if no flags
    if (raw == 0) {
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

/**
 * @brief update the latest battery sample in shared memory.
 */
void battery_update_sample(void) {
    // create sample 
    shm_battery->sys_time_us = get_global_time_us();
    shm_battery->rtc_time_s = getRtcCount();
    shm_battery->error = getBatteryData(&shm_battery->cell_voltage_v[0], &shm_battery->cell_voltage_v[1], &shm_battery->current_mA);
    if(!shm_battery->error) shm_battery->error = max17320_get_cell_temperature_c(0, &shm_battery->cell_temperature_c[0]);
    if(!shm_battery->error) shm_battery->error = max17320_get_cell_temperature_c(1, &shm_battery->cell_temperature_c[1]);
    if(!shm_battery->error) shm_battery->error = max17320_read(MAX17320_REG_STATUS, &shm_battery->status);
    if(!shm_battery->error) shm_battery->error = max17320_read(MAX17320_REG_PROTALRT, &shm_battery->protection_alert);
  
    // push semaphore to indicate to user applications that new data is available
    sem_post(sem_battery_data_ready);

    //clear protection alert flags and status flags
    if(!shm_battery->error) shm_battery->error = max17320_write(MAX17320_REG_PROTALRT, 0x0000);
    if(!shm_battery->error) shm_battery->error = max17320_write(MAX17320_REG_STATUS, 0x0000);
}

/**
 * @brief Converts a battery sample to human readable csv format and saves it
 *     to a file.
 * 
 * @param fp pointer to the destination csv file
 * @param pSample pointer to battery sample
 */
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

/**
 * @brief This thread handles many things related to the battery:
 * (1) It acquires a battery sample from the BMS every second.
 * (2) It checks if all cell temperatures are within an acceptable temperature
 * range and disables charge/discharge FETs accordingly.
 * (3) It converts all battery samples to human readable format and saves them
 * to disk.
 * 
 * @param paramPtr unused
 * @return void* always NULL
 */
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
        if( (shm_battery->cell_temperature_c[i_cell] > MAX_CHARGE_TEMP) ||  (shm_battery->cell_temperature_c[i_cell] < MIN_CHARGE_TEMP) ) {          
            if (!charging_disabled){  
              disableCharging();
              charging_disabled = 1;
              CETI_WARN("Battery charging disabled, cell %d outside thermal limits: %.3f C", i_cell + 1, shm_battery->cell_temperature_c[i_cell]);
            }
        }

        if( (shm_battery->cell_temperature_c[i_cell] > MAX_DISCHARGE_TEMP) ) {
            if (!discharging_disabled){  
              disableDischarging();
              discharging_disabled = 1;
              CETI_WARN("Battery discharging disabled, cell %d outside thermal limit: %.3f C", i_cell + 1, shm_battery->cell_temperature_c[i_cell]);
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
        battery_sample_to_csv(battery_data_file, shm_battery);
        fclose(battery_data_file);
      }
      

      // Delay to implement a desired sampling rate.
      // Take into account the time it took to acquire/save data.
      polling_sleep_duration_us = BATTERY_SAMPLING_PERIOD_US;
      polling_sleep_duration_us -= get_global_time_us() - shm_battery->sys_time_us;
      if(polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);

    }
    sem_close(sem_battery_data_ready);
    munmap(shm_battery, sizeof(CetiBatterySample));
    g_battery_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Battery gauge interface
//-----------------------------------------------------------------------------
int getBatteryData(double* battery_v1_v, double* battery_v2_v, double* battery_i_mA) {
    int ret = max17320_get_voltages(&dev);
    if (battery_v1_v != NULL)
      *battery_v1_v = dev.cell_1_voltage;
    if (battery_v2_v != NULL)
      *battery_v2_v = dev.cell_2_voltage;
    ret |= max17320_get_battery_current(&dev);
    if (battery_i_mA != NULL)
      *battery_i_mA = dev.battery_current;
    return ret;
}

//-----------------------------------------------------------------------------
// Charge and Discharge Enabling
//-----------------------------------------------------------------------------
int enableCharging(void) {
    int ret = max17320_enable_charging(&dev);
    return ret;
}

int disableCharging(void) {
    int ret = max17320_disable_charging(&dev);
    return ret;
}


int enableDischarging(void) {
    int ret = max17320_enable_discharging(&dev);
    return ret;
}

int disableDischarging(void) {
    int ret = max17320_disable_discharging(&dev);
    return ret;
}

int resetBattTempFlags(void) {
    discharging_disabled = 0;
    charging_disabled = 0;
    enableDischarging();
    enableCharging();
    return(0);
}
