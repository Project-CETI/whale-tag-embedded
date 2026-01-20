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
#include "led_ctrl.h"
#include "log/imu_log.h"
#include "recovery.h"
#include "sensors/audio.h"
#include "sensors/light.h"
#include "sensors/pressure_temperature.h"
#include "systemMonitor.h"
#include "utils/logging.h"
#include "utils/meta.h"
#include "utils/thread_error.h"

/* Headers that include hardware dependent libraries can't be unit tested.
 * i.e. exclude any header with <pigpio.h> included in it.
 * (MSH)
 */
#ifndef UNIT_TEST
#include "sensors/ecg.h"
#include "sensors/ecg_helpers/ecg_lod.h"
#include "sensors/imu.h"
#endif // UNIT_TEST

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

//-----------------------------------------------------------------------------
// Initialize global variables
//-----------------------------------------------------------------------------
#define THREAD_MANAGER_JOIN_TIMEOUT_S (30)

volatile int g_stopAcquisition = 1;

static uint32_t s_threads_in_error = 0;

typedef enum {
    PRI_DEFAULT = 0,
    PRI_MIN = 1,
    PRI_MAX = 2,
} ThreadPri;

static const struct {
    char *name;
    void *(*main_fn)(void *);
    ThreadPri priority;
    int affinity;
} acq_thread_desc[NUM_ACQ_THREAD] = {
#if ENABLE_LIGHT_SENSOR
    [ACQ_THREAD_ALS] = {
        .name = "ambient light senspr",
        .main_fn = light_thread,
        .affinity = LIGHT_CPU + 1,
    },
#endif // ENABLE_LIGHT_SENSOR

#if ENABLE_AUDIO
    [ACQ_THREAD_AUDIO_ACQ] = {.name = "audio acquisition", .main_fn = audio_thread_spi, .affinity = AUDIO_SPI_CPU + 1, .priority = PRI_MAX},
    [ACQ_THREAD_AUDIO_LOG] = {
        .name = "audio logging",
#if ENABLE_AUDIO_FLAC
        .main_fn = audio_thread_writeFlac,
#else
        .main_fn = audio_thread_writeRaw,
#endif
        .affinity = AUDIO_WRITEDATA_CPU + 1,
        .priority = PRI_MIN,
    },
#endif // ENABLE_AUDIO

#if ENABLE_BATTERY_GAUGE
    [ACQ_THREAD_BATTERY] = {
        .name = "battery monitoring",
        .main_fn = battery_thread,
        .affinity = BATTERY_CPU + 1,
    },
#endif // ENABLE_BATTERY_GAUGE

    [ACQ_THREAD_DEPLOYMENT_CONFIG_LOG] = {
        .name = "log deployment config",
        .main_fn = meta_log_thread,
    },

#if ENABLE_ECG
    [ACQ_THREAD_ECG_ACQ] = {
        .name = "ecg acquistion",
        .main_fn = ecg_thread_getData,
        .affinity = ECG_GETDATA_CPU + 1,
        .priority = PRI_MAX,
    },
    [ACQ_THREAD_ECG_LOG] = {
        .name = "ecg logging",
        .main_fn = ecg_thread_writeData,
        .affinity = ECG_WRITEDATA_CPU + 1,
        .priority = PRI_MIN,
    },
#if ENABLE_ECG_LOD
    [ACQ_THREAD_ECG_LOD_ACQ] = {
        .name = "ecg lod acquistion",
        .main_fn = ecg_lod_thread,
    },
#endif // ENABLE_ECG_LOD
#endif // ENABLE_ECG

#if ENABLE_RECOVERY
    [ACQ_THREAD_GPS] = {
        .name = "gps acquisition",
        .main_fn = recovery_rx_thread,
        .affinity = RECOVERY_RX_CPU + 1,
    },
#endif // ENABLE_RECOVERY

#if ENABLE_IMU
    [ACQ_THREAD_IMU_ACQ] = {
        .name = "imu acquisition",
        .main_fn = imu_thread,
        .affinity = IMU_CPU + 1,
    },
    [ACQ_THREAD_IMU_LOG] = {
        .name = "imu logging",
        .main_fn = imu_log_thread,
    },
#endif // ENABLE_IMU

#if ENABLE_PRESSURETEMPERATURE_SENSOR
    [ACQ_THREAD_PRESSURE] = {
        .name = "pressure",
        .main_fn = pressureTemperature_thread,
        .affinity = PRESSURETEMPERATURE_CPU + 1,
    },
#endif // ENABLE_PRESSURETEMPERATURE_SENSOR

#if ENABLE_SYSTEMMONITOR
    [ACQ_THREAD_SYSTEM_MONITOR] = {
        .name = "system monitor",
        .main_fn = systemMonitor_thread,
        .affinity = SYSTEMMONITOR_CPU + 1,
    },
#endif // ENABLE_SYSTEMMONITOR

};

