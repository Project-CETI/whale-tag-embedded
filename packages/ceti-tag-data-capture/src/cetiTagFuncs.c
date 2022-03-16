//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    Description
//  0.00    10/08/21   Begin work, establish framework
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagFuncs.c
// Description: Functions used by the top wrapper
//-----------------------------------------------------------------------------
#include <pigpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cetiTagLogging.h"
#include "cetiTagWrapper.h"

//-----------------------------------------------------------------------------
// Command interpreter and handler
//-----------------------------------------------------------------------------
int hdlCmd(void) {
  int i, n;
  char cArg[256], cTemp[256];  // strings and some
  char *pTemp1, *pTemp2;       // working pointers
  //-----------------------------------------------------------------------------
  // Part 1 - quit or any other special commands here
  if (!strncmp(g_command, "quit", 4)) {
    printf("Quitting\n");
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "Stopping the App\n");  // echo it back
    fclose(g_rsp);
    CETI_LOG("Application Stopping");
    g_exit = 1;
    return 0;
  }
  //-----------------------------------------------------------------------------
  // Part 2 - Client commands
  if (!strncmp(g_command, "dbug", 4)) {
    printf("Debug Placeholder is Executing\n");
    g_rsp = fopen(RSP, "w");
    printf("Debug Placeholder is Finished\n");
    fprintf(g_rsp, "Running Debug Routine(s)\n");  // echo it back
    fclose(g_rsp);
    CETI_LOG("debug %d", 1234);
    return 0;
  }

  if (!strncmp(g_command, "configFPGA", 10)) {
    printf("hdlCmd(): Configuring the FPGA\n");

    if (!loadFpgaBitstream()) {
      CETI_LOG("hdlCmd(): FPGA Configuration Succeeded");
      g_rsp = fopen(RSP, "w");
      fprintf(g_rsp, "hdlCmd(): Configuring FPGA Succeeded\n");  // echo it back
    } else {
      CETI_LOG("FPGA Configuration Failed");
      g_rsp = fopen(RSP, "w");
      fprintf(
          g_rsp,
          "hdlCmd(): Configuring FPGA Failed - Try Again\n");  // echo it back
    }
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "checkCAM", 8)) {
    printf("hdlCmd(): Testing CAM Link\n");
    cam(0, 0, 0, 0, 0, cTemp);
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "CAM Link Up\n");                     // echo it
    printf("hdlCmd(): camcheck FE: %02X \n", cTemp[4]);  // should be FE
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "startAcq", 8)) {
    printf("hdlCmd(): Starting Acquisition\n");
    start_acq();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): Acquisition Started\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "stopAcq", 7)) {
    printf("hdlCmd(): Stopping Acquisition\n");
    stop_acq();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): Acquisition Stopped\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "resetFPGA", 7)) {
    printf("hdlCmd(): Resetting the FPGA\n");
    gpioSetMode(RESET, PI_OUTPUT);
    gpioWrite(RESET, 0);
    usleep(1000);
    gpioWrite(RESET, 1);
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): FPGA Reset Completed\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "sr_192", 6)) {
    printf("hdlCmd(): Set sampling rate to 192 kHz\n");
    setup_192kHz();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "sr_96", 5)) {
    printf("hdlCmd(): Set sampling rate to 96 kHz\n");
    setup_96kHz();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "sr_48", 5)) {
    printf("hdlCmd(): Set sampling rate to 48 kHz\n");
    setup_48kHz();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "sr_dflt", 7)) {
    printf("hdlCmd(): Set sampling rate to default (750 Hz)\n");
    setup_default();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "sr_sim", 6)) {
    printf("hdlCmd(): Set simulation sampling rate\n");

    // the user will enter an integer as part of the command
    // Must be between 5 and 256 (units of microseconds)
    // the units are microseconds.
    // e.g.  "sr_sim 100"  which yields 10 kHz sampling rate sim

    pTemp1 =
        strchr(g_command,
               ' ');  // find space char - separates the params in the script
    pTemp1++;         // this is pointing to the period argument now
    pTemp2 = strchr(g_command, 0x0A);  // and this is the end of the command
    n = pTemp2 - pTemp1;               // will be 1,2 or 3 chars long

    for (i = 0; i < n; i++)
      cArg[i] = *(pTemp1 + i);  // this is the period in microseconds as a
                                // string
    cArg[n] = '\0';             // append null term
    setup_sim_rate(atoi(cArg));
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): Setup the Simulated Sampling Rate\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  if (!strncmp(g_command, "resetFIFO", 9)) {
    printf("hdlCmd(): Resetting the FIFO\n");
    reset_fifo();
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "hdlCmd(): FIFO Reset\n");  // echo it
    fclose(g_rsp);
    return 0;
  }

  else {
    g_rsp = fopen(RSP, "w");
    fprintf(g_rsp, "\n");  // echo it
    fprintf(g_rsp, "CETI Tag Electronics Available Commands\n");
    fprintf(g_rsp,
            "---------------------------------------------------------\n");
    fprintf(g_rsp, "configFPGA  Load FPGA bitstream\n");
    fprintf(g_rsp, "resetFPGA   Reset FPGA state machines\n");
    fprintf(g_rsp, "resetFIFO   Reset audio HW FIFO\n");
    fprintf(g_rsp, "checkCAM    Verify hardware control link\n");
    fprintf(g_rsp, "startAcq    Start acquiring samples from the FIFO\n");
    fprintf(g_rsp, "stopAcq     Stop acquiring samples from  the FIFO\n");
    fprintf(g_rsp, "sr_192      Set sampling rate to 192 kHz \n");
    fprintf(g_rsp, "sr_96       Set sampling rate to 96 kHz\n");
    fprintf(g_rsp, "sr_48       Set sampling rate to 48 kHz \n");
    fprintf(g_rsp, "sr_dflt     Set sampling rate to 750 Hz (ADC default)\n");
    fprintf(g_rsp,
            "sr_sim      Set simulated data sampling rate (only for use with "
            "simulator FPGA)\n");
    fprintf(g_rsp, "\n");
    fclose(g_rsp);
    return 0;
  }
  return 1;
}

