//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin,
//               Joseph DelPreto     (delpreto@csail.mit.edu),
//               Michael Salino-Hugg (msalinohugg@seas.harvard.edu),
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "state_machine.h"

#include "battery.h"
#include "burnwire.h"
#include "launcher.h" // for g_exit, g_stopLogging sampling rate, data filepath, and CPU affinity
#include "led_ctrl.h"
#include "recovery.h"
#include "sensors/imu.h" // for recovery float detection
#include "sensors/pressure_temperature.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update

#include "utils/config.h"
#include "utils/logging.h"
#include "utils/power.h"
#include "utils/str.h"    //for strtoidentifier
#include "utils/timing.h" //for get_global_time_us(), getRtcCount()

#include <errno.h>
#include <math.h>    // for M_PI
#include <pthread.h> // to set CPU affinity
#include <stdint.h>
#include <stdio.h>  // for FILE
#include <stdlib.h> // for atof, atol, strtoul, etc
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h> // gethostname

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

typedef double f64;

// Global/static variables
//-----------------------------------------------------------------------------

static const char *state_str[] = {
    [ST_START] = "START",
    [ST_RECORD_DIVING] = "RECORD_DIVING",
    [ST_RECORD_FLOATING] = "RECORD_FLOATING",
    [ST_RECORD_SURFACE] = "RECORD_SURFACE",
    [ST_BRN_ON] = "BRN_ON",
    [ST_LOW_POWER_BURN] = "LOW_POWER_BURN",
    [ST_RETRIEVE] = "RETRIEVE",
    [ST_SHUTDOWN] = "SHUTDOWN",
    [ST_UNKNOWN] = "UNKNOWN"};

static int presentState = ST_UNKNOWN;
static unsigned int start_time_s = 0;
static int s_state_machine_paused = 0;

// Output file
static const char *stateMachine_data_file_headers[] = {
    "State To Process",
    "Next State",
};
static const int num_stateMachine_data_file_headers = sizeof(stateMachine_data_file_headers) / sizeof(*stateMachine_data_file_headers);

//-----------------------------------------------------------------------------
// DEPTH DETECTION
//-----------------------------------------------------------------------------
static int __at_depth(void) {
#if ENABLE_PRESSURETEMPERATURE_SENSOR
    return ((g_pressure->error == WT_OK) && (g_pressure->pressure_bar > g_config.dive_pressure));
#else
    return 0;
#endif
}

static int __at_surface(void) {
#if ENABLE_PRESSURETEMPERATURE_SENSOR
    return (g_pressure->error != WT_OK) || (g_pressure->pressure_bar < g_config.surface_pressure);
#else
    return 1;
#endif
}

//-----------------------------------------------------------------------------
// FLOAT_DETECTION
//-----------------------------------------------------------------------------
#define FLOAT_DETECT_SMOOTHING_COUNT 10
#define FLOAT_DETECT_TARGET_PITCH_DEG (-85.0) // pitch imperically found to not be -90.0 probably due to suction cups (MSH)
#define FLOAT_DETECT_TARGET_ROLL_DEG (0.0)
#define FLOAT_DETECT_ANGLE_RANGE_DEG (10.0)
#define FLOAT_DETECT_HOLD_TIME MIN_TO_SEC(20)
#define FLOAT_DETECT_SMOOTHING_COUNT 10

static f64 imu_d_pitch_norm_deg[FLOAT_DETECT_SMOOTHING_COUNT] = {};
static f64 imu_d_roll_norm_deg[FLOAT_DETECT_SMOOTHING_COUNT] = {};
static int imu_buffer_offset = 0;
static f64 imu_abs_d_pitch_sum_deg = 0.0;
static f64 imu_abs_d_roll_sum_deg = 0.0;
static int float_start_detected = 0;
static time_t float_start_time_s;

static f64 p_error_average = 0.0;
static f64 r_error_average = 0.0;

static int __oriented_upright(void) {
#if ENABLE_IMU
    return ((p_error_average < 10.0) && (r_error_average < 10.0));
#else
    return 0;
#endif
}

