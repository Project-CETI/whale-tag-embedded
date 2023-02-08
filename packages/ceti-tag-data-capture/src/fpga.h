//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef FPGA_H
#define FPGA_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "utils/logging.h"

//-----------------------------------------------------------------------------
// Definitions/Configurations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// GPIO For FPGA configuration

#define DONE (27)                          // GPIO 27
#define INIT_B (25)                        // GPIO 25
#define PROG_B (26)                        // GPIO 26
#define CLOCK (21)                         // GPIO 21
#define DATA (20)                          // GPIO 20
#define FPGA_BITSTREAM "../config/top.bin" // fpga bitstream
#define BITSTREAM_SIZE_BYTES                                                   \
    (243048 * 2) // See Xilinx Configuration User Guide UG332

//-----------------------------------------------------------------------------
// GPIO For CAM

#define RESET (5) // GPIO 5
#define DIN (19)  // GPIO 19  FPGA -> HOST
#define DOUT (18) // GPIO 18  HOST-> FPGA
#define SCK (16)  // Moved from GPIO 1 to GPIO 16 to free I2C0
#define NUM_BYTES_MESSAGE 8

//-----------------------------------------------------------------------------

#define POWER_FLAG 17

#include "utils/logging.h"
#include <pigpio.h>
#include <stdio.h> // for FILE
#include <stdlib.h> // for malloc()
#include <unistd.h> // for usleep()

int init_fpga(void);
int loadFpgaBitstream(void);
void cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
         unsigned int pld0, unsigned int pld1, char *pResponse);

#endif // FPGA_H