//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "../launcher.h"
#include "../systemMonitor.h"
#include "../utils/logging.h"
#include "../utils/timing.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define VIDEO_FILE_LENGTH_MS 300000

// 480x640p90, 1280x720p60, 1920x1080p30
#define VIDEO_WIDTH 1920
#define VIDEO_HEIGHT 1080
#define VIDEO_FRAMERATE 30

#define xstr(s) stringify(s)
#define stringify(s) #s

int g_video_thread_is_running = 0;

void *video_thread(void *paramPtr) {
    g_video_thread_tid = gettid();

    pid_t pid;
    int64_t video_start_time_us = get_global_time_us();
    pid = fork();
    if (pid == 0) {
        // the spawned child runs this code
        char video_filename[256] = "";
        sprintf(video_filename, "/data/%ld.h264", get_global_time_us());
        char *argv[] = {
            "libcamera-vid",
            "-t", xstr(VIDEO_FILE_LENGTH_MS),
            "--inline",
            "-v", "0",
            "--width", xstr(VIDEO_WIDTH),
            "--height", xstr(VIDEO_HEIGHT),
            "--framerate", xstr(VIDEO_FRAMERATE),
            "-o", video_filename,
            NULL};
        execvp("libcamera-vid", argv);
        // we only get here if the child failed
        CETI_ERR("failed to created libcamera-vid child process");
        exit(1);
    }
    g_video_thread_is_running = 1;

    while (!g_stopAcquisition) {
        int64_t timestamp_us = get_global_time_us();

        // start a new video
        if ((timestamp_us - video_start_time_us) >= (VIDEO_FILE_LENGTH_MS * 1000)) {
            // wait for old pid to finish
            waitpid(pid, NULL, 0); // Wait for child to finish

            pid = fork();
            video_start_time_us = get_global_time_us();
            if (pid == 0) {
                // the spawned child process runs this code
                char video_filename[256] = "";
                sprintf(video_filename, "%ld.h264", get_global_time_us());
                char *argv[] = {
                    "libcamera-vid", "-t", xstr(VIDEO_FILE_LENGTH_MS), "--inline", "-o", video_filename, NULL};
                execvp("libcamera-vid", argv);
                // we only get here if the child failed
                CETI_ERR("failed to created libcamera-vid child process");
                exit(1);
            }
        }

        // sleep a bit
        int64_t elapsed_time_us = get_global_time_us() - timestamp_us;
        int64_t sleep_time_us = 1000000 - elapsed_time_us;
        if (sleep_time_us > 0) {
            usleep(sleep_time_us);
        }
    }

    // kill video process
    printf("Stopping recording early...\n");
    kill(pid, SIGINT);

    waitpid(pid, NULL, 0); // Wait for child to finish
    g_video_thread_is_running = 0;
    return NULL;
}