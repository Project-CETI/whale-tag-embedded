//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Dual Pressure Sensor Data Logger 
// 
//-----------------------------------------------------------------------------
// File: pressureLogger.c
//
// C source code for the program. See pressureLogger.h for additional notes

#include <pigpio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "pressureLogger.h"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

FILE *g_cmd=NULL;
FILE *g_rsp=NULL;

char g_command[256];
int g_exit=0;
int g_cmdPend = 0;

//-----------------------------------------------------------------------------
//

int main (void)
{
	pthread_t  cmdHdlThreadId = 0;
	pthread_t  sensorThreadId = 0;

	printf("CETI Dual Pressure Logger ");
	printf(VERSION);
	printf("\n");
	printf("main(): Begin Logging\n");
//	CETI_initializeLog(LOGFILE);
	printf("main(): Starting Pressure Logging Application\n");

	printf("main(): Creating Command/Response Thread\n");
	pthread_create(&cmdHdlThreadId,NULL,&cmdHdlThread,NULL); //TODO add error check

	printf("main(): Creating Sensor And Control Thread\n");
	pthread_create(&sensorThreadId,NULL,&sensorThread,NULL);

// Interactive command monitor and handling

	while(!g_exit) {  
		usleep(100000);
		if(g_cmdPend){
			hdlCmd();
			g_cmdPend = 0;
		}
	}

	printf("Canceling Threads\n");
	pthread_cancel(cmdHdlThreadId);
	pthread_cancel(sensorThreadId);

	printf("Program Exit\n");
	return(0);
}

//-----------------------------------------------------------------------------
void * cmdHdlThread(void * paramPtr)
{
	// thread that monitors the command pipe and sets a flag if
	// a command is entered. When the FIFO is opened for reading, the external
	// client process needs to open it for writing until this thread
	// advances - that is to say fopen read is blocked waiting to be opened for
	// a write. This is in the background...the main program is still running.

	while(!g_exit){
		if(!g_cmdPend){ //open the comand pipe for reading and wait for a command
			g_cmd = fopen(CMD,"r"); //blocked here waiting
			fgets(g_command,256,g_cmd);	//get the command
			fclose(g_cmd); //close the pipe
			g_cmdPend = 1;
		} else {
			usleep(100);
		}
	}
	return NULL;
}

void * sensorThread(void * paramPtr)
{


// The actual code to read the sensors and save to file will go here
// ...
// code here!
//

	return NULL;

}