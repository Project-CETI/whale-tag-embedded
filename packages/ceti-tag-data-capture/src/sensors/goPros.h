//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef GOPROS_H
#define GOPROS_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define _GNU_SOURCE   // change how sched.h will be included

#include "../utils/logging.h"
#include "../launcher.h" // for g_exit, sampling rate, data filepath, and CPU affinity
#include "../systemMonitor.h" // for the global CPU assignment variable to update

#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define NUM_GOPROS 4
#define GOPRO_COMMAND_START 1
#define GOPRO_COMMAND_STOP 2
#define GOPRO_COMMAND_HEARTBEAT 3
#define GOPRO_PYTHON_HEARTBEAT_PERIOD_US 5000000
#define GOPRO_PYTHON_PORT 2300
#define GOPRO_PYTHON_BUFFERSIZE 512

// Define a struct for sending/receiving data to Python.
#pragma pack(1)
typedef struct payload_t {
    uint32_t goPro_index;
    uint32_t command;
    uint32_t is_ack;
} goPro_python_message_struct;
#pragma pack()

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_goPros();
int start_goPro(int gopro_index);
int stop_goPro(int gopro_index);
int create_goPros_socket();
void close_goPros_socket();
int send_goPros_message(void* msg, uint32_t msgsize);
void send_command_to_all_goPros(int goPro_command);
void* goPros_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_goPros_thread_is_running;

#endif // GOPROS_H