static uint8_t acq_thread_valid[NUM_ACQ_THREAD] = {0};
static pthread_t acq_threads[NUM_ACQ_THREAD];
static pid_t acq_thread_tids[NUM_ACQ_THREAD];

//-----------------------------------------------------------------------------
// Individual thread methods
//-----------------------------------------------------------------------------
void threadManager_create_thread(AcqThreadType thread_index) {
    if (NULL == acq_thread_desc[thread_index].main_fn) {
        return;
    }

    // check if thread is already running
    if (acq_thread_valid[thread_index] && (EBUSY == pthread_tryjoin_np(acq_threads[thread_index], NULL))) {
        return;
    }

    // setup thread attributes
    pthread_attr_t attr;
    int attr_result = pthread_attr_init(&attr);
    if (attr_result != 0) {
        CETI_WARN("Failed to initialize attribute struct for %s thread: %s", acq_thread_desc[thread_index].name, strerror(errno));
    }

    // set affinity
    if (acq_thread_desc[thread_index].affinity != 0) {
        int cpu_index;
        int affinity_result;
        cpu_set_t cpuset;

        cpu_index = acq_thread_desc[thread_index].affinity - 1;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_index, &cpuset);
        affinity_result = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
        if (affinity_result != 0) {
            CETI_WARN("Failed to set %s thread to CPU %d: %s", acq_thread_desc[thread_index].name, cpu_index, strerror(errno));
        }
    }

    // set priority
    if (PRI_DEFAULT != acq_thread_desc[thread_index].priority) {
        int pri_result;
        struct sched_param sp = {};

        if (PRI_MAX == acq_thread_desc[thread_index].priority) {
            sp.sched_priority = sched_get_priority_max(SCHED_RR);
        } else if (PRI_MIN == acq_thread_desc[thread_index].priority) {
            sp.sched_priority = sched_get_priority_min(SCHED_RR);
        }
        pri_result = pthread_attr_setschedpolicy(&attr, SCHED_RR);
        if (0 == pri_result) {
            pri_result = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        }
        if (0 == pri_result) {
            CETI_LOG("Thread sched attribute inheritance set");
            pri_result = pthread_attr_setschedparam(&attr, &sp);
        }
        if (pri_result != 0) {
            CETI_WARN("Failed to set %s thread priority: %s", acq_thread_desc[thread_index].name, strerror(errno));
        }
    }

    // create thread
    int create_result = pthread_create(&acq_threads[thread_index], &attr, acq_thread_desc[thread_index].main_fn, NULL);
    if (create_result != 0) {
        CETI_WARN("Failed to create %s thread: %s", acq_thread_desc[thread_index].name, strerror(errno));
    }
    acq_thread_valid[thread_index] = 1;
    pthread_attr_destroy(&attr);
}

int threadManager_join_thread(AcqThreadType thread_index) {
    if (!acq_thread_valid[thread_index]) {
        return 0;
    }
    int result = pthread_join(acq_threads[thread_index], NULL);
    if (result == 0) {
        acq_thread_valid[thread_index] = result;
    }
    return result;
}

int threadManager_tryjoin_thread(AcqThreadType thread_index) {
    if (!acq_thread_valid[thread_index]) {
        return 0;
    }
    return pthread_tryjoin_np(acq_threads[thread_index], NULL);
}

