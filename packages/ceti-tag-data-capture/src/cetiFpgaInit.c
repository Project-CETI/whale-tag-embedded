#include <pigpio.h>
#include <string.h>

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
    char *filepath = strncat(g_process_path, FPGA_BITSTREAM, sizeof(g_process_path));

    //Initialize fpga
    ResultFPGA init_result = init_fpga(filepath);
    switch (init_result){
        case FPGA_OK:
            CETI_LOG("FPGA successfully initialized!!!");
            return 0;
        
        case FPGA_ERR_CONFIG_MALLOC:
            CETI_ERR("Failed to malloc fpga bitstream.");
            return -1;
            
        case FPGA_ERR_BIN_OPEN:
            CETI_ERR("Failed to open bitstream %s", g_process_path);
            return -2;

        case FPGA_ERR_N_DONE:
            CETI_ERR("Did not receive done signal from FPGA.");
            return -3;

        default:
            CETI_ERR("Unknown FPGA error.");
            return -4;
        
    }
}