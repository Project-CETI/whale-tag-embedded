//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "launcher.h"

#include "battery.h"
#include "burnwire.h"
#include "device/fpga.h"
#include "recovery.h"
#include "sensors/audio.h"
#include "sensors/light.h"
#include "sensors/pressure_temperature.h"
#include "state_machine.h"
#include "systemMonitor.h"
#include "utils/config.h"
#include "utils/logging.h"
#include "utils/thread_error.h"
#include "utils/timing.h"

/* Headers that include hardware dependent libraries can't be unit tested.
 * i.e. exclude any header with <pigpio.h> included in it.
 * (MSH)
 */
#ifndef UNIT_TEST
#include "commands.h"
#include "sensors/ecg.h"
#include "sensors/ecg_helpers/ecg_lod.h"
#include "sensors/imu.h"

#include <pigpio.h>
#endif // UNIT_TEST

#include <signal.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// Initialize global variables
//-----------------------------------------------------------------------------
int g_exit = 0;
int g_stopAcquisition = 0;
int g_stopLogging = 0;
char g_process_path[256] = "/opt/ceti-tag-data-capture/bin";

static uint32_t s_threads_in_error = 0;

void sig_handler(int signum) {
    CETI_LOG("Received termination request.");
    g_stopAcquisition = 1;
    usleep(100000);
    g_exit = 1;
}

//-----------------------------------------------------------------------------
// Main loop.
//-----------------------------------------------------------------------------
int main(void) {
    // Define callbacks for handling signals.
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // Get process location
    int bytes = readlink("/proc/self/exe", g_process_path, sizeof(g_process_path) - 1);
    while (bytes > 0) {
        if (g_process_path[bytes - 1] == '/') {
            g_process_path[bytes] = '\0';
            break;
        }
        bytes--;
    }

    // Tag-wide initialization.
    init_logging();

    // Initialize components.
    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Starting initialization");
    init_tag(); // returns 0 if all components succeeded, < 0 if error(s)

    //-----------------------------------------------------------------------------
    // Create threads.
    pthread_t thread_ids[50] = {0};
    int *threads_running[50];
#ifdef DEBUG
    char thread_name[50][32];
#endif
    int num_threads = 0;
    int audio_acquisition_thread_index = -1;
    int audio_write_thread_index = -1;
    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Starting acquisition threads");
    // RTC
#if ENABLE_RTC
    pthread_create(&thread_ids[num_threads], NULL, &rtc_thread, NULL);
    threads_running[num_threads] = &g_rtc_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "rtc");
#endif
    num_threads++;
#endif
    // Handle user commands.
    pthread_create(&thread_ids[num_threads], NULL, &command_thread, NULL);
    threads_running[num_threads] = &g_command_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "command");
#endif
    num_threads++;
    // Run the state machine.
    pthread_create(&thread_ids[num_threads], NULL, &stateMachine_thread, NULL);
    threads_running[num_threads] = &g_stateMachine_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "statemachine");
#endif
    num_threads++;
    // IMU
#if ENABLE_IMU
    pthread_create(&thread_ids[num_threads], NULL, &imu_thread, NULL);
    threads_running[num_threads] = &g_imu_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "imu");
#endif
    num_threads++;
#endif
    // Ambient light
#if ENABLE_LIGHT_SENSOR
    if (s_threads_in_error & (1 << THREAD_ALS_ACQ)) {
        CETI_WARN("Failed to initialize light acquisition thread. Thread created to log errors.");
    }
    pthread_create(&thread_ids[num_threads], NULL, &light_thread, NULL);
    threads_running[num_threads] = &g_light_thread_is_running;
    CETI_DEBUG("Thread created: Light Acquisition");
#ifdef DEBUG
    strcpy(thread_name[num_threads], "light");
#endif
    num_threads++;
#endif
    // Water pressure and temperature