static void __reset_float_detection(void) {
    float_start_detected = 0;

    // reset buffer
    bzero(imu_d_pitch_norm_deg, sizeof(imu_d_pitch_norm_deg));
    bzero(imu_d_roll_norm_deg, sizeof(imu_d_roll_norm_deg));
    imu_buffer_offset = 0;
    imu_abs_d_pitch_sum_deg = 0.0f;
    imu_abs_d_roll_sum_deg = 0.0f;
}

static void __update_float_detection(void) {
    EulerAngles_f64 latest_euler;
    if (imu_get_latest_rotation_euler(&latest_euler) != 0) {
        return;
    }

    f64 d_pitch_norm = fabs(FLOAT_DETECT_TARGET_PITCH_DEG - (latest_euler.pitch * M_PI / 180.0));
    f64 d_roll_norm = fabs(FLOAT_DETECT_TARGET_ROLL_DEG - (latest_euler.roll * M_PI / 180.0));

    imu_abs_d_pitch_sum_deg -= imu_d_pitch_norm_deg[imu_buffer_offset];
    imu_abs_d_roll_sum_deg -= imu_d_roll_norm_deg[imu_buffer_offset];

    imu_d_pitch_norm_deg[imu_buffer_offset] = d_pitch_norm;
    imu_d_roll_norm_deg[imu_buffer_offset] = d_roll_norm;

    imu_abs_d_pitch_sum_deg += imu_d_pitch_norm_deg[imu_buffer_offset];
    imu_abs_d_roll_sum_deg += imu_d_roll_norm_deg[imu_buffer_offset];

    imu_buffer_offset = (imu_buffer_offset + 1) % FLOAT_DETECT_SMOOTHING_COUNT;

    p_error_average = imu_abs_d_pitch_sum_deg / FLOAT_DETECT_SMOOTHING_COUNT;
    r_error_average = imu_abs_d_pitch_sum_deg / FLOAT_DETECT_SMOOTHING_COUNT;

    // is floating
    if (!__at_depth() && __oriented_upright()) {
        if (!float_start_detected) {
            float_start_time_s = get_monotonic_time_s();
            float_start_detected = 1;
        }
    }

    // is not floating so reset if needed
    if (float_start_detected) {
        __reset_float_detection();
    }
}

static int __is_floating(void) {
#if FLOAT_DETECTION
    return (float_start_detected && (get_monotonic_time_s() - float_start_time_s > FLOAT_DETECT_HOLD_TIME));
#else
    return 0;
#endif // FLOAT_DETECTION
}

//-----------------------------------------------------------------------------
// NETWORKING CHECKS
//-----------------------------------------------------------------------------

static int __is_charging(void) {
#if ENABLE_BATTERY_GAUGE
    if (shm_battery->error != WT_OK) {
        return 1; // keeps wifi on if BMS is failing to communicate
    }

    return (shm_battery->current_mA > -10.0);
#else
    return 0;
#endif // ENABLE_BATTERY_GAUGE
}

static void __update_networking(void) {
    // Turn off networking if the grace period has passed.
    if (networking_is_enabled() && !networking_ssh_session_active() && (get_monotonic_time_s() - start_time_s > MIN_TO_SEC(WIFI_GRACE_PERIOD_MIN)) && !__is_charging()) {
        networking_disable();
    }
}

//-----------------------------------------------------------------------------
// Low Memory Checks
//-----------------------------------------------------------------------------
static uint64_t __void_free_data_bytes(void) {
    struct statvfs fs = {};
    statvfs("/data", &fs);
    uint64_t available_bytes = fs.f_bfree * fs.f_bsize;
    return available_bytes;
}

static int __is_low_on_memory(void) {
    uint64_t available_GB = (__void_free_data_bytes() >> 20);
    return (available_GB < 1);
}

//-----------------------------------------------------------------------------
// Voltage Checks
//-----------------------------------------------------------------------------
static int battery_low_voltage_count = 0;
static int battery_critical_voltage_count = 0;
static int s_bms_error_count = 0;

void reset_voltage_counters(void) {
    battery_low_voltage_count = 0;
    battery_critical_voltage_count = 0;
    s_bms_error_count = 0;
}

