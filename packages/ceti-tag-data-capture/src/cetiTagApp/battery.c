//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//              Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "battery.h"

// === Private Local Headers ===
#include "device/max17320.h"
#include "launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
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
static FILE *battery_data_file = NULL;
static char battery_data_file_notes[256] = "";
static const char *battery_data_file_headers[] = {
    "Battery V1 [V]",
    "Battery V2 [V]",
    "Battery I [mA]",
    "Battery T1 [C]",
    "Battery T2 [C]",
    "State of Charge [\%]",
    "Status",
    "Protection Alerts",
};
static const int num_battery_data_file_headers = sizeof(battery_data_file_headers) / sizeof(*battery_data_file_headers);
CetiBatterySample *shm_battery = NULL;
static sem_t *sem_battery_data_ready;
static int charging_disabled, discharging_disabled;

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_battery() {
    WTResult hw_result = max17320_init();
    if (hw_result != WT_OK) {
        CETI_ERR("Failed to connect to MAX17320 Fuel Gauge: %s", wt_strerror(hw_result));
        return -1;
    }

    // Open an output file to write data.
    CETI_LOG("Successfully initialized the battery gauge");
    if (init_data_file(battery_data_file, BATTERY_DATA_FILEPATH,
                       battery_data_file_headers, num_battery_data_file_headers,
                       battery_data_file_notes, "init_battery()") < 0)
        return -1;

    // setup shared memory
    shm_battery = create_shared_memory_region(BATTERY_SHM_NAME, sizeof(CetiBatterySample));

    // setup semaphore
    sem_battery_data_ready = sem_open(BATTERY_SEM_NAME, O_CREAT, 0644, 0);
    if (sem_battery_data_ready == SEM_FAILED) {
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
    static char status_string[72] = ""; // max string length is 65
    static uint16_t previous_status = 0;
    char *flags[11] = {};
    int flag_count = 0;

    // mask do not care bits
    raw &= 0xF7C6;

    // don't do work we don't have to do
    if (raw == previous_status) {
        return status_string;
    }

    // clear old string
    status_string[0] = '\0';

    // check if no flags
    if (raw == 0) {
        previous_status = 0;
        return status_string;
    }

    // detect flags
    if (_RSHIFT(raw, 15, 1))
        flags[flag_count++] = "PA";
    if (_RSHIFT(raw, 1, 1))
        flags[flag_count++] = "POR";
    // if(_RSHIFT(raw, 7, 1)) flags[flag_count++] = "dSOCi"; //ignored indicates interger change in SoC
    if (_RSHIFT(raw, 2, 1))
        flags[flag_count++] = "Imn";
    if (_RSHIFT(raw, 6, 1))
        flags[flag_count++] = "Imx";
    if (_RSHIFT(raw, 8, 1))
        flags[flag_count++] = "Vmn";
    if (_RSHIFT(raw, 12, 1))
        flags[flag_count++] = "Vmx";
    if (_RSHIFT(raw, 9, 1))
        flags[flag_count++] = "Tmn";
    if (_RSHIFT(raw, 13, 1))
        flags[flag_count++] = "Tmx";
    if (_RSHIFT(raw, 10, 1))
        flags[flag_count++] = "Smn";
    if (_RSHIFT(raw, 14, 1))
        flags[flag_count++] = "Smx";

    // generate string
    for (int j = 0; j < flag_count; j++) {
        if (j != 0) {
            strncat(status_string, " | ", sizeof(status_string) - 1);
        }
        strncat(status_string, flags[j], sizeof(status_string) - 1);
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
    char *flags[16] = {};
    int flag_count = 0;

    // don't do work we don't have to do
    if (raw == previous_protAlrt) {
        return protAlrt_string;
    }

    // clear old string
    protAlrt_string[0] = '\0';

    // check if no flags
    if (raw == 0) {
        previous_protAlrt = 0;
        return protAlrt_string;
    }

    if (_RSHIFT(raw, 15, 1))
        flags[flag_count++] = "ChgWDT";
    if (_RSHIFT(raw, 14, 1))
        flags[flag_count++] = "TooHotC";
    if (_RSHIFT(raw, 13, 1))
        flags[flag_count++] = "Full";
    if (_RSHIFT(raw, 12, 1))
        flags[flag_count++] = "TooColdC";
    if (_RSHIFT(raw, 11, 1))
        flags[flag_count++] = "OVP";
    if (_RSHIFT(raw, 10, 1))
        flags[flag_count++] = "OCCP";
    if (_RSHIFT(raw, 9, 1))
        flags[flag_count++] = "Qovflw";
    if (_RSHIFT(raw, 8, 1))
        flags[flag_count++] = "PrepF";
    if (_RSHIFT(raw, 7, 1))
        flags[flag_count++] = "Imbalance";
    if (_RSHIFT(raw, 6, 1))
        flags[flag_count++] = "PermFail";
    if (_RSHIFT(raw, 5, 1))
        flags[flag_count++] = "DieHot";
    if (_RSHIFT(raw, 4, 1))
        flags[flag_count++] = "TooHotD";
    if (_RSHIFT(raw, 3, 1))
        flags[flag_count++] = "UVP";
    if (_RSHIFT(raw, 2, 1))
        flags[flag_count++] = "ODCP";
    if (_RSHIFT(raw, 1, 1))
        flags[flag_count++] = "ResDFault";
    if (_RSHIFT(raw, 0, 1))
        flags[flag_count++] = "LDet";

    // generate string
    for (int j = 0; j < flag_count; j++) {
        if (j != 0) {
            strncat(protAlrt_string, " | ", sizeof(protAlrt_string) - 1);
        }
        strncat(protAlrt_string, flags[j], sizeof(protAlrt_string) - 1);
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
    shm_battery->error = WT_OK;
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_get_cell_voltage_v(0, &shm_battery->cell_voltage_v[0]);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_get_cell_voltage_v(1, &shm_battery->cell_voltage_v[1]);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_get_current_mA(&shm_battery->current_mA);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_get_cell_temperature_c(0, &shm_battery->cell_temperature_c[0]);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_get_cell_temperature_c(1, &shm_battery->cell_temperature_c[1]);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_get_state_of_charge(&shm_battery->state_of_charge);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_read(MAX17320_REG_STATUS, &shm_battery->status);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_read(MAX17320_REG_PROTALRT, &shm_battery->protection_alert);
    }

    // push semaphore to indicate to user applications that new data is available
    sem_post(sem_battery_data_ready);

    // clear protection alert flags and status flags
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_write(MAX17320_REG_PROTALRT, 0x0000);
    }
    if (shm_battery->error == WT_OK) {
        shm_battery->error = max17320_write(MAX17320_REG_STATUS, 0x0000);
    }
}

/**
 * @brief Converts a battery sample to human readable csv format and saves it
 *     to a file.
 *
 * @param fp pointer to the destination csv file
 * @param pSample pointer to battery sample
 */
void battery_sample_to_csv(FILE *fp, CetiBatterySample *pSample) {
    // Write timing information.
    fprintf(fp, "%ld", pSample->sys_time_us);
    fprintf(fp, ",%d", pSample->rtc_time_s);
    // Write any notes, then clear them so they are only written once.
    fprintf(fp, ",%s", battery_data_file_notes);
    strcpy(battery_data_file_notes, "");
    if (pSample->error != WT_OK) {
        fprintf(fp, "ERROR(%s, %04Xh) | ", wt_strerror_device_name(pSample->error), (pSample->error & 0xFFFF));
    }
    if (pSample->cell_voltage_v[0] < 0.0 || pSample->cell_voltage_v[1] < 0.0 || pSample->cell_temperature_c[0] < -80.0 || pSample->cell_temperature_c[1] < -80.0) { // it seems to return -0.01 for voltages and -5.19 for current when no sensor is connected
        CETI_WARN("readings are likely invalid");
        fprintf(fp, "INVALID? | ");
    }

    // Write the sensor data.
    fprintf(fp, ",%.3f", pSample->cell_voltage_v[0]);
    fprintf(fp, ",%.3f", pSample->cell_voltage_v[1]);
    fprintf(fp, ",%.3f", pSample->current_mA);
    fprintf(fp, ",%.3f", pSample->cell_temperature_c[0]);
    fprintf(fp, ",%.3f", pSample->cell_temperature_c[1]);
    fprintf(fp, ",%.3f", pSample->state_of_charge);
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
void *battery_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_battery_thread_tid = gettid();

    // Set the thread CPU affinity.
    if (BATTERY_CPU >= 0) {
        pthread_t thread;
        thread = pthread_self();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(BATTERY_CPU, &cpuset);
        if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
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
        for (int i_cell = 0; i_cell < 2; i_cell++) {
            if ((shm_battery->cell_temperature_c[i_cell] > MAX_CHARGE_TEMP) || (shm_battery->cell_temperature_c[i_cell] < MIN_CHARGE_TEMP)) {
                if (!charging_disabled) {
                    WTResult hw_result = max17320_disable_charging();
                    if (hw_result != WT_OK) {
                        CETI_ERR("Could not disable charging FET: %s", wt_strerror(hw_result));
                    } else {
                        charging_disabled = 1;
                        CETI_WARN("Battery charging disabled, cell %d outside thermal limits: %.3f C", i_cell + 1, shm_battery->cell_temperature_c[i_cell]);
                    }
                }
            }

            if ((shm_battery->cell_temperature_c[i_cell] > MAX_DISCHARGE_TEMP)) {
                if (!discharging_disabled) {
                    WTResult hw_result = max17320_disable_discharging();
                    if (hw_result != WT_OK) {
                        CETI_ERR("Could not disable discharging FET: %s", wt_strerror(hw_result));
                    } else {
                        discharging_disabled = 1;
                        CETI_WARN("Battery discharging disabled, cell %d outside thermal limit: %.3f C", i_cell + 1, shm_battery->cell_temperature_c[i_cell]);
                    }
                }
            }
        }

        // ******************   End Battery Temperature Checks *********************

        if (!g_stopLogging) {
            /* ToDo: move to seperate logging app.
             * Daemon should only handle sample acq not storage
             */
            battery_data_file = fopen(BATTERY_DATA_FILEPATH, "at");
            if (battery_data_file == NULL) {
                CETI_WARN("failed to open data output file: %s", BATTERY_DATA_FILEPATH);
            } else {
                battery_sample_to_csv(battery_data_file, shm_battery);
                fclose(battery_data_file);
            }
        }

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.
        polling_sleep_duration_us = BATTERY_SAMPLING_PERIOD_US;
        polling_sleep_duration_us -= get_global_time_us() - shm_battery->sys_time_us;
        if (polling_sleep_duration_us > 0)
            usleep(polling_sleep_duration_us);
    }
    sem_close(sem_battery_data_ready);
    munmap(shm_battery, sizeof(CetiBatterySample));
    g_battery_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Charge and Discharge Enabling
//-----------------------------------------------------------------------------
int resetBattTempFlags(void) {
    WTResult hw_result = WT_OK;
    hw_result = max17320_enable_discharging();
    if (hw_result != WT_OK) {
        CETI_ERR("Could not enable discharging FET: %s", wt_strerror(hw_result));
        return hw_result;
    }
    discharging_disabled = 0;
    hw_result = max17320_enable_charging();
    if (hw_result != WT_OK) {
        CETI_ERR("Could not enable charging FET: %s", wt_strerror(hw_result));
        return hw_result;
    }
    charging_disabled = 0;
    return WT_OK;
}
