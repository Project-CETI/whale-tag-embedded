//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

// Private local libraries
#include "fpga.h"
#include "gpio.h"

// Private system libraries
#include <pigpio.h>
#include <stdio.h>  // for FILE
#include <stdlib.h> // for malloc()
#include <string.h> // for memcpy()
#include <unistd.h> // for usleep()

#define NUM_BYTES_MESSAGE 8
#define FPGA_BITSTREAM_SIZE_BYTES (243048 * 2)  // See Xilinx Configuration User Guide UG332
#define FPGA_BITSTREAM_CONFIG_FILE_SIZE_BYTES (149516)

/*****************************************************************************
 * Device Code
*/
WTResult wt_fpga_init(const char *fpga_bitstream_path){
    // Power present flag used by FPGA during shutdown
    gpioSetMode(FPGA_POWER_FLAG, PI_OUTPUT);
    gpioWrite(FPGA_POWER_FLAG, 1);
    return wt_fpga_load_bitstream(fpga_bitstream_path);
}

void wt_fpga_reset(void) {
    gpioSetMode(FPGA_CAM_RESET, PI_OUTPUT);
    gpioWrite(FPGA_CAM_RESET, 0);
    usleep(1000);
    gpioWrite(FPGA_CAM_RESET, 1);
}

uint16_t wt_fpga_get_version(void) {
    char fpgaCamResponse[16];
    wt_fpga_cam(0x10, 0, 0, 0, 0, fpgaCamResponse);
    return (fpgaCamResponse[4] << 8) | fpgaCamResponse[5];
}

WTResult wt_fpga_load_bitstream(const char *fpga_bitstream_path) {
    FILE *pConfigFile; // pointer to configuration file
    char *pConfig;     // pointer to configuration buffer

    char data_byte;
    int i, j;

    gpioSetMode(FPGA_CLOCK, PI_OUTPUT);
    gpioSetMode(FPGA_DATA, PI_OUTPUT);
    gpioSetMode(FPGA_PROG_B, PI_OUTPUT);
    gpioSetMode(FPGA_DONE, PI_INPUT);

    // initialize the GPIO lines
    gpioWrite(FPGA_CLOCK, 0);
    gpioWrite(FPGA_DATA, 0);
    gpioWrite(FPGA_PROG_B, 0);

    // allocate the buffer
    pConfig = malloc(FPGA_BITSTREAM_SIZE_BYTES); // allocate memory for the configuration bitstream
    if (pConfig == NULL) {
        return WT_RESULT(WT_DEV_FPGA, WT_ERR_MALLOC);
    }

    // read the FPGA configuration file
    // ToDo: replace with mmap, do not hardcode bitstreamsize
    pConfigFile = fopen(fpga_bitstream_path, "rb");
    if (pConfigFile == NULL) {
        return WT_RESULT(WT_DEV_FPGA, WT_ERR_FILE_OPEN);
    }

    size_t num_read = fread(pConfig, 1, FPGA_BITSTREAM_SIZE_BYTES, pConfigFile);
    fclose(pConfigFile);
    if (num_read < FPGA_BITSTREAM_CONFIG_FILE_SIZE_BYTES) {
        return WT_RESULT(WT_DEV_FPGA, WT_ERR_FILE_READ);
    }


    // Relase FPGA_PROG_B
    usleep(500);
    gpioWrite(FPGA_PROG_B, 1);

    // Clock out the bitstream byte by byte MSB first
    for (j = 0; j < FPGA_BITSTREAM_SIZE_BYTES; j++) {
        data_byte = pConfig[j];

        for (i = 0; i < 8; i++) {
            gpioWrite(FPGA_DATA, !!(data_byte & 0x80)); // double up to add some time
            gpioWrite(FPGA_DATA, !!(data_byte & 0x80)); // for setup

            gpioWrite(FPGA_CLOCK, 1);
            gpioWrite(FPGA_CLOCK, 1);

            gpioWrite(FPGA_CLOCK, 0);

            data_byte = data_byte << 1;
        }
    }

    //free configuration bitstream memory
    free(pConfig);

    return gpioRead(FPGA_DONE) ? WT_OK : WT_RESULT(WT_DEV_FPGA, WT_ERR_FPGA_N_DONE);
}

//-----------------------------------------------------------------------------
// Control and monitor interface between Pi and the hardware
//  - Uses the FPGA to implement flexible bridging to peripherals
//  - Host (the Pi) sends and opcode, arguments and payload (see design doc for
//  opcode defintions)
//  - FPGA receives, executes the opcode and returns a pointer to the response
//  string
//-----------------------------------------------------------------------------
void wt_fpga_cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
         unsigned int pld0, unsigned int pld1, char *pResponse) {
  int i, j;
  char data_byte = 0x00;
  char send_packet[NUM_BYTES_MESSAGE];
  char recv_packet[NUM_BYTES_MESSAGE];

  gpioSetMode(FPGA_CAM_RESET, PI_OUTPUT);
  gpioSetMode(FPGA_CAM_DOUT, PI_OUTPUT);
  gpioSetMode(FPGA_CAM_DIN, PI_INPUT);
  gpioSetMode(FPGA_CAM_SCK, PI_OUTPUT);

  // Initialize the GPIO lines
  gpioWrite(FPGA_CAM_RESET, 1);
  gpioWrite(FPGA_CAM_SCK, 0);
  gpioWrite(FPGA_CAM_DOUT, 0);

  // Send a CAM packet Pi -> FPGA
  send_packet[0] = 0x02;         // STX
  send_packet[1] = (char)opcode; // Opcode
  send_packet[2] = (char)arg0;   // Arg0
  send_packet[3] = (char)arg1;   // Arg1
  send_packet[4] = (char)pld0;   // Payload0
  send_packet[5] = (char)pld1;   // Payload1
  send_packet[6] = 0x00;         // Checksum
  send_packet[7] = 0x03;         // ETX

  for (j = 0; j < NUM_BYTES_MESSAGE; j++) {
    data_byte = send_packet[j];

    for (i = 0; i < 8; i++) {
      gpioWrite(FPGA_CAM_DOUT, !!(data_byte & 0x80)); // setup data
      usleep(100);
      gpioWrite(FPGA_CAM_SCK, 1); // pulse the clock
      usleep(100);
      gpioWrite(FPGA_CAM_SCK, 0);
      usleep(100);
      data_byte = data_byte << 1;
    }
  }
  gpioWrite(FPGA_CAM_DOUT, 0);
  usleep(2);

  usleep(2000); // need to let i2c finish

  // Receive the response packet FPGA -> Pi
  for (j = 0; j < NUM_BYTES_MESSAGE; j++) {

    data_byte = 0;

    for (i = 0; i < 8; i++) {
      gpioWrite(FPGA_CAM_SCK, 1); // pulse the clock
      usleep(100);
      data_byte = (gpioRead(FPGA_CAM_DIN) << (7 - i) | data_byte);
      gpioWrite(FPGA_CAM_SCK, 0);
      usleep(100);
    }

    recv_packet[j] = data_byte;
  }
  if(pResponse != NULL){
    memcpy(pResponse, recv_packet, NUM_BYTES_MESSAGE);
  }
}