static void __update_voltage_counters(void) {
#if ENABLE_BATTERY_GAUGE
    if (shm_battery->error == WT_OK) {
        s_bms_error_count = 0;

        if ((shm_battery->cell_voltage_v[0] < g_config.release_voltage_v) || (shm_battery->cell_voltage_v[1] < g_config.release_voltage_v)) {
            battery_low_voltage_count++;
        } else {
            battery_low_voltage_count = 0;
        }

        if ((shm_battery->cell_voltage_v[0] < g_config.critical_voltage_v) || (shm_battery->cell_voltage_v[1] < g_config.critical_voltage_v)) {
            battery_critical_voltage_count++;
        } else {
            battery_critical_voltage_count = 0;
        }
    } else {
        // report new errors
        if (s_bms_error_count == 0) {
            char err_str[512];
            CETI_ERR("BMS reading resulted in error: %s", wt_strerror_r(shm_battery->error, err_str, sizeof(err_str)));
        }

        s_bms_error_count++;

        // burn if consistently in error
        if (s_bms_error_count == MISSION_BMS_CONSECUTIVE_ERROR_THRESHOLD) {
            char err_str[512];
            CETI_ERR("BMS remained in error for %d samples: %s", s_bms_error_count, wt_strerror_r(shm_battery->error, err_str, sizeof(err_str)));
        }
    }
#endif
}

static int __is_low_votage(void) {
#if ENABLE_BATTERY_GAUGE
    return (battery_low_voltage_count >= BATTERY_LOW_VOLTAGE_CONSECUTIVE_THRESHOLD);
#else
    return 0;
#endif
}

static int __is_critical_votage(void) {
#if ENABLE_BATTERY_GAUGE
    return (battery_critical_voltage_count >= BATTERY_CRITICAL_VOLTAGE_CONSECUTIVE_THRESHOLD);
#else
    return 0;
#endif
}

static int __in_bms_error(void) {
#if ENABLE_BATTERY_GAUGE
    return (s_bms_error_count >= MISSION_BMS_CONSECUTIVE_ERROR_THRESHOLD);
#else
    return 0;
#endif
}

//-----------------------------------------------------------------------------
// Burnwire Checks
//-----------------------------------------------------------------------------
// RTC counts
typedef enum {
    BSS_NONE,
    BSS_FILE,
    BSS_RTC,
    BSS_NTP,
} BurnStartSource;

static BurnStartSource burnwire_start_source_s = BSS_NONE;
static unsigned int burnwire_timeout_start_s = 0;
static int64_t burnwire_time_of_day_release_s = 0;
static uint32_t burnwire_started_time_s = 0;

/**
 * @brief  resyncronizes burnwire timings if more accurate realtime timestamp available
 *
 * @return
 */
static void __burnwire_timing_update(void) {
    // Resyncronize clock if networking still up and time has never synced
    if (networking_is_enabled() && !timing_has_syncronized_to_ntp()) {
        timing_syncronize_to_ntp();
        // update burn time if previous burn time was generated via the RTC (not file or NTP)
        if (timing_has_syncronized_to_ntp() && (burnwire_start_source_s == BSS_RTC)) {
            burnwire_start_source_s = BSS_NTP;
            burnwire_timeout_start_s = get_global_time_s();
            CETI_LOG("Updating burnwire timeout start time %u", burnwire_timeout_start_s);

            if (g_config.tod_release.valid) {
                burnwire_time_of_day_release_s = get_next_time_of_day_occurance_s(&g_config.tod_release.value);
                CETI_LOG("Time of day release updated to %lu", burnwire_time_of_day_release_s);
            }
        }
    }
}

//-----------------------------------------------------------------------------
// State Machine control
//-----------------------------------------------------------------------------
static int stateMachine_log_restarted = 0;
int init_stateMachine() {
    CETI_LOG("Successfully initialized the state machine");
    // Open an output file to write data.
    if (init_data_file(STATEMACHINE_DATA_FILEPATH,
                       stateMachine_data_file_headers, num_stateMachine_data_file_headers,
                       NULL, "init_stateMachine()") < 0) {
        return -1;
    }
    stateMachine_log_restarted = 1;

    return 0;
}

