//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "timing.h"

#include "../device/rtc.h"
#include "../launcher.h" // for g_exit, the state machine data filepath, to get an initial RTC timestamp if needed
#include "../systemMonitor.h"
#include "logging.h"

#include <pthread.h> // to set CPU affinity
#include <sys/time.h>
#include <sys/timex.h>

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_rtc_thread_is_running = 0;
static int latest_rtc_count = -1;
static int latest_rtc_error = WT_OK;
static int64_t last_rtc_update_time_us = -1;

int init_timing() {
#if ENABLE_RTC
    // Test whether the RTC is available.
    updateRtcCount();
    if (latest_rtc_error != WT_OK) {
        CETI_ERR("Failed to fetch a valid RTC count: %s", wt_strerror(latest_rtc_error));
        latest_rtc_count = -1;
        last_rtc_update_time_us = -1;
        return (-1);
    }

#endif

    CETI_LOG("Successfully initialized timing");
    return 0;
}

//-----------------------------------------------------------------------------
// RTC second counter
//-----------------------------------------------------------------------------
int getRtcCount() { return latest_rtc_count; }
WTResult getRtcStatus(void) { return latest_rtc_error; }

void updateRtcCount() {
    uint32_t count_s = 0;
    
    sync_global_time_init();

    latest_rtc_error = rtc_get_count(&count_s);
    if (latest_rtc_error == WT_OK) {
        latest_rtc_count = (int)count_s;
        last_rtc_update_time_us = get_global_time_us();
    } else {
        latest_rtc_count = -1;
        last_rtc_update_time_us = -1;
    }
}

// Thread to update the latest RTC time, to use the I2C bus more sparingly
//  instead of having all other threads that request RTC use the bus.
void *rtc_thread(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_rtc_thread_tid = gettid();

    // Set the thread CPU affinity.
    if (RTC_CPU >= 0) {
        pthread_t thread;
        thread = pthread_self();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(RTC_CPU, &cpuset);
        if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
            CETI_LOG("Successfully set affinity to CPU %d", RTC_CPU);
        else
            CETI_WARN("Failed to set affinity to CPU %d", RTC_CPU);
    }

    // Do an initial RTC update.
    updateRtcCount();

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    int old_rtc_count = -1;
    int delay_duration_us = 0;
    int prev_num_updates_required = 0;
    g_rtc_thread_is_running = 1;
    while (!g_exit) {
        // Wait the long polling period since the last update.
        // Unless the last time only required a single update to find a new RTC
        // value, in which case we might be out of step with the RTC clock and we
        // should use fast polling again to get near the RTC update boundary.
        //    Note that this is hopefully only the case on the first loop.
        if (prev_num_updates_required > 1) {
            delay_duration_us =
                (last_rtc_update_time_us + RTC_UPDATE_PERIOD_LONG_US) -
                get_global_time_us();
            if (delay_duration_us > RTC_UPDATE_PERIOD_SHORT_US)
                usleep(delay_duration_us);
        }
        // Update the RTC until its value changes, using the faster polling
        // period.
        old_rtc_count = latest_rtc_count;
        prev_num_updates_required = 0;
        while (latest_rtc_count == old_rtc_count) {
            usleep(RTC_UPDATE_PERIOD_SHORT_US);
            updateRtcCount();
            prev_num_updates_required++;
        }
    }
    g_rtc_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Global time
//-----------------------------------------------------------------------------
int64_t get_global_time_us() {
    struct timeval current_timeval;
    int64_t current_time_us;

    gettimeofday(&current_timeval, NULL);
    current_time_us = (int64_t)(current_timeval.tv_sec * 1000000LL) +
                      (int64_t)(current_timeval.tv_usec);
    return current_time_us;
}

int64_t get_global_time_ms() {
    int64_t current_time_ms;
    struct timeval current_timeval;
    gettimeofday(&current_timeval, NULL);
    current_time_ms = (int64_t)(current_timeval.tv_sec * 1000LL) +
                      (int64_t)(current_timeval.tv_usec / 1000);
    return current_time_ms;
}

int64_t get_global_time_s(void) {
    struct timeval current_timeval;
    gettimeofday(&current_timeval, NULL);
    return (int64_t)(current_timeval.tv_sec);
}

int sync_global_time_init(void) {
    struct timex timex_info = {.modes = 0};
    int ntp_result = ntp_adjtime(&timex_info);
    int ntp_synchronized = (ntp_result >= 0) && (ntp_result != TIME_ERROR);

    if (ntp_synchronized) {
        struct timeval current_timeval;
        gettimeofday(&current_timeval, NULL);

        WTResult hw_result = rtc_set_count((uint32_t)current_timeval.tv_sec);
        if (hw_result != WT_OK) {
            CETI_ERR("Could not syncronize RTC: %s", wt_strerror(hw_result));
        }
        CETI_LOG("RTC synchronized to system clock: %ld)", current_timeval.tv_sec);
    } else {
        /* ToDo: GPS clock syncronization
         * MSH - This would require GPS lock on recovery board at this point in code
         */

        CETI_LOG("Synchronizing system clock to RTC)");
        struct timeval current_timeval = {.tv_sec = getRtcCount()};
        if (latest_rtc_error != WT_OK) {
            CETI_ERR("Could not read time from RTC: %s", wt_strerror(latest_rtc_error));
            /* ToDo: how do we handle this error? Maybe update to last recorded time in a given file?*/
        }
        settimeofday(&current_timeval, NULL);
    }
    return 0;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#ifdef UNIT_TEST
struct tm s_fake_time;
void set_fake_time(const struct tm *tm_s) {
    s_fake_time = *tm_s;
}
#endif
int64_t get_next_time_of_day_occurance_s(const struct tm *time_of_day) {
    struct tm tm = {};
    time_t current_time, next_time;

    // copy tm struct from input/config
    memcpy(&tm, time_of_day, sizeof(tm));

    // Get the current time
#ifndef UNIT_TEST
    time(&current_time);
    struct tm *current_tm = gmtime(&current_time); // Get current local time as struct tm
#else
    struct tm *current_tm = &s_fake_time;
    current_time = timegm(current_tm);

#endif

    // set to today's date
    tm.tm_year = current_tm->tm_year;
    tm.tm_mon = current_tm->tm_mon;
    tm.tm_mday = current_tm->tm_mday;
    next_time = timegm(&tm);

    // check that time hasn't already happened
    if (next_time <= current_time) {
        tm.tm_mday += 1;
        next_time = timegm(&tm);
    }

    // return epoch time
    return next_time;
}