#if ENABLE_PRESSURETEMPERATURE_SENSOR
    if (s_threads_in_error & (1 << THREAD_PRESSURE_ACQ)) {
        CETI_WARN("Failed to initialize pressure acquisition thread. Thread created to log errors");
    }
    pthread_create(&thread_ids[num_threads], NULL, &pressureTemperature_thread, NULL);
    threads_running[num_threads] = &g_pressureTemperature_thread_is_running;
    CETI_DEBUG("Thread created: Pressure Acquisition");
#ifdef DEBUG
    strcpy(thread_name[num_threads], "pressure");
#endif
    num_threads++;
#endif
    // Battery status monitor
#if ENABLE_BATTERY_GAUGE
    pthread_create(&thread_ids[num_threads], NULL, &battery_thread, NULL);
    threads_running[num_threads] = &g_battery_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "battery");
#endif
    num_threads++;
#endif
    // Recovery board (GPS).
#if ENABLE_RECOVERY
    if (g_config.recovery.enabled) {
        if(!(s_threads_in_error & THREAD_GPS_ACQ)) {
            pthread_create(&thread_ids[num_threads], NULL, &recovery_rx_thread, NULL);
            threads_running[num_threads] = &g_recovery_rx_thread_is_running;
#ifdef DEBUG
            strcpy(thread_name[num_threads], "recovery");
#endif
            num_threads++;
        } else {
            recovery_off();
        }
    }
#endif
    // ECG
#if ENABLE_ECG
#if ENABLE_ECG_LOD
    pthread_create(&thread_ids[num_threads], NULL, &ecg_lod_thread, NULL);
    threads_running[num_threads] = &g_ecg_lod_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "ecg_lod");
#endif // DEBUG
    num_threads++;
#endif // ENABLE_ECG_LOD

    pthread_create(&thread_ids[num_threads], NULL, &ecg_thread_getData, NULL);
    threads_running[num_threads] = &g_ecg_thread_getData_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "ecg_acq");
#endif
    num_threads++;

    pthread_create(&thread_ids[num_threads], NULL, &ecg_thread_writeData, NULL);
    threads_running[num_threads] = &g_ecg_thread_writeData_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "ecg_log");
#endif
    num_threads++;
#endif
    // System resource monitor
#if ENABLE_SYSTEMMONITOR
    pthread_create(&thread_ids[num_threads], NULL, &systemMonitor_thread, NULL);
    threads_running[num_threads] = &g_systemMonitor_thread_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "sys_monitor");
#endif
    num_threads++;
#endif
    // Audio
#if ENABLE_AUDIO
    usleep(1000000); // wait to make sure all other threads are on their assigned CPUs (maybe not needed?)
    pthread_create(&thread_ids[num_threads], NULL, &audio_thread_spi, NULL);
    threads_running[num_threads] = &g_audio_thread_spi_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "audio_acq");
#endif
    audio_acquisition_thread_index = num_threads;
    num_threads++;
#if ENABLE_AUDIO_FLAC
    pthread_create(&thread_ids[num_threads], NULL, &audio_thread_writeFlac, NULL);
    threads_running[num_threads] = &g_audio_thread_writeData_is_running;
#ifdef DEBUG
    strcpy(thread_name[num_threads], "audio_write");
#endif
    audio_write_thread_index = num_threads;
    num_threads++;
#else
    // dump raw audio files
    pthread_create(&thread_ids[num_threads], NULL, &audio_thread_writeRaw, NULL);
    threads_running[num_threads] = &g_audio_thread_writeData_is_running;
    audio_write_thread_index = num_threads;
    num_threads++;
#endif
#endif

    usleep(100000);
    CETI_LOG("Created %d threads", num_threads);

    //-----------------------------------------------------------------------------
    // Run the application!
    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Data acquisition is running!");
    CETI_LOG("-------------------------------------------------");
#ifdef DEBUG
    int logcount = 20;
