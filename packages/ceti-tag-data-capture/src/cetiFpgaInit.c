#include <pigpio.h>

#include "fpga.h"

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

    return init_fpga(strncat(g_process_path, FPGA_BITSTREAM, sizeof(g_process_path)));
}