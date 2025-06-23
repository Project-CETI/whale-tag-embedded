//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_GPIO_H__
#define __CETI_WHALE_TAG_HAL_GPIO_H__

//-----------------------------------------------------------------------------
// Dacq Flow Control Handshaking Interrupt
// The flow control signal is GPIO 22, (WiringPi 3). The FPGA sets this when
// FIFO is at HWM and clears it at LWM as viewed from the write count port. The
// FIFO margins may need to be tuned depending on latencies and how this program
// is constructed. The margins are set in the FPGA Verilog code, not here.

#define IMU_N_RESET 4
#define FPGA_CAM_RESET (5) // GPIO 5
// (6)
// (7)
// (8)
// (9)
// (10)
// (11)
// Overflow indicator.
#define AUDIO_OVERFLOW_GPIO (12) // -1 to not use the indicator
// (13)
// (14)
// (15)
#define FPGA_CAM_SCK (16) // Moved from GPIO 1 to GPIO 16 to free I2C0
#define FPGA_POWER_FLAG (17)
#define FPGA_CAM_DOUT (18) // GPIO 18  HOST-> FPGA
#define FPGA_CAM_DIN (19)  // GPIO 19  FPGA -> HOST
#define FPGA_DATA (20)     // GPIO 20
#define FPGA_CLOCK (21)    // GPIO 21
#define AUDIO_DATA_AVAILABLE (22)
#define IMU_BB_I2C_SDA (23) //
#define IMU_BB_I2C_SCL (24)
#define FPGA_INIT_B (25) // GPIO 25
#define FPGA_PROG_B (26) // GPIO 26
#define FPGA_DONE (27)   // GPIO 27

#endif // __CETI_WHALE_TAG_HAL_GPIO_H__
