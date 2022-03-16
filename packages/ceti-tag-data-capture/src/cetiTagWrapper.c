//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    Description
//  0.00    10/08/21   Begin work, establish framework
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagWrapper.c
// Description: The Ceti Tag Application Top Level Framework
//  - main() Starts the log, runs inits and launches the command monitor thread
//  - cmdHdlThread() monitors the command FIFO and signals main when a command
//  is received
//-----------------------------------------------------------------------------
#include "cetiTagWrapper.h"

#include <pigpio.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cetiTagLogging.h"

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

int main(void) {
  pthread_t cmdHdlThreadId = 0;
  pthread_t spiThreadId = 0;
  pthread_t writeDataThreadId = 0;

  printf("CETI Tag Electronics Main Application ");
  printf(CETI_VERSION);
  printf("\n");
  printf("main(): Begin Logging\n");
  CETI_initializeLog();
  printf("main(): Starting Application\n");

  if (gpioInitialise() < 0) {
    fprintf(stderr, "main(): pigpio initialisation failed\n");
    return 1;
  }

  printf("main(): Creating Command/Response Thread\n");
  pthread_create(&cmdHdlThreadId, NULL, &cmdHdlThread,
                 NULL);  // TODO add error check
  printf("main(): Creating SPI Acquisition Thread\n");
  pthread_create(&spiThreadId, NULL, &spiThread, NULL);
  printf("main(): Creating Data Acquisition Thread\n");
  pthread_create(&writeDataThreadId, NULL, &writeDataThread, NULL);

  CETI_LOG("Application Started");

  // Main Loop
  while (!g_exit) {  // main loop runs the hearbeat and handles commands
    usleep(100000);
    if (g_cmdPend) {
      hdlCmd();
      g_cmdPend = 0;
    }
  }

  printf("Canceling cmdHdlThread\n");
  pthread_cancel(cmdHdlThreadId);
  pthread_cancel(spiThreadId);
  pthread_cancel(writeDataThreadId);
  gpioTerminate();
  printf("Program Exit\n");
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
    if (!g_cmdPend) {  // open the comand pipe for reading and wait for a
                       // command
      g_cmd = fopen(CMD, "r");       // blocked here waiting
      fgets(g_command, 256, g_cmd);  // get the command
      fclose(g_cmd);                 // close the pipe
      g_cmdPend = 1;
    } else {
      usleep(100);
    }
  }
  return NULL;
}