#endif
    while (!g_exit) {
        // Let threads do their work.
        usleep(100000);
        if (g_exit)
            break;

// Check if the audio needs to be restarted after an overflow.
#if ENABLE_AUDIO
        if (g_audio_overflow_detected && (g_audio_thread_spi_is_running && g_audio_thread_writeData_is_running)) {
            // Wait for the threads to stop.
            while (g_audio_thread_spi_is_running || g_audio_thread_writeData_is_running)
                usleep(100000);
            // Restart the threads.
            pthread_create(&thread_ids[audio_acquisition_thread_index], NULL, &audio_thread_spi, NULL);
            threads_running[audio_acquisition_thread_index] = &g_audio_thread_spi_is_running;
#if ENABLE_AUDIO_FLAC
            pthread_create(&thread_ids[audio_write_thread_index], NULL, &audio_thread_writeFlac, NULL);
            threads_running[audio_write_thread_index] = &g_audio_thread_writeData_is_running;
#else
            pthread_create(&thread_ids[audio_write_thread_index], NULL, &audio_thread_writeRaw, NULL);
            threads_running[audio_write_thread_index] = &g_audio_thread_writeData_is_running;
#endif
            usleep(100000);
        }
#endif
#ifdef DEBUG
        if (logcount == 0) {
            int num_threads_running = 0;
            CETI_LOG("Active Threads");
            for (int thread_index = 0; thread_index < num_threads; thread_index++) {
                num_threads_running += *threads_running[thread_index];
                if (*threads_running[thread_index]) {
                    CETI_LOG("    %s", thread_name[thread_index]);
                }
            }
            logcount = 20;
        } else {
            logcount--;
        }
#endif

        // Check if the data partition is full.
        if (!g_stopAcquisition && get_dataPartition_free_kb() < MIN_DATA_PARTITION_FREE_KB) {
            CETI_LOG("*** DATA PARTITION IS FULL. Stopping all threads that acquire data.");
            g_stopAcquisition = 1;
        }
    }
    g_stopAcquisition = 1;
    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Data acquisition completed. Waiting for threads to stop.");

    //-----------------------------------------------------------------------------
    // Give threads time to notice the g_exit flag and shut themselves down.
    int num_threads_running = num_threads;
    int threads_timeout_reached = 0;
    int64_t wait_for_threads_timeout_us = 30000000;
    int64_t wait_for_threads_startTime_us = get_global_time_us();
    while (num_threads_running > 0 && !threads_timeout_reached) {
        usleep(100000);
        num_threads_running = 0;
        for (int thread_index = 0; thread_index < num_threads; thread_index++) {
            num_threads_running += *threads_running[thread_index];
        }
        threads_timeout_reached = get_global_time_us() - wait_for_threads_startTime_us > wait_for_threads_timeout_us;
    }

    // Forcefully cancel the threads.
    CETI_LOG("%d/%d threads stopped gracefully.", (int)(num_threads - num_threads_running), num_threads);
    CETI_LOG("Canceling threads");
    for (int thread_index = 0; thread_index < num_threads; thread_index++)
        pthread_cancel(thread_ids[thread_index]);

    // Tag-wide cleanup.
    CETI_LOG("Tag-wide cleanup");
    gpioTerminate();

    CETI_LOG("Done!");

    return (0);
}

