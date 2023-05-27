//-----------------------------------------------------------------------------
// Command interpreter and handler
//-----------------------------------------------------------------------------

#include <pigpio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pressureLogger.h"


int hdlCmd(void) {

    char cTemp[256]; 

    //-----------------------------------------------------------------------------
    // Part 1 - quit or any other special commands here
    if (!strncmp(g_command, "quit", 4)) {
        printf("Quitting\n");
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "Stopping the App\n"); // echo it back
        fclose(g_rsp);
        g_exit = 1;
        return 0;
    }
    //-----------------------------------------------------------------------------
    // Part 2 - Client commands
    if (!strncmp(g_command, "dbug", 4)) {
        printf("Debug Placeholder is Executing\n");
        g_rsp = fopen(RSP, "w");
        printf("Debug Placeholder is Finished\n");
        fprintf(g_rsp, "Running Debug Routine(s)\n"); // echo it back
        fclose(g_rsp);
        return 0;
    }  

    else {
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "\n"); // echo it
        fprintf(g_rsp, "CETI Tag Electronics Available Commands\n");
        fprintf(g_rsp,
                "---------------------------------------------------------\n");
        fprintf(g_rsp, "quit        Exit the application gracefully\n");

        fprintf(g_rsp, "debug       Dummy function for debugging\n");       

        fprintf(g_rsp, "\n");
        fclose(g_rsp);
        return 0;
    }
    return 1;
}