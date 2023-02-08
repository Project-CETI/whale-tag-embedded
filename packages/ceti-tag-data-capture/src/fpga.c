//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "fpga.h"

//-----------------------------------------------------------------------------
int init_fpga(void) {

  // Power present flag used by FPGA during shutdown
  gpioSetMode(POWER_FLAG, PI_OUTPUT);
  gpioWrite(POWER_FLAG, 1);

  // Configure the FPGA and log version
  char fpgaCamResponse[16];
  if (!loadFpgaBitstream()) {
      cam(0x10, 0, 0, 0, 0, fpgaCamResponse);
      CETI_LOG("init_fpga(): Successfully configured the FPGA, Ver: 0x%02X%02X ",
               fpgaCamResponse[4], fpgaCamResponse[5]);
  } else {
      CETI_LOG("init_fpga(): XXXX FPGA initial configuration failed XXXX");
      return (-1);
  }
  return 0;
}

//-----------------------------------------------------------------------------
int loadFpgaBitstream(void) {

    FILE *pConfigFile; // pointer to configuration file
    char *pConfig;     // pointer to configuration buffer

    char data_byte;
    int i, j;

    gpioSetMode(CLOCK, PI_OUTPUT);
    gpioSetMode(DATA, PI_OUTPUT);
    gpioSetMode(PROG_B, PI_OUTPUT);
    gpioSetMode(DONE, PI_INPUT);

    // initialize the GPIO lines
    gpioWrite(CLOCK, 0);
    gpioWrite(DATA, 0);
    gpioWrite(PROG_B, 0);

    // allocate the buffer
    pConfig = malloc(BITSTREAM_SIZE_BYTES); // allocate memory for the
                                            // configuration bitstream
    if (pConfig == NULL) {
        CETI_LOG("loadFpgaBitstream(): Failed to allocate memory for the configuration file");
        return 1;
    }

    // read the FPGA configuration file
    // ToDo: replace with mmap, do not hardcode bitstreamsize
    pConfigFile = fopen(FPGA_BITSTREAM, "rb");
    if (pConfigFile == NULL) {
        CETI_LOG("loadFpgaBitstream():cannot open input file");
        return 1;
    }
    fread(pConfig, 1, BITSTREAM_SIZE_BYTES, pConfigFile);
    fclose(pConfigFile);

    // Relase PROG_B
    usleep(500);
    gpioWrite(PROG_B, 1);

    // Clock out the bitstream byte by byte MSB first

    for (j = 0; j < BITSTREAM_SIZE_BYTES; j++) {

        data_byte = pConfig[j];

        for (i = 0; i < 8; i++) {
            gpioWrite(DATA, !!(data_byte & 0x80)); // double up to add some time
            gpioWrite(DATA, !!(data_byte & 0x80)); // for setup

            gpioWrite(CLOCK, 1);
            gpioWrite(CLOCK, 1);

            gpioWrite(CLOCK, 0);

            data_byte = data_byte << 1;
        }
    }

    return (!gpioRead(DONE));
}

//-----------------------------------------------------------------------------
// Control and monitor interface between Pi and the hardware
//  - Uses the FPGA to implement flexible bridging to peripherals
//  - Host (the Pi) sends and opcode, arguments and payload (see design doc for
//  opcode defintions)
//  - FPGA receives, executes the opcode and returns a pointer to the response
//  string
//-----------------------------------------------------------------------------
void cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
         unsigned int pld0, unsigned int pld1, char *pResponse) {
    int i, j;
    char data_byte = 0x00;
    char send_packet[NUM_BYTES_MESSAGE];
    char recv_packet[NUM_BYTES_MESSAGE];

    gpioSetMode(RESET, PI_OUTPUT);
    gpioSetMode(DOUT, PI_OUTPUT);
    gpioSetMode(DIN, PI_INPUT);
    gpioSetMode(SCK, PI_OUTPUT);

    // Initialize the GPIO lines
    gpioWrite(RESET, 1);
    gpioWrite(SCK, 0);
    gpioWrite(DOUT, 0);

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
            gpioWrite(DOUT, !!(data_byte & 0x80)); // setup data
            usleep(100);
            gpioWrite(SCK, 1); // pulse the clock
            usleep(100);
            gpioWrite(SCK, 0);
            usleep(100);
            data_byte = data_byte << 1;
        }
    }

    gpioWrite(DOUT, 0);
    usleep(2);

    usleep(2000);   //need to let i2c finish

    // Receive the response packet FPGA -> Pi
    for (j = 0; j < NUM_BYTES_MESSAGE; j++) {

        data_byte = 0;

        for (i = 0; i < 8; i++) {
            gpioWrite(SCK, 1); // pulse the clock
            usleep(100);
            data_byte = (gpioRead(DIN) << (7 - i) | data_byte);
            gpioWrite(SCK, 0);
            usleep(100);
        }

        recv_packet[j] = data_byte;
    }
    for (j = 0; j < NUM_BYTES_MESSAGE; j++) {
        *(pResponse + j) = recv_packet[j];
    }
}

