#include "meta.h"

#include "../launcher.h"
#include "config.h"
#include "timing.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define META_FILE_PATH "/opt/ceti-tag-data-capture/config/tag-info.yaml"

#define FILE_LOG_TIMEOUT_S (60)

int meta_log(uint64_t timestamp) {
    int fd_dst, fd_src;
    char buf[4096];
    int nread;
    char meta_file_path[256];

    // Create a name for the info file.
    // Append a number to the filename base until one is found that doesn't exist yet.
    int filename_postfix_count = 0;
    int filename_exists = 0;
    do {
        if (filename_postfix_count == 0) {
            snprintf(meta_file_path, 255, "/data/data_tag_info_%lu.yaml", timestamp);
        } else {
            snprintf(meta_file_path, 255, "/data/data_tag_info_%lu_%02d.yaml", timestamp, filename_postfix_count);
        }
        filename_exists = (access(meta_file_path, F_OK) != -1);
        filename_postfix_count++;
    } while (filename_exists);

    fd_src = open(META_FILE_PATH, O_RDONLY);
    if (fd_src < 0) {
        return -1;
    }

    fd_dst = open(meta_file_path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_dst < 0) {
        int e = errno;
        close(fd_src);
        errno = e;
        return -1;
    }

    char last_char = '\0';
    nread = read(fd_src, buf, sizeof(buf));
    while (nread > 0) {
        char *out_ptr = buf;

        int nwritten = write(fd_dst, out_ptr, nread);
        if (nwritten >= 0) {
            last_char = buf[nread - 1];
            nread -= nwritten;
            out_ptr += nwritten;
        } else if (errno != EINTR) {
            int e = errno;
            close(fd_src);
            close(fd_dst);
            errno = e;
            return -1;
        }

        if (nread == 0) {
            nread = read(fd_src, buf, sizeof(buf));
        }
    }

    close(fd_src);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdate-time"
    // Append a firmware line
    char firmware_line[127];
    snprintf(firmware_line, sizeof(firmware_line),
             "firmware_version: \"%s\"\nfirmware_build_date: \"%s %s\"\n",
             CETI_VERSION, __DATE__, __TIME__);
#pragma GCC diagnostic pop

    if (last_char != '\n') {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "\n%s", firmware_line);
        strncpy(firmware_line, tmp, sizeof(firmware_line));
        firmware_line[sizeof(firmware_line) - 1] = '\0'; // safety null-termination
    }
    if (write(fd_dst, firmware_line, strlen(firmware_line)) < 0) {
        int e = errno;
        close(fd_dst);
        errno = e;
        return -1;
    }

    close(fd_dst);
    return 0;
}

/**
 * @brief tag info and config logging thread
 *
 * @param paramPtr
 * @return void*
 */
void *meta_log_thread(void *paramPtr) {
    uint64_t start_clock_ms = get_monotonic_time_ms();
    uint64_t now_ms = start_clock_ms;
    while (now_ms - start_clock_ms < (1000 * FILE_LOG_TIMEOUT_S)) {
        if (g_stopAcquisition) {
            /* exit early */
            return NULL;
        }

        now_ms = get_monotonic_time_ms();
        usleep(100000); // this doesn't need to be so precise
    }

    /* timer complete: log meta file*/
    uint64_t timestamp_us = get_global_time_us();
    config_log(timestamp_us);
    meta_log(timestamp_us);

    return NULL;
}
