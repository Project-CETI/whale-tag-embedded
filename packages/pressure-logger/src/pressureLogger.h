//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Dual Pressure Sensor Data Logger 
// 
//-----------------------------------------------------------------------------
// Version    Date    Description					
//  0.00    05/26/23  Begin work, establish framework for the logger
//

//-----------------------------------------------------------------------------
// File: pressureLogger.h
//
// Description: This is the header for a utility program for logging pressure data from a pair
// Keller 4LD sensors. 
//
// This program targets the Pi Zero 2 platform and has been branched from the
// v2_2_refactor build which includes many features not needed for the pressure
// logger. Those features are disabled either in the build script or otherwise
// so that this program can run on a standalone Pi Zero 2 and not require
// any sensors other than the 2 Keller devices.
//
//
// This design uses two threads.  One thread is for interacting with the program
// via named pipes using a command/response protocol, which is useful for development 
// and adds some flexibility. The second thread simply fetches readings from the Kellers
// and writes them to a .csv file which is the basic requirement for the program
//
//-----------------------------------------------------------------------------

#ifndef WRAP_H
#define WRAP_H


#define VERSION "0.0 Begin Implementation"


#include <stdio.h>

#if 0

typedef enum {            //Tag operational states for deployment sequencing
	
	ST_CONFIG=0,          //get the deployment parameters from config file
    ST_START=1,  		  //turn on the audio recorder, illuminate ready LED   
    ST_DEPLOY=2,		  //wait for whale to dive		
    ST_REC_SUB=3,		  //recording while underwater	
    ST_REC_SURF=4,		  //recording while surfaced - trying for a GPS fix	
    ST_BRN_ON=5,          //burnwire is on, may or may not be at the surface when in this state
    ST_RETRIEVE=7,        //burnwire timed out, likely at surface, monitor GPS and transmit coord if enough battery
    ST_SHUTDOWN=8,        //battery critical, put system in minimum power mode

} wt_state_t; 

typedef struct {  //To hold rotation vector input report information
	char reportID;
	char sequenceNum;
	char status;
	char delay;
	signed short quat_i;
	signed short quat_j;
	signed short quat_k;
	signed short  quat_real;
	signed short  accEstimate;
} rotation_t;

# endif

typedef struct {  //To hold Keller 4LD pressure sensor data
	char status;
	char p_msb;
	char p_lsb;
	char t_msb;
	char t_lsb;
} psns_t;

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

extern void * cmdHdlThread( void * paramPtr);
//extern void * spiThread(void * paramPtr);
//extern void * writeDataThread(void * paramPtr);
extern void * sensorThread(void * paramPtr);
//extern void isr_get_fifo_block(void); 

extern int hdlCmd(void);


#if 0
extern int initTag(void);

extern int loadFpgaBitstream(void);

extern void cam(unsigned int opcode,
		unsigned int arg0, unsigned int arg1,
	    unsigned int pld0, unsigned int pld1, char *pResponse);
	    
extern int start_acq(void);
extern int stop_acq(void);
extern int setup_192kHz(void);
extern int setup_96kHz(void);
extern int setup_48kHz(void);
extern int setup_default(void);
extern int setup_sim_rate(char rate);
extern int reset_fifo(void);

extern int initI2cDevices(void); 
extern int learnIMU(void);
extern int setupIMU(void);
extern int getRotation(rotation_t *rotation);
extern int demoGPS(void);

extern int rfOn(void);
extern int rfOff(void);
extern int burnwireOn(void);
extern int burnwireOff(void);

extern int getRtcCount( int * pRtcCount);
extern int resetRtcCount (void);
extern int getTimeDeploy(void);

extern int getBoardTemp (int * boardTemp);

#endif 
extern int getTempPsns (double * pressureSensorData);

#if 0
extern int getBattStatus (double * batteryData);
extern int getQuaternion (short * quaternion);
extern int getAmbientLight(int * ambientLight);
extern int getGpsLocation(char * gpsLocation);
extern int updateState(int presentState);
extern int burnwireOn(void);
extern int burnwireOff(void);
extern int rfOn(void);
extern int rfOff(void);
extern int xbTx(void);
#endif

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