//-----------------------------------------------------------------------------
// State Machine and Controls
// * Details of state machine are documented in the high-level design
//-----------------------------------------------------------------------------
wt_state_t stateMachine_get_state(void) {
    return presentState;
}

int stateMachine_set_state(wt_state_t new_state) {
    static int s_sensor_acq_stopped = 1;
    static int s_burnwire_on = 0;

    // nothing to do
    if (new_state == presentState) {
        // CETI_LOG("Already in state %s", get_state_str(presentState));
        return 0;
    }

    // skip burn states if no burnwire hardware
#if !ENABLE_BURNWIRE
    if (new_state == ST_BRN_ON) {
        new_state = ST_RETRIEVE;
    } else if (new_state == ST_LOW_POWER_BURN) {
        new_state = ST_SHUTDOWN;
    }
#endif

    if (ST_START == new_state) {
        // Record the time of startup (used to keep wifi-enabled)
        start_time_s = get_monotonic_time_s();
        // Create files with the configuration and other metadata.
        // Wait a bit after startup, so the system clock can be adjusted.
        threadManager_create_thread(ACQ_THREAD_);
    }

    // check if data acquisition threads can be disabled
    if ((ST_LOW_POWER_BURN == new_state) || (ST_SHUTDOWN == new_state)) {
        threadManager_stop_acquisition_threads();
    } else {
        threadManager_start_acquisition_threads();
    }

#if ENABLE_BURNWIRE
    // check if burnwire should be on
    if ((ST_LOW_POWER_BURN == new_state) || (ST_BRN_ON == new_state)) {
        if (!s_burnwire_on) {
            burnwireOn();
            burnwire_started_time_s = get_global_time_s();
            // Clear the persistent burnwire timeout start time if one exists.
            remove(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH);
            s_burnwire_on = 1;
        }
    } else {
        if (s_burnwire_on) {
            burnwireOff();
            s_burnwire_on = 0;
        }
    }

    if (ST_RECORD_DIVING == new_state) {
        // Record this time as the burnwire timeout start time if one has not already been recorded
        // since we now know that this is a real deployment.
        if (access(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, F_OK) == -1) {
#ifndef UNIT_TEST
            burnwire_timeout_start_s = get_global_time_s();
            if (timing_has_syncronized_to_ntp()) {
                burnwire_start_source_s = BSS_NTP;
            } else {
                burnwire_start_source_s = BSS_RTC;
            }
            CETI_LOG("Starting dive; recording burnwire timeout start time %u", burnwire_timeout_start_s);
            FILE *file_burnwire_timeout_start_s = NULL;
            file_burnwire_timeout_start_s = fopen(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, "w");
            if (file_burnwire_timeout_start_s != NULL) {
                fprintf(file_burnwire_timeout_start_s, "%u", burnwire_timeout_start_s);
                fclose(file_burnwire_timeout_start_s);
            } else {
                CETI_WARN("Failed to create %s: %s", STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, strerror(errno));
            }
#endif
        }
    }
#endif

#if ENABLE_RECOVERY
    // check state recovery board should be on
    if (g_config.recovery.enabled) {
        if (ST_RECORD_DIVING == new_state) {
            recovery_sleep();
        } else if (ST_RECORD_SURFACE == new_state) {
            recovery_gps_only();
        } else {
            recovery_wake();

            // set current state in message
            char hostname[32];
            gethostname(hostname, 31);

            char comment[41] = {};
            snprintf(comment, 40, "%s %s", hostname, get_state_str(new_state));
            // set recovery board comment
            recovery_set_comment(comment);
        }
    }
#endif // ENABLE_RECOVERY

    // update state
    CETI_LOG("State transition: %s -> %s\n", get_state_str(presentState), get_state_str(new_state));
    presentState = new_state;
    return 0;
}

void stateMachine_pause(void) {
    s_state_machine_paused = 1;
}

void stateMachine_resume(void) {
    s_state_machine_paused = 0;
}

