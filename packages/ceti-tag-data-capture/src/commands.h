
#ifndef COMMANDS_H
#define COMMANDS_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "launcher.h" // for specification of enabled sensors, init_tag(), g_exit, etc.
#include "utils/logging.h"
#include "recovery.h"
#include "battery.h"
#include "burnwire.h"
#include "sensors/imu.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sched.h> // to get the current CPU assigned to the thread

//-----------------------------------------------------------------------------
// File locations and polling configuration
//-----------------------------------------------------------------------------
#define CMD_FILEPATH "../ipc/cetiCommand" // fifo locations
#define RSP_FILEPATH "../ipc/cetiResponse"

#define CMD_POLLING_PERIOD_US 1000 // How often to check the file for new commands

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern FILE *g_cmd_file;
extern FILE *g_rsp_file;
extern char g_command[256];
extern int g_command_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_commands();
int handle_command(void);
void* command_thread(void* paramPtr);

#endif // COMMANDS_H