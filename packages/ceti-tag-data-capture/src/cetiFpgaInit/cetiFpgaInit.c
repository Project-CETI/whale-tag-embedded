//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include <pigpio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "hal.h"
#include "logging.h"

#define FPGA_BITSTREAM "../config/top.bin" // fpga bitstream

int g_exit = 0;
int g_stopAcquisition = 0;
char g_process_path[256] = "/opt/ceti-tag-data-capture/bin";

int main(void){
    if (gpioInitialise() < 0) {
        CETI_ERR("Failed to initialize pigpio");
        return -1;
    }
    atexit(gpioTerminate);

    // Get process dir path
    int bytes = readlink("/proc/self/exe", g_process_path, sizeof(g_process_path) - 1);
    while(bytes > 0) {
        if(g_process_path[bytes - 1] == '/'){
        g_process_path[bytes] = '\0';
        break;
        }
        bytes--;
    }
    char *filepath = strncat(g_process_path, FPGA_BITSTREAM, sizeof(g_process_path));

    //Initialize fpga
    WTResult init_result = wt_fpga_init(filepath);
    if(init_result != WT_OK){
        CETI_ERR("%s", wt_strerror(init_result));
    }
}