int updateStateMachine() {
    switch (presentState) {
        // ---------------- Startup ----------------
        case (ST_START): {
            // See if a start time for burnwire timeouts has been previously saved.
            // This would happen if there was an unexpected shutdown during a deployment.
            // If the file is present, use the timestamp it contains.
            // Otherwise, set the timeout start to the current time.
            // Note that the target time will be at first dive to ensure it's a real deployment,
            // but will use current time for now in case there is never a dive.
            burnwire_timeout_start_s = get_global_time_s(); // default to the current time
            if (timing_has_syncronized_to_ntp()) {
                burnwire_start_source_s = BSS_NTP;
            } else {
                burnwire_start_source_s = BSS_RTC;
            }
            FILE *file_burnwire_timeout_start_s = NULL;
            char line[512];
            file_burnwire_timeout_start_s = fopen(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, "r");
            if (file_burnwire_timeout_start_s != NULL) {
                CETI_LOG("Loading a previously saved burnwire timeout start time");
                char *fgets_result = fgets(line, 512, file_burnwire_timeout_start_s);
                fclose(file_burnwire_timeout_start_s);
                if (fgets_result != NULL) {
                    unsigned int loaded_start_time_s = strtoul(line, NULL, 10);
                    if (loaded_start_time_s != 0) // will be 0 if the conversion to integer failed
                        burnwire_timeout_start_s = loaded_start_time_s;
                    burnwire_start_source_s = BSS_FILE;
                }
            }
            CETI_LOG("Using the following burnwire timeout start time: %u", burnwire_timeout_start_s);
            if (g_config.tod_release.valid) {
                burnwire_time_of_day_release_s = get_next_time_of_day_occurance_s(&g_config.tod_release.value);
                CETI_LOG("Time of day release set to %lu", burnwire_time_of_day_release_s);
            }

#if ENABLE_RECOVERY
            // configure recovery board
            if (g_config.recovery.enabled) {
                // send wake message
                char hostname[512];
                gethostname(hostname, 511);

                char message[1024];
                snprintf(message, sizeof(message), "CETI %s ready!", hostname);
                recovery_message(message);

                char rec_callsign_msg[10];
                char callsign_msg[10];
                callsign_to_str(&g_config.recovery.callsign, callsign_msg);
                callsign_to_str(&g_config.recovery.recipient, rec_callsign_msg);
                CETI_LOG("Recovery configured: %s -> %s @ %7.3f MHz", callsign_msg, rec_callsign_msg, g_config.recovery.freq_MHz);
            } else {
                CETI_LOG("Recovery disabled");
            }
#endif // ENABLE_RECOVERY

            // Transition to the appropriate recording state.
            if (__at_depth()) {
                stateMachine_set_state(ST_RECORD_DIVING);
            } else {
                stateMachine_set_state(ST_RECORD_SURFACE);
            }

            break;
        }

        // Recording while at surface, trying to get a GPS fix
        case (ST_RECORD_SURFACE): {
            if (__is_low_on_memory()) {
                CETI_LOG("LOW MEMORY!!! Initializing Burn");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            // Turn on the burnwire if the timeout has passed since the deployment started.
            if (get_global_time_s() - burnwire_timeout_start_s > g_config.timeout_s) {
                CETI_LOG("TIMEOUT!!! Initializing Burn (%ld - %d > %ld)", get_global_time_s(), burnwire_timeout_start_s, g_config.timeout_s);
                stateMachine_set_state(ST_BRN_ON);
                break;
            } else if (g_config.tod_release.valid && (burnwire_time_of_day_release_s < get_global_time_s())) {
                CETI_LOG("Time of day release!!! Initializing Burn");
                stateMachine_set_state(ST_BRN_ON);
                break;
            }

            if (__in_bms_error()) {
                CETI_LOG("BMS ERROR!!! Initializing Burn");
                stateMachine_set_state(ST_BRN_ON);
                break;
            }

            if (__is_low_votage()) {
                CETI_LOG("LOW VOLTAGE!!! Initializing Burn from Surface");
                stateMachine_set_state(ST_BRN_ON);
                break;
            }

            if (__at_depth()) {
                stateMachine_set_state(ST_RECORD_DIVING); // back down...
                break;
            }

            if (__is_floating()) {
                stateMachine_set_state(ST_RECORD_FLOATING);
                break;
            }

            break;
        }

        // Recording while sumberged
        case (ST_RECORD_DIVING): {
            if (__is_low_on_memory()) {
                CETI_LOG("LOW MEMORY!!! Initializing Burn");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            // Turn on the burnwire if the timeout has passed since the deployment started.
            if ((get_global_time_s() - burnwire_timeout_start_s) > g_config.timeout_s) {
                CETI_LOG("TIMEOUT!!! Initializing Burn (%ld - %d > %ld)", get_global_time_s(), burnwire_timeout_start_s, g_config.timeout_s);
                stateMachine_set_state(ST_BRN_ON);
                break;
            } else if (g_config.tod_release.valid && (burnwire_time_of_day_release_s < get_global_time_s())) {
                CETI_LOG("Time of day release!!! Initializing Burn");
                stateMachine_set_state(ST_BRN_ON);
                break;
            }

            // Turn on the burnwire if the battery voltage is low.
            if (__in_bms_error()) {
                CETI_LOG("BMS ERROR!!! Initializing Burn");
                stateMachine_set_state(ST_BRN_ON);
                break;
            }

            if (__is_low_votage()) {
                CETI_LOG("LOW VOLTAGE!!! Initializing Burn from Surface");
                stateMachine_set_state(ST_BRN_ON);
                break;
            }

            // Transition state if at the surface.
            if (__at_surface()) {
                stateMachine_set_state(ST_RECORD_SURFACE); // came to surface
                break;
            }
            break;
        }

        // Recording while likely detatched from whale
        case (ST_RECORD_FLOATING): {
            if (__is_low_on_memory()) {
                CETI_LOG("LOW MEMORY!!! Initializing Burn from floating");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            if (__in_bms_error()) {
                CETI_LOG("BMS IN ERROR!!! Initializing Burn from floating");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            if (__is_low_votage()) {
                CETI_LOG("LOW VOLTAGE!!! Initializing Burn from floating");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            if (__at_depth()) {
                stateMachine_set_state(ST_RECORD_DIVING);
            }

            if (!__oriented_upright()) {
                stateMachine_set_state(ST_RECORD_SURFACE);
                break;
            }

            break;
        }

        // Releasing via the burnwire
        case (ST_BRN_ON): {
            if (__is_low_on_memory()) {
                CETI_LOG("LOW MEMORY!!! Initializing Burn");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            // Shutdown if the battery is too low.
            if (__in_bms_error()) {
                CETI_LOG("BMS ERROR!!! Disabling sensors");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            if (__is_critical_votage()) {
                CETI_LOG("CRITICAL VOLTAGE!!! Disabling sensors");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            if (__is_low_votage()) {
                CETI_LOG("LOW VOLTAGE!!! Initializing Burn from Surface");
                stateMachine_set_state(ST_LOW_POWER_BURN);
                break;
            }

            // switch state once the burn is complete
            if (get_global_time_s() - burnwire_started_time_s > g_config.burn_interval_s) {
                stateMachine_set_state(ST_RETRIEVE);
            }
            break;
        }

        // Releasing via the burnwire
        case (ST_LOW_POWER_BURN): {
            // switch state once the burn is complete
            if (get_global_time_s() - burnwire_started_time_s > g_config.burn_interval_s) {
                stateMachine_set_state(ST_SHUTDOWN);
            }
            break;
        }

        //  Waiting to be retrieved.
        case (ST_RETRIEVE): {
            if (__is_low_on_memory()) {
                CETI_LOG("LOW MEMORY!!! Shutting down");
                stateMachine_set_state(ST_SHUTDOWN);
                break;
            }

            // Shutdown if the battery is too low.
            if (__in_bms_error()) {
                CETI_LOG("BMS ERROR!!! Shutting down");
                stateMachine_set_state(ST_SHUTDOWN);
                break;
            }

            if (__is_low_votage()) {
                CETI_LOG("LOW VOLTAGE!!! Shutting down");
                stateMachine_set_state(ST_SHUTDOWN);
                break;
            }

            if (__is_floating()) {
                CETI_LOG("Floating at surface detected. Entering low power.");
                stateMachine_set_state(ST_SHUTDOWN);
                break;
            }
            break;
        }

        //  Shut everything off in an orderly way if battery is critical to
        //  reduce file system corruption risk
        case (ST_SHUTDOWN): {
            break;
        }

        default:
            CETI_ERR("Tag in unknown mission state. Restarting state machine");
            stateMachine_set_state(ST_START);
            break;
    }
    return (0);
}

// Helper to convert a state ID to a printable string.
__attribute__((const))
const char *
get_state_str(wt_state_t state) {
    if ((state < ST_START) || (state > ST_UNKNOWN)) {
        CETI_LOG("presentState is out of bounds. Setting to ST_UNKNOWN. Current value: %d", presentState);
        state = ST_UNKNOWN;
    }
    return state_str[state];
}

wt_state_t strtomissionstate(const char *_String, const char **_EndPtr) {
    wt_state_t state = ST_UNKNOWN;
    const char *end_ptr = NULL;
    const char *name = strtoidentifier(_String, &end_ptr);
    if (name != NULL) {
        size_t len = end_ptr - name;

        for (state = ST_START; state < ST_UNKNOWN; state++) {
            if (len != strlen(state_str[state])) {
                continue;
            }

            if (memcmp(name, state_str[state], len) == 0) {
                break;
            }
        }

    } else {
        // skip whitespace
        char *e_ptr;
        state = strtoul(_String, &e_ptr, 0);
        if (state > ST_UNKNOWN || (state == 0 && e_ptr == _String)) {
            state = ST_UNKNOWN;
        }
        end_ptr = e_ptr;
    }

    if (_EndPtr != NULL) {
        *_EndPtr = end_ptr;
    }
    return state;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *stateMachine_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    volatile ThreadParam *pParam = (volatile ThreadParam *)paramPtr;
    pParam->tid = gettid();

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically update state");
    while (!g_exit) {
        // Acquire timing information for when the next state will begin processing.
        int64_t clock_time_us = get_monotonic_time_us();

        if (!s_state_machine_paused) {
            int64_t global_time_us = get_global_time_us();
            int current_rtc_count_s = getRtcCount();
            wt_state_t state_to_process = presentState;

            // update detection values that should always be updated for the mission
            // state machine to work
            if ((ST_START != presentState) && (ST_UNKNOWN > presentState)) {
                __update_float_detection();
                __update_voltage_counters();
                __burnwire_timing_update();
                __update_networking();
            }

            // Process the next state.
            updateStateMachine();

            // Write state information to the data file.
            if (!g_stopLogging) {
                FILE *stateMachine_data_file = fopen(STATEMACHINE_DATA_FILEPATH, "at");
                if (stateMachine_data_file == NULL) {
                    CETI_LOG("failed to open data output file: %s", STATEMACHINE_DATA_FILEPATH);
                } else {
                    // Write timing information.
                    fprintf(stateMachine_data_file, "%ld", global_time_us);
                    fprintf(stateMachine_data_file, ",%d", current_rtc_count_s);
                    // Write any notes, then clear them so they are only written once.
                    fprintf(stateMachine_data_file, ",");
                    if (stateMachine_log_restarted) {
                        fprintf(stateMachine_data_file, "Restarted! | ");
                    }
                    stateMachine_log_restarted = 0;
                    // Write the sensor data.
                    fprintf(stateMachine_data_file, ",%s", get_state_str(state_to_process));
                    fprintf(stateMachine_data_file, ",%s", get_state_str(presentState));
                    // Finish the row of data and close the file.
                    fprintf(stateMachine_data_file, "\n");
                    fclose(stateMachine_data_file);
                }
            }
        }

        // Delay to implement a desired sampling rate.
        // Take into account the time it took to process the state.
        int64_t polling_sleep_duration_us = STATEMACHINE_UPDATE_PERIOD_US;
        polling_sleep_duration_us -= get_monotonic_time_us() - clock_time_us;
        if (polling_sleep_duration_us > 0) {
            usleep(polling_sleep_duration_us);
        }
    }

    // Clear the persistent burnwire timeout start time if one exists.
    remove(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH);

    CETI_LOG("Done!");
    return NULL;
}