void threadManager_start_acquisition(void) {
    if (0 == g_stopAcquisition) {
        return; // acq already stopped
    }
    g_stopAcquisition = 0;

    // IMU
    threadManager_create_thread(ACQ_THREAD_IMU_ACQ);
    threadManager_create_thread(ACQ_THREAD_IMU_LOG);

    // Ambient light
    threadManager_create_thread(ACQ_THREAD_ALS);

    // Water pressure and temperature
    threadManager_create_thread(ACQ_THREAD_PRESSURE);

    // Battery status monitor
    threadManager_create_thread(ACQ_THREAD_BATTERY);

    // Recovery board (GPS).
#if ENABLE_RECOVERY
    if (g_config.recovery.enabled) {
        if (!(s_threads_in_error & (1 << THREAD_GPS_ACQ))) {
            threadManager_create_thread(ACQ_THREAD_GPS);
        } else {
            recovery_off();
        }
    }
#endif

    // ECG
    threadManager_create_thread(ACQ_THREAD_ECG_LOD_ACQ);
    threadManager_create_thread(ACQ_THREAD_ECG_ACQ);
    threadManager_create_thread(ACQ_THREAD_ECG_LOG);

    // System resource monitor
    threadManager_create_thread(ACQ_THREAD_SYSTEM_MONITOR);

    // Audio
    threadManager_create_thread(ACQ_THREAD_AUDIO_ACQ);
    threadManager_create_thread(ACQ_THREAD_AUDIO_LOG);

    // Tag deployment info
    threadManager_create_thread(ACQ_THREAD_DEPLOYMENT_CONFIG_LOG);
    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Data acquisition is running!");
    CETI_LOG("-------------------------------------------------");
}

void threadManager_stop_acquisition(void) {
    struct timespec ts;

    // signal to acquisition threads to stop
    if (1 == g_stopAcquisition) {
        return;
    }
    g_stopAcquisition = 1;

    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Data acquisition completed. Waiting for threads to stop.");

    // grab timestamp
    clock_gettime(CLOCK_REALTIME, &ts);

    // set timeout
    ts.tv_sec += THREAD_MANAGER_JOIN_TIMEOUT_S;

    // check that all acquisition threads stop
    for (int thread_index = 0; thread_index < NUM_ACQ_THREAD; thread_index++) {
        if (acq_thread_valid[thread_index] && (pthread_timedjoin_np(acq_threads[thread_index], NULL, &ts) != 0)) {
            CETI_ERR("%s thread failed to stop. Cancelling thread", acq_thread_desc[thread_index].name);
            pthread_cancel(acq_threads[thread_index]);
        }
        acq_thread_valid[thread_index] = 0;
    }
    CETI_LOG("All acquisition threads have been stopped");
}

// Initialize harware associated with data acquisition
void threadManager_init(void) {
    int result = 0;

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
    if (g_config.recovery.enabled) {
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
        if (s_threads_in_error != 0) {
            uint32_t critical_err = s_threads_in_error & ((1 << THREAD_BMS_ACQ) | (1 << THREAD_AUDIO_ACQ));
            LEDCtrl_flash_err(NUM_ACQ_THREAD, s_threads_in_error, critical_err);
        }
        if (!(s_threads_in_error & (1 << THREAD_GPS_ACQ))) {
            char rec_msg[68] = {};
            snprintf(rec_msg, 67, "THREAD INIT ERR: %04Xh", s_threads_in_error);
            recovery_message(rec_msg);
            if (g_config.recovery.tx_on_whale) {
                recovery_gps_only();
            }
        }
    }

    return;
}

//-----------------------------------------------------------------------------
// SYSTEM CORE
//-----------------------------------------------------------------------------
#include "burnwire.h"
#include "commands.h"
#include "device/fpga.h"
#include "device/max17320.h"
// #include "launcher.h"
// #include "led_ctrl.h"
#include "state_machine.h"
#include "utils/config.h"
// #include "utils/logging.h"
#include "utils/timing.h"

#include <linux/reboot.h>
#include <pigpio.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <time.h>

#define CORE_THREAD_JOIN_TIMEOUT_S 45

typedef enum {
    CORE_THREAD_RTC,
    CORE_THREAD_COMMAND_PIPE,
    CORE_THREAD_LED_CTRL,
    NUM_CORE_THREAD,
} CoreThreadType;

int g_exit = 0;
int g_stopLogging = 0;
char g_process_path[256] = "/opt/ceti-tag-data-capture/bin";