int loadFpgaBitstream(void) {
  FILE *pConfigFile;  // pointer to configuration file
  char *pConfig;      // pointer to configuration buffer

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
  pConfig = malloc(
      BITSTREAM_SIZE_BYTES);  // allocate memory for the configuration bitstream
  if (pConfig == NULL) {
    fprintf(stderr,
            "loadFpgaBitstream(): Failed to allocate memory for the "
            "configuration file\n");
    return 1;
  }

  // read the FPGA configuration file
  // ToDo: replace with mmap, do not hardcode bitstreamsize
  pConfigFile = fopen(FPGA_BITSTREAM, "rb");
  if (pConfigFile == NULL) {
    fprintf(stderr, "loadFpgaBitstream():cannot open input file\n");
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
      gpioWrite(DATA, !!(data_byte & 0x80));  // double up to add some time
      gpioWrite(DATA, !!(data_byte & 0x80));  // for setup

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
  send_packet[0] = 0x02;          // STX
  send_packet[1] = (char)opcode;  // Opcode
  send_packet[2] = (char)arg0;    // Arg0
  send_packet[3] = (char)arg1;    // Arg1
  send_packet[4] = (char)pld0;    // Payload0
  send_packet[5] = (char)pld1;    // Payload1
  send_packet[6] = 0x00;          // Checksum
  send_packet[7] = 0x03;          // ETX

  for (j = 0; j < NUM_BYTES_MESSAGE; j++) {
    data_byte = send_packet[j];

    for (i = 0; i < 8; i++) {
      gpioWrite(DOUT, !!(data_byte & 0x80));  // setup data
      usleep(100);
      gpioWrite(SCK, 1);  // pulse the clock
      usleep(100);
      gpioWrite(SCK, 0);
      usleep(100);
      data_byte = data_byte << 1;
    }
  }

  gpioWrite(DOUT, 0);
  usleep(2);

  // Receive the response packet FPGA -> Pi
  for (j = 0; j < NUM_BYTES_MESSAGE; j++) {
    data_byte = 0;

    for (i = 0; i < 8; i++) {
      gpioWrite(SCK, 1);  // pulse the clock
      usleep(100);
      data_byte = (gpioRead(DIN) << (7 - i) | data_byte);
      gpioWrite(SCK, 0);
      usleep(100);
    }

    recv_packet[j] = data_byte;
  }
  for (j = 0; j < NUM_BYTES_MESSAGE; j++) {
    // printf("received %d: %02X \n",j,recv_packet[j]);
    *(pResponse + j) = recv_packet[j];  // fill in the response
  }
}
