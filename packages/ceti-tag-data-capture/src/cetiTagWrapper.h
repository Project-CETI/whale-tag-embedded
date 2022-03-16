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
// File: cetiTagWrapper.h
// Description: The Ceti Tag Application Top Level Header
//-----------------------------------------------------------------------------

#ifndef CETI_WRAP_H
#define CETI_WRAP_H

#include <stdio.h>

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

extern void *cmdHdlThread(void *paramPtr);
extern void *spiThread(void *paramPtr);
extern void *writeDataThread(void *paramPtr);
extern void isr_get_fifo_block(void);  // ISR for SPI

extern int hdlCmd(void);
extern int loadFpgaBitstream(void);
extern void cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
                unsigned int pld0, unsigned int pld1, char *pResponse);
extern int start_acq(void);
extern int stop_acq(void);
extern int setup_192kHz(void);
extern int setup_96kHz(void);
extern int setup_48kHz(void);
extern int setup_default(void);
extern int setup_sim_rate(char rate);
extern int reset_fifo(void);

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

#define CETI_VERSION "2.1.0"
#define CMD "../ipc/cetiCommand"  // fifo locations
#define RSP "../ipc/cetiResponse"

#define ACQ_FILE \
  "../data/test_acq_raw.dat"  // for saving short test acquisitions

//-----------------------------------------------------------------------------
// Wiring Pi For FPGA configuration

#define DONE (27)                           // GPIO 27
#define INIT_B (25)                         // GPIO 25
#define PROG_B (26)                         // GPIO 26
#define CLOCK (21)                          // GPIO 21
#define DATA (20)                           // GPIO 20
#define FPGA_BITSTREAM "../config/top.bin"  // fpga bitstream
#define BITSTREAM_SIZE_BYTES \
  (243048 * 2)  // See Xilinx Configuration User Guide UG332

//-----------------------------------------------------------------------------
// Wiring Pi For CAM

#define RESET (5)  // GPIO 5
#define DIN (19)   // GPIO 19  FPGA -> HOST
#define DOUT (18)  // GPIO 18  HOST-> FPGA
#define SCK (16)   // Moved from GPIO 1 to GPIO 16 to free I2C0
#define NUM_BYTES_MESSAGE 8

//-----------------------------------------------------------------------------
// Misc: Test point GPIO

#define TEST_POINT (17)

//-----------------------------------------------------------------------------
// Data Acq SPI Settings and Audio Data Buffering

#define SPI_CE (0)
#define SPI_CLK_RATE (15000000)  // Hz

// SPI Block Size
//  - The Pi default SPI buffer setting must be increased from 4096 to 32768
//  bytes before using this
//     * Append "spidev.bufsiz=32768" to the file /boot/cmdline.txt .  Setting
//     will take effect next boot
//     * To see buffer size setting run $ cat
//     /sys/module/spidev/parameters/bufsiz
//     * This Will be documented in SD card setup procedure
//  - The optimum block size is a trade off between leaving enough room in case
//  the interrupt execution is delayed
//    and the frequency of the interrupts.

#define HWM \
  (256)  // High Water Mark from Verilog code - these are 32 byte chunks (2
         // sample sets)
#define SPI_BLOCK_SIZE \
  (HWM * 32)  // make SPI block size <= HWM * 32 otherwise may underflow

// 	NUM_SPI_BLOCKS is the number of blocks acquired before writing out to
// mass storage 	A setting of 500 will give buffer about 10 seconds at a
// time if
// sample rate is 48 kHz 	Example sizing of buffer (DesignMaps.xlsx has a
// calcuator for this)
// 		* SPI block HWM * 32 bytes = 16384 bytes (half the hardware FIFO
// in this example)
// 		* Sampling rate 48000 Hz
// 		* 16 bytes per sample set (4 channels, 24 bits + 8 bit header
// per channel)
// 		* 1 SPI Block is then 16384/16 = 1024 sample sets or 21.33 ms
// worth of data in time
// 		* 10 seconds is 10/.02133  = 469 SPI Blocks

#define NUM_SPI_BLOCKS \
  (256)  // for example, 500 is just over 10 seconds at 48 kSPS
#define RAM_SIZE (NUM_SPI_BLOCKS * SPI_BLOCK_SIZE)  // bytes

//-----------------------------------------------------------------------------
// Dacq Flow Control Handshaking Interrupt
// The flow control signal is GPIO 22, (WiringPi 3). The FPGA sets this when
// FIFO is at HWM and clears it at LWM as viewed from the write count port. The
// FIFO margins may need to be tuned depending on latencies and how this program
// is constructed. The margins are set in the FPGA Verilog code, not here.

#define DATA_AVAIL (22)

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

extern FILE *g_cmd;
extern FILE *g_rsp;

extern char g_command[256];
extern int g_exit;
extern int g_cmdPend;

#endif