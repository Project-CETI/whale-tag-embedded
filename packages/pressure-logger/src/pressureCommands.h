//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef COMMANDS_H
#define COMMANDS_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "launcher.h" // for specification of enabled sensors, init_tag(), g_exit, sampling rate, data filepath, and CPU affinity, etc.
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
#include <pthread.h> // to set CPU affinity

//-----------------------------------------------------------------------------
// File locations and polling configuration
//-----------------------------------------------------------------------------
#define CMD_PIPE_PATH "../ipc/cetiCommand" // fifo locations
#define RSP_PIPE_PATH "../ipc/cetiResponse"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern FILE *g_cmd_pipe;
extern FILE *g_rsp_pipe;
extern char g_command[256];
extern int g_command_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_commands();
int handle_command(void);
void* command_thread(void* paramPtr);

#endif // COMMANDS_H