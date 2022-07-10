//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version:  Refer to cetiTagWrapper.h
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagWrapper.c
// Description: The Ceti Tag Application Top Level Framework
//  - main() Starts the log, runs inits and launches the command monitor thread
//  - cmdHdlThread() monitors the command FIFO and signals main when a command
//  is received
//-----------------------------------------------------------------------------
#include <pigpio.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "cetiTagLogging.h"
#include "cetiTagWrapper.h"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

FILE *g_cmd = NULL;
FILE *g_rsp = NULL;

char g_command[256];
int g_exit = 0;
int g_cmdPend = 0;

//-----------------------------------------------------------------------------
//

void sig_handler(int signum) {
   g_exit = 1;
}

int main(void) {
    pthread_t cmdHdlThreadId = 0;
    pthread_t spiThreadId = 0;
    pthread_t writeDataThreadId = 0;
    pthread_t sensorThreadId = 0;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    CETI_initializeLog();

    if (gpioInitialise() < 0) {
        CETI_LOG("main(): pigpio initialisation failed");
        return 1;
    }

    if (initTag() < 0) {
        CETI_LOG("main(): Tag initialisation failed");
        return 1;
    }
    CETI_LOG("main(): Creating command/response thread");
    pthread_create(&cmdHdlThreadId, NULL, &cmdHdlThread, NULL);
    CETI_LOG("main(): Creating SPI audio acquisition thread");
    pthread_create(&spiThreadId, NULL, &spiThread, NULL);
    CETI_LOG("main(): Creating audio save to disk thread");
    pthread_create(&writeDataThreadId, NULL, &writeDataThread, NULL);
    CETI_LOG("main(): Creating sensor thread");
    pthread_create(&sensorThreadId,NULL,&sensorThread,NULL);

    CETI_LOG("main(): Application Started");

    // Main Loop
    while (!g_exit) { // main loop runs the hearbeat and handles commands
        usleep(100000);
        if (g_cmdPend) {
            hdlCmd();
            g_cmdPend = 0;
        }
    }

    printf("Canceling Threads\n");
    CETI_LOG("Canceling Threads");
    pthread_cancel(cmdHdlThreadId);
    pthread_cancel(spiThreadId);
    pthread_cancel(writeDataThreadId);
    pthread_cancel(sensorThreadId);
    gpioTerminate();
    printf("Program Exit\n");
    CETI_LOG("Program Exit");
    return (0);
}

//-----------------------------------------------------------------------------
void *cmdHdlThread(void *paramPtr) {
    // thread that monitors the cetiCommand pipe and sets a flag if
    // a command is entered. When the FIFO is opened for reading, the external
    // client process needs to open it for writing until this thread
    // advances - that is to say fopen read is blocked waiting to be opened for
    // a write. This is in the background...the main program is still running.

    while (!g_exit) {
        if (!g_cmdPend) { // open the comand pipe for reading and wait for a
                          // command
            g_cmd = fopen(CMD, "r");      // blocked here waiting
            fgets(g_command, 256, g_cmd); // get the command
            fclose(g_cmd);                // close the pipe
            g_cmdPend = 1;
        } else {
            usleep(100);
        }
    }
    return NULL;
}