//#define CETI_CONFIG_FILE "../config/ceti-config.txt"
//#define LOGFILE "../log/cetiLog"  //log file

#define CMD "../ipc/Command"     //fifo locations
#define RSP "../ipc/Response"

//#define ACQ_FILE "../data/audio/audio_raw.dat"  
//#define ACQ_FILE "../data/audio/acq"
#define SNS_FILE "../data/pressure.csv"

//-----------------------------------------------------------------------------
// Wiring Pi For FPGA configuration

#if 0
#define DONE    (27)  	// GPIO 27
#define INIT_B  (25) 	// GPIO 25
#define PROG_B  (26) 	// GPIO 26
#define CLOCK   (21) 	// GPIO 21
#define DATA    (20)  	// GPIO 20
#define FPGA_BITSTREAM "../config/top.bin"      //fpga bitstream
#define BITSTREAM_SIZE_BYTES  (243048 * 2)		// See Xilinx Configuration User Guide UG332

//-----------------------------------------------------------------------------
// Wiring Pi For CAM

#define RESET   (5)	    	// GPIO 5
#define DIN     (19)		// GPIO 19  FPGA -> HOST
#define DOUT    (18) 		// GPIO 18  HOST-> FPGA
#define SCK     (16) 		// Moved from GPIO 1 to GPIO 16 to free I2C0 
#define NUM_BYTES_MESSAGE 8

//-----------------------------------------------------------------------------
// GPS and XBEE Reserved pins (N.B. These are not used by the Pi SW. Rather,
// the 3 serial ports (GPS, RTK and XBEE) are muxed and driven by the FPGA). 
// However, the HAT pins are used for interconnect between the boards,
// which means these pins can only be used as inputs to the Pi.

#define GPS_RX (17)  
#define GPS_TX (4)
#define RTK_RX (23)
#define RTK_TX (24)
#define XBEE_RX (6)
#define XBEE_TX (12)

//-----------------------------------------------------------------------------
// Data Acq SPI Settings and Audio Data Buffering

#define SPI_CE (0)
#define SPI_CLK_RATE (15000000) 	//Hz

// SPI Block Size

#define HWM (256) //High Water Mark from Verilog code - these are 32 byte chunks (2 sample sets)
#define SPI_BLOCK_SIZE (HWM * 32)  //make SPI block size <= HWM * 32 otherwise may underflow

// 	NUM_SPI_BLOCKS is the number of blocks acquired before writing out to mass storage
// 	A setting of 2100 will give buffer about 30 seconds at a time if sample rate is 96 kHz
// 	Example sizing of buffer (DesignMaps.xlsx has a calcuator for this)
// 		* SPI block HWM * 32 bytes = 8192 bytes (25% of the hardware FIFO in this example)
// 		* Sampling rate 96000 Hz
// 		* 6 bytes per sample set (3 channels, 16 bits per channel)
// 		* 1 SPI Block is then 8192/6 = 1365.333 sample sets or about 14 ms worth of data in time
// 		* 30 seconds is 30/.014  = about 2142 SPI blocks
//
//      * N.B. Make NUM_SPI_BLOCKS an integer multiple of 3 for alignment reasons

#define NUM_SPI_BLOCKS ( 2100 ) //2100 is about 30 secs at 96 kSPS 3-channels 

#define RAM_SIZE (NUM_SPI_BLOCKS * SPI_BLOCK_SIZE) //bytes

#define RAM_XFERS_PER_FILE ( 10 )  // 10 yields about 5 minute audio files if NUM_SPI_BLOCKS is 2100

//-----------------------------------------------------------------------------
// Dacq Flow Control Handshaking Interrupt
// The flow control signal is GPIO 22, (WiringPi 3). The FPGA sets this when FIFO is at HWM and
// clears it at LWM as viewed from the write count port. The FIFO margins may need to be tuned
// depending on latencies and how this program is constructed. The margins are set in the FPGA
// Verilog code, not here.

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

#endif