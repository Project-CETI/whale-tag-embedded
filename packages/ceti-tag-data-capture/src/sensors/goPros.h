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
#define NUM_GOPROS 1

// Configure the socket for communicating with the Python interface.
#define GOPRO_PYTHON_PORT 2300
#define GOPRO_PYTHON_BUFFERSIZE 512

// How often the C program will let the Python program know it's still running.
#define GOPRO_PYTHON_HEARTBEAT_PERIOD_US 5000000
#define GOPRO_PYTHON_ACK_TIMEOUT_US 30000000 // note that the Python discover/connect timeout is currently 20 seconds

// Define the commands that will be exchanged with the Python interface.
#define GOPRO_COMMAND_START 1
#define GOPRO_COMMAND_STOP  2
#define GOPRO_COMMAND_HEARTBEAT 3

// Define a struct for sending/receiving data to Python.
#pragma pack(1)
typedef struct payload_t {
    uint32_t goPro_index;
    uint32_t command;
    uint32_t is_ack;
    uint32_t success;
} goPro_python_message_struct;
#pragma pack()

// Define user interface parameters
#define GOPRO_SWITCH_GPIO    13
#define GOPRO_LED_GPIO_BLUE  10
#define GOPRO_LED_GPIO_RED    9
#define GOPRO_LED_GPIO_GREEN 11
#define GOPRO_LED_HEARTBEAT_PERIOD_US 1000000
#define GOPRO_MAGNET_DETECTION_BUFFER_US 3000000
#define GOPRO_MAGNET_DETECTION_THRESHOLD_US 2500000

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_goPros();
int start_goPro(int gopro_index);
int stop_goPro(int gopro_index);
int create_goPros_socket();
void close_goPros_socket();
int send_goPros_message(void* msg, uint32_t msgsize);
int send_command_to_all_goPros(int goPro_command);
void* goPros_thread(void* paramPtr);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_goPros_thread_is_running;

#endif // GOPROS_H







