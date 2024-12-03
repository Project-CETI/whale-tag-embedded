//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef COMMANDS_H
#define COMMANDS_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// File locations and polling configuration
//-----------------------------------------------------------------------------
#define CMD_PIPE_PATH "../ipc/cetiCommand" // fifo locations
#define RSP_PIPE_PATH "../ipc/cetiResponse"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern int g_command_thread_is_running;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int handle_command(void);
void *command_thread(void *paramPtr);

#endif // COMMANDS_H