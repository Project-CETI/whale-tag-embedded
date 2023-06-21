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
static FILE* data_file = NULL;
static char data_file_notes[256] = "";
//-----------------------------------------------------------------------------
//
int main (void)
{
    pthread_t  cmdHdlThreadId = 0;
    pthread_t  sensorThreadId = 0;
    printf("CETI Dual Pressure Logger ");
    printf(VERSION);
    printf("\n");
    printf("main(): Starting Pressure Logging Application\n");
    printf("main(): Creating Command/Response Thread\n");
    pthread_create(&cmdHdlThreadId,NULL,&cmdHdlThread,NULL); //TODO add error check
    
    // Before creating the sensor thread, check if the CSV file already exists
    // If it does not, create it and write the headers
    int data_file_exists = (access(SNS_FILE, F_OK) != -1);
    
    // Open the file
    data_file = fopen(SNS_FILE, "at");
    if (data_file == NULL)
    {
        printf("Failed to open/create an output data file: %s", SNS_FILE);
        return 1;
    }
    // Write headers if the file didn't already exist
    if (!data_file_exists)
    {
        // Concatenate header columns
        char header[400] = "";
        strcat(header,  "Timestamp [ms]");
        strcat(header, ",Pressure (Sensor 0) [bar]");
        strcat(header, ",Pressure (Sensor 1) [bar]");
        strcat(header, ",Temperature (Sensor 0) [C]");
        strcat(header, ",Temperature (Sensor 1) [C]");
        strcat(header, ",Notes\n");
        // Write header to file
        fprintf(data_file, "%s", header);
        printf("Created a new output data file");
    }
    else
        printf("Using the existing output data file: %s", SNS_FILE);
        
    // Add notes for the first timestep to indicate that logging was restarted
    strcat(data_file_notes, "Restarted! | ");
    
    // Close the file
    fclose(data_file);
    
    printf("main(): Creating Sensor And Control Thread\n");
    pthread_create(&sensorThreadId,NULL,&sensorThread,NULL);
    if (gpioInitialise() < 0)
    {
        fprintf(stderr, "main(): pigpio initialization failed\n");
        return 1;
    }
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
            fgets(g_command,256,g_cmd); //get the command
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
    double pressureSensorData0[2];
    double pressureSensorData1[2];
    long long counter = 0;

    while (!g_exit) {
      data_file = fopen(SNS_FILE, "at");
      if(data_file == NULL)
      {
        printf("sensorThread(): failed to open data output file: %s", SNS_FILE);
        // Sleep a bit before retrying
        for(int i = 0; i < 10 && !g_exit; i++)
          usleep(100000);
      }
      else
      {

        if (!(getTempPsns_0(pressureSensorData0) || getTempPsns_1(pressureSensorData1)))
        {
        	
            counter += 100;
            // Write timing information
            fprintf(data_file, "%lld", counter);
            // Write the sensor data
            fprintf(data_file, ",%.3f", pressureSensorData0[1]);
            fprintf(data_file, ",%.3f", pressureSensorData1[1]);
            fprintf(data_file, ",%.3f", pressureSensorData0[0]);
            fprintf(data_file, ",%.3f", pressureSensorData1[0]);
            // Write any notes, then clear them so they are only written once
            fprintf(data_file, ",%s", data_file_notes);
            strcpy(data_file_notes, "");
            // Finish the row of data and close the file.
            fprintf(data_file, "\n");
            fclose(data_file);
        }
        
        // Delay to implement a desired sampling rate.
        // Take into account the time it took to acquire/save data.

        usleep(SAMPLING_RATE_US);
      }
    }
    printf("sensorThread(): Done!");
    return NULL;
}
