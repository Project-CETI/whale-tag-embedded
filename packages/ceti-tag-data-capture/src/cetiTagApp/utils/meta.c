#include "meta.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define META_FILE_PATH "/opt/ceti-tag-data-capture/config/tag-info.yaml"

int meta_log(uint64_t timestamp) {
    int fd_dst, fd_src;
    char buf[4096];
    int nread;
    char meta_file_path[256];

    snprintf(meta_file_path, 255, "/data/data_tag_info_%lu.yaml", timestamp);

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

    nread = read(fd_src, buf, sizeof(buf));
    while (nread > 0) {
        char *out_ptr = buf;

        int nwritten = write(fd_dst, out_ptr, nread);
        if (nwritten >= 0) {
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
    close(fd_dst);
    return 0;
}
