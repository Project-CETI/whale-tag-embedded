#include "thread_error.h"
#include <syslog.h>

static uint32_t thread_device_status = 0;
void update_thread_device_status(ThreadKind thread_id, WTResult device_status, const char *thread_name) {
    if (device_status == WT_OK) {
        thread_device_status &= ~(1 << thread_id);
    } else {
        if ((thread_device_status & (1 << thread_id)) == 0) { // warn if thread was previously ok
            char err_str[512];
            syslog(LOG_DEBUG, "[ERROR]: %s(): Device error: %s", thread_name, wt_strerror_r(device_status, err_str, sizeof(err_str)));
            thread_device_status |= (1 << thread_id);
        }
    }
}