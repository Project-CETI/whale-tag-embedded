//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
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
#define FPGA_BITSTREAM_SIZE_BYTES (243048 * 2)  // See Xilinx Configuration User Guide UG332
#define FPGA_BITSTREAM_CONFIG_FILE_SIZE_BYTES (149516)

//-----------------------------------------------------------------------------
// GPIO For CAM

#define RESET (5) // GPIO 5
#define DIN (19)  // GPIO 19  FPGA -> HOST
#define DOUT (18) // GPIO 18  HOST-> FPGA
#define SCK (16)  // Moved from GPIO 1 to GPIO 16 to free I2C0
#define NUM_BYTES_MESSAGE 8

#define FPGA_ADC_WRITE(addr, value)       cam(1, (addr), (value), 0, 0, NULL)
#define FPGA_ADC_READ(addr, value_ptr)    {\
    char fpga_response[NUM_BYTES_MESSAGE];\
    cam(1, (0x80 | (addr)), 0, 0, 0, NULL);\
    cam(1, (0x80 | (addr)), 0, 0, 0, fpga_response);\
    *(value_ptr) = (fpga_response[4] << 8) | fpga_response[5];\
}
#define FPGA_ADC_SYNC()       cam(2, 0, 0, 0, 0, NULL)
#define FPGA_FIFO_RESET()     cam(3, 0, 0, 0, 0, NULL)
#define FPGA_FIFO_START()     cam(4, 0, 0, 0, 0, NULL)
#define FPGA_FIFO_STOP()      cam(5, 0, 0, 0, 0, NULL)
#define FPGA_FIFO_BIT_DEPTH(audio_bit_depth) cam(0x11, 0, audio_bit_depth, 0, 0, NULL)

//-----------------------------------------------------------------------------

#define POWER_FLAG 17

typedef enum  {
    FPGA_OK,
    FPGA_ERR_CONFIG_MALLOC = -1,
    FPGA_ERR_BIN_OPEN = -2,
    FPGA_ERR_N_DONE = -3,
} ResultFPGA;

ResultFPGA init_fpga(const char *fpga_bitstream_path);
ResultFPGA loadFpgaBitstream(const char *fpga_bitstream_path);
void cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
         unsigned int pld0, unsigned int pld1, char *pResponse);

#endif // FPGA_H