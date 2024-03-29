//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
//
// Note - version must correspond with the repository release tag
//
// Version    Date    Description
//  2_1.0   10/08/21   Begin work, establish framework
//  2_1.1   06/27/22   Update v2.0 to work with v2.1 hardware
//  2_1.2   07/03/22   Fix deploy timeout bug
//  2_1.3   08/17/22   Bit-banged i2c for IMU, serial comms for Recovery Board
//  2_1.4   11/3/22    Add battery check funcs, general clean up. FPGA shutdown method
//  2_1.5   11/24/22   Change RAM buffer size to accomodate read-only rootfs
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagWrapper.h
// Description: The Ceti Tag application top level header file
//-----------------------------------------------------------------------------

#ifndef CETI_WRAP_H
#define CETI_WRAP_H

#define CETI_VERSION "v2_1.5 release candidate - in test"

#include <stdio.h>

typedef enum {       // Tag operational states for deployment sequencing
    ST_CONFIG = 0,   // get the deployment parameters from config file
    ST_START,        // turn on the audio recorder, illuminate ready LED
    ST_DEPLOY,       // wait for whale to dive
    ST_REC_SUB,      // recording while underwater
    ST_REC_SURF,     // recording while surfaced - trying for a GPS fix
    ST_BRN_ON,       // burnwire is on, may or may not be at the surface when in
                     // this state
    ST_RETRIEVE,     // burnwire timed out, likely at surface, monitor GPS and
                     // transmit coord if enough battery
    ST_SHUTDOWN,     // battery critical, put system in minimum power mode
    ST_UNKNOWN
} wt_state_t;

#define MAX_STATE_STRING_LEN (32)
static const char state_str[][MAX_STATE_STRING_LEN] = {
    "CONFIG",
    "START",
    "DEPLOY",
    "REC_SUB",
    "REC_SURF",
    "BRN_ON",
    "RETRIEVE",
    "SHUTDOWN",
    "ST_UNKNOWN"
};

typedef struct { // To hold rotation vector input report information
    char reportID;
    char sequenceNum;
    char status;
    char delay;
    signed short quat_i;
    signed short quat_j;
    signed short quat_k;
    signed short quat_real;
    signed short accEstimate;
} rotation_t;

typedef struct { // To hold Keller 4LD pressure sensor data
    char status;
    char p_msb;
    char p_lsb;
    char t_msb;
    char t_lsb;
} psns_t;

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

extern void *cmdHdlThread(void *paramPtr);
extern void *spiThread(void *paramPtr);
extern void *writeDataThread(void *paramPtr);
extern void *sensorThread(void *paramPtr);
extern void isr_get_fifo_block(void);

extern int hdlCmd(void);
extern int initTag(void);
extern int loadFpgaBitstream(void);

extern void cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
                unsigned int pld0, unsigned int pld1, char *pResponse);

extern int start_acq(void);
extern int stop_acq(void);
extern int setup_192kHz(void);
extern int setup_96kHz(void);
extern int setup_48kHz(void);
extern int setup_default(void);
extern int reset_fifo(void);

extern int initI2cDevices(void);

extern int resetIMU(void);
extern int setupIMU(void);
extern int learnIMU(void);
extern int getRotation(rotation_t *rotation);

extern int rcvryOn(void);
extern int rcvryOff(void);
extern int burnwireOn(void);
extern int burnwireOff(void);

extern int getRtcCount(unsigned int *pRtcCount);
extern int resetRtcCount(void);
extern unsigned int getTimeDeploy(void);

extern int getBoardTemp(int *boardTemp);
extern int getTempPsns(double *pressureSensorData);
extern int getBattStatus(double *batteryData);
extern int getQuaternion(short *quaternion);
extern int getAmbientLight(int *ambientLight);
extern int getGpsLocation(char *gpsLocation);
extern int updateState();
extern int burnwireOn(void);
extern int burnwireOff(void);

extern int testRecoverySerial(void);


//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

#define CETI_CONFIG_FILE "../config/ceti-config.txt"

#define CMD "../ipc/cetiCommand" // fifo locations
#define RSP "../ipc/cetiResponse"

#define SNS_FILE "../data/sensors.csv"
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
// Other GPIO Assignments


#define IMU_N_RESET 4
#define POWER_FLAG 17

//-----------------------------------------------------------------------------
// Data Acq SPI Settings and Audio Data Buffering

#define SPI_CE (0)
#define SPI_CLK_RATE (15000000) // Hz

// SPI Block Size

#define HWM                                                                    \
    (256) // High Water Mark from Verilog code - these are 32 byte chunks (2
          // sample sets)
#define SPI_BLOCK_SIZE                                                         \
    (HWM * 32) // make SPI block size <= HWM * 32 otherwise may underflow

// NUM_SPI_BLOCKS is the number of blocks acquired before writing out to
// mass storage. A setting of 2100 will give buffer about 30 seconds at a time if
// sample rate is 96 kHz. Example sizing of buffer (DesignMaps.xlsx has a
// calcuator for this):
//
//  - SPI block HWM * 32 bytes = 8192 bytes (25% of the hardware FIFO in this example)
//  - Sampling rate 96000 Hz
//  - 6 bytes per sample set (3 channels, 16 bits per channel)
//  - 1 SPI Block is then 8192/6 = 1365.333 sample sets or about 14 ms worth of data in
//  - 30 seconds is 30/.014  = about 2142 SPI blocks
//
// N.B. Make NUM_SPI_BLOCKS an integer multiple of 3 for alignment
// reasons

//#define NUM_SPI_BLOCKS (2100*10)   // 5 minute buffer
#define NUM_SPI_BLOCKS (2100 * 2)    //1 minute buffer
#define RAM_SIZE (NUM_SPI_BLOCKS * SPI_BLOCK_SIZE) // bytes

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

extern int g_systemState;

#endif
