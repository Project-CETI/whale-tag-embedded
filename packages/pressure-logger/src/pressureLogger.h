//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Dual Pressure Sensor Data Logger 
// 
//-----------------------------------------------------------------------------
// Version    Date    Description					
//  0.00    05/26/23  Begin work, establish framework for the logger
//

//-----------------------------------------------------------------------------
// File: pressureLogger.h
//
// Description: This is the header for a utility program for logging pressure data from a pair
// Keller 4LD sensors. 
//
// This program targets the Pi Zero 2 platform and has been branched from the
// v2_2_refactor build which includes many features not needed for the pressure
// logger. Those features are disabled either in the build script or otherwise
// so that this program can run on a standalone Pi Zero 2 and not require
// any sensors other than the 2 Keller devices.
//
//
// This design uses two threads.  One thread is for interacting with the program
// via named pipes using a command/response protocol, which is useful for development 
// and adds some flexibility. The second thread simply fetches readings from the Kellers
// and writes them to a .csv file which is the basic requirement for the program
//
//-----------------------------------------------------------------------------

#ifndef WRAP_H
#define WRAP_H

#define VERSION "0.0 Begin Implementation"

#include <stdio.h>

typedef struct {  //To hold Keller 4LD pressure sensor data
	char status;
	char p_msb;
	char p_lsb;
	char t_msb;
	char t_lsb;
} psns_t;

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

extern void * cmdHdlThread( void * paramPtr);
extern void * sensorThread(void * paramPtr);
extern int hdlCmd(void);
extern int getTempPsns (double * pressureSensorData);

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

#define CMD "../ipc/Command"     //fifo locations
#define RSP "../ipc/Response"
#define SNS_FILE "../data/pressure.csv"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

extern FILE *g_cmd;
extern FILE *g_rsp;

extern char g_command[256];
extern int g_exit;
extern int g_cmdPend;


#endif