const char *core_thread_names[NUM_CORE_THREAD] = {
    [CORE_THREAD_RTC] = "rtc",
    [CORE_THREAD_COMMAND_PIPE] = "command handling",
    [CORE_THREAD_LED_CTRL] = "LED control",
};

static pthread_t core_threads[NUM_CORE_THREAD];

static void sig_handler(int signum) {
    CETI_LOG("Received termination request.");
    threadManager_stop_acquisition();
    g_exit = 1;
}

static int core_init(void) {
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

    /********** start core threads ***********/
    // Initialize components.
    CETI_LOG("-------------------------------------------------");
    CETI_LOG("Starting initialization");

    // Load the deployment configuration
    char config_file_path[512];
    strncpy(config_file_path, g_process_path, sizeof(config_file_path) - 1);
    strncat(config_file_path, CETI_CONFIG_FILE, sizeof(config_file_path) - 1);
    CETI_LOG("Reading permanent nonvolatile settings from %s", config_file_path);
    config_read(config_file_path);
    CETI_LOG("Reading current settings from %s", CETI_CONFIG_OVERWRITE_FILE);
    config_read(CETI_CONFIG_OVERWRITE_FILE);

    if (gpioInitialise() < 0) {
        CETI_ERR("Failed to initialize pigpio");
        return 1;
    } else {
        CETI_LOG("Successfully initialized pigpio");
    }

#if ENABLE_BATTERY_GAUGE
    // Renabling battery power
    int battery_result = max17320_clear_write_protection();
    if (WT_OK == battery_result) {
        battery_result = max17320_enable_discharging();
    }
    if (WT_OK == battery_result) {
        battery_result = max17320_enable_charging();
    }
    if (WT_OK != battery_result) {
        char err_str[128] = {};
        wt_strerror_r(battery_result, err_str, sizeof(err_str));
        CETI_WARN("Failed to \"wake\" the tag: %s", err_str);
    }
#endif

    init_timing();
#ifdef ENABLE_RTC
    pthread_create(&core_threads[CORE_THREAD_RTC], NULL, &rtc_thread, NULL);
#endif // ENABLE_RTC

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
    }
#endif
    pthread_create(&core_threads[CORE_THREAD_LED_CTRL], NULL, &LEDCtrl_thread, NULL);
    LEDCtrl_set_state(LED_STATE_FPGA);

#if ENABLE_BURNWIRE
    init_burnwire();
#endif

#if ENABLE_RECOVERY
    // start recovery board hardware so it can start booting
    wt_recovery_init();
#endif

    // thread to manage acqusition threads
    threadManager_init();

    // Handle user commands.
    pthread_create(&core_threads[CORE_THREAD_COMMAND_PIPE], NULL, &command_thread, NULL);

    return 0;
}

int main(int argc, char *argv[]) {
    g_exit = 0;

    // Define callbacks for handling signals.
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    if (0 != core_init()) {
        CETI_ERR("Failed to initialize tag!!!");
        return -1;
    }

    // Run the state machine.
    init_stateMachine();

    //-----------------------------------------------------------------------------
    // Run the application!
    // !!!! THIS IS A LOOP !!!!
    stateMachine_thread(NULL);

    //-----------------------------------------------------------------------------
    // Join all core threads!
    struct timespec ts;

    // grab timestamp
    clock_gettime(CLOCK_REALTIME, &ts);

    // set timeout
    ts.tv_sec += CORE_THREAD_JOIN_TIMEOUT_S;

    // check that all acquisition threads stop
    for (int thread_index = 0; thread_index < NUM_CORE_THREAD; thread_index++) {
        if (pthread_timedjoin_np(core_threads[thread_index], NULL, &ts) != 0) {
            CETI_ERR("%s thread failed to stop. Cancelling thread", core_thread_names[thread_index]);
            pthread_cancel(core_threads[thread_index]);
        }
    }

    //-----------------------------------------------------------------------------
    // Tag wide cleanup
    CETI_LOG("Tag-wide cleanup");
    gpioTerminate();

    CETI_LOG("Done!");
    if (ST_SHUTDOWN == stateMachine_get_state()) {
        // shut down system
        sync();
        reboot(LINUX_REBOOT_CMD_POWER_OFF);
    }
    return (0);
}