//-----------------------------------------------------------------------------
// Helper method to initialize the tag.
//-----------------------------------------------------------------------------
int init_tag() {
    int result = 0;

    // Load the deployment configuration
    char config_file_path[512];
    strncpy(config_file_path, g_process_path, sizeof(config_file_path) - 1);
    strncat(config_file_path, CETI_CONFIG_FILE, sizeof(config_file_path) - 1);
    CETI_LOG("Reading permanent nonvolatile settings from %s", config_file_path);
    config_read(config_file_path);
    CETI_LOG("Reading current settings from %s", CETI_CONFIG_OVERWRITE_FILE);
    config_read(CETI_CONFIG_OVERWRITE_FILE);

    // Read config overlay from /data
    config_read("/data/ceti-config.txt");

    // Log used config used this deployment
    config_log();

    // Tag-wide initialization.
    if (gpioInitialise() < 0) {
        CETI_ERR("Failed to initialize pigpio");
        return 1;
    } else {
        CETI_LOG("Successfully initialized pigpio");
    }
    if (init_timing() != 0) {
        result += -1;
    }
    if (init_stateMachine() != 0) {
        result += -1;
    }

#if ENABLE_FPGA
    char fpga_bitstream_path[512];
    strncpy(fpga_bitstream_path, g_process_path, sizeof(fpga_bitstream_path) - 1);
#if ENABLE_RUNTIME_AUDIO
    snprintf(fpga_bitstream_path, sizeof(fpga_bitstream_path), "%s../config/top.%dch.%dbit.bin", g_process_path, CHANNELS, g_config.audio.bit_depth);
#else
    snprintf(fpga_bitstream_path, sizeof(fpga_bitstream_path), "%s../config/top.bin", g_process_path);
#endif
    // strncat(fpga_bitstream_path, FPGA_BITSTREAM, sizeof(fpga_bitstream_path) - 1);
    WTResult fpga_result = wt_fpga_init(fpga_bitstream_path);
    if (fpga_result != WT_OK) {
        char err_str[512];
        CETI_ERR("%s", wt_strerror_r(fpga_result, err_str, sizeof(err_str)));
        result += -1;
    }
#endif

#if ENABLE_BATTERY_GAUGE
    int bms_error = init_battery();
    if (bms_error != 0) {
        if (bms_error & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED)) {
            s_threads_in_error |= (1 << THREAD_BMS_ACQ);
        } else {
            result += -1; // non-critical error
        }
    }
#endif

#if ENABLE_BURNWIRE
    if (init_burnwire() != 0) {
        result += -1;
    }
#endif

#if ENABLE_AUDIO
    int audio_result = audio_thread_init();
    if (audio_result != THREAD_OK) {
        if (audio_result & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED)) {
            s_threads_in_error |= (1 << THREAD_AUDIO_ACQ);
        }
        result += -1;
    }
#endif

#if ENABLE_LIGHT_SENSOR
    int light_result = init_light();
    if (light_result != THREAD_OK) {
        if (light_result & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED)) {
            s_threads_in_error |= (1 << THREAD_ALS_ACQ);
        }
        result += -1;
    }
#endif

#if ENABLE_IMU
    int imu_result = init_imu();
    if (imu_result != THREAD_OK) {
        if (imu_result & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED)) {
            s_threads_in_error |= (1 << THREAD_IMU_ACQ);
        }
        result += -1;
    }
#endif

#if ENABLE_RECOVERY
    if (g_config.recovery.enabled && ()) {
        int recovery_result = recovery_thread_init(&g_config);
        if (recovery_result != THREAD_OK) {
            if (recovery_result & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED | THREAD_ERR_HW)) {
                s_threads_in_error |= (1 << THREAD_GPS_ACQ);
            }
            result += -1;
        }
    }
    if (!g_config.recovery.enabled || (s_threads_in_error & (1 << THREAD_GPS_ACQ))) {
        recovery_off();
    }
#endif

#if ENABLE_PRESSURETEMPERATURE_SENSOR
    int pressure_result = init_pressureTemperature();
    if (pressure_result != THREAD_OK) {
        if (pressure_result & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED)) {
            s_threads_in_error |= (1 << THREAD_PRESSURE_ACQ);
        }
        result += -1;
    }
#endif

#if ENABLE_ECG
    int ecg_result = init_ecg();
    if (ecg_result != THREAD_OK) {
        if (pressure_result & (THREAD_ERR_SEM_FAILED | THREAD_ERR_SHM_FAILED)) {
            s_threads_in_error |= (1 << THREAD_ECG_ACQ);
        }
        result += -1;
    }
#endif

#if ENABLE_SYSTEMMONITOR
    if (init_systemMonitor() != 0) {
        result += -1;
    }
#endif

    if (result < 0 || (s_threads_in_error)) {
        CETI_ERR("Tag initialization failed (at least one component failed to initialize - see previous printouts for more information)");
        if (!(s_threads_in_error & (1 << THREAD_GPS_ACQ))) {
            char rec_msg[68] = {};
            snprintf(rec_msg, 67, "THREAD INIT ERR: %04Xh", s_threads_in_error);
            recovery_message(rec_msg);
        }
        return result - s_threads_in_error;
    }

    return result;
}
