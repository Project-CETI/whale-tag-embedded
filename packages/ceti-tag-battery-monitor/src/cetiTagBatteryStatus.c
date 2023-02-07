//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagBatteryStatus.c
// 
// Description: The code in this file supports battery monitoring and is part of 
// of the ceti-tag-battery-monitor systemd service. The service uses named pipes 
// to execute functions to utilize the Tag's battery monitoring hardware and 
// power control features.   
//              
//-----------------------------------------------------------------------------

#include <pigpio.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CMD "cetiCommand"  // fifos for exchanging data with shell script or
#define RSP "cetiResponse" // other program(s)

#define ADDR_GAS_GAUGE 0x59

// DS2778 Gas Gauge Registers and Settings
#define PROTECT 0x00
#define BATT_CTL 0x60  
#define OVER_VOLTAGE 0x7F
#define CELL_1_V_MS 0x0C
#define CELL_1_V_LS 0x0D
#define CELL_2_V_MS 0x1C
#define CELL_2_V_LS 0x1D
#define BATT_I_MS 0x0E
#define BATT_I_LS 0x0F
#define R_SENSE 0.025

#define BATT_CTL_VAL 0X8E  //SETS UV CUTOFF TO 2.6V
#define OV_VAL 0x5A //SETS OV CUTOFF TO 4.2V

void *cmdHdlThread(void *paramPtr);
int hdlCmd(void);
void cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
                unsigned int pld0, unsigned int pld1, char *pResponse);
int getBattStatus(double *batteryData);

//-----------------------------------------------------------------------------
// GPIO needed to use the hardware Control and Monitor interface (CAM)
#define RESET (5) // GPIO 5
#define DIN (19)  // GPIO 19  FPGA -> HOST
#define DOUT (18) // GPIO 18  HOST-> FPGA
#define SCK (16)  // Moved from GPIO 1 to GPIO 16 to free I2C0
#define NUM_BYTES_MESSAGE 8
//-----------------------------------------------------------------------------
// Named pipes for communication with other programs
FILE *g_cmd = NULL;
FILE *g_rsp = NULL;

char g_command[256];
int g_exit = 0;
int g_cmdPend = 0;

//-----------------------------------------------------------------------------
void *cmdHdlThread(void *paramPtr) {
    while (!g_exit) {
        if (!g_cmdPend) { // open the comand pipe for reading and wait for a
                          // command
            g_cmd = fopen(CMD, "r");      // blocked here waiting
            fgets(g_command, 256, g_cmd); // get the command
            fclose(g_cmd);                // close the pipe
            g_cmdPend = 1;
        } else {
            usleep(100);
        }
    }
    return NULL;
}
//-----------------------------------------------------------------------------
int hdlCmd(void) {

    char cTemp[256]; 
    double batteryData[3];

    //-----------------------------------------------------------------------------
    // Part 1 - quit or any other special commands here
    if (!strncmp(g_command, "quit", 4)) {
        printf("Quitting\n");
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "Stopping the battery monitoring app\n"); // echo it back
        fclose(g_rsp);  
        g_exit = 1;
        return 0;
    }  

    if (!strncmp(g_command, "checkCell_1", 11)) {
     //   printf("hdlCmd(): Read cell 1 voltage\n");
        getBattStatus(batteryData);
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "%.2f\n", batteryData[0]);
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "checkCell_2", 11)) {
     //   printf("hdlCmd(): Read cell 2 voltage\n");
        getBattStatus(batteryData);
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "%.2f\n", batteryData[1]);
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "powerdown", 9)) {
        printf("hdlCmd(): Power down via FPGA \n");
        g_rsp = fopen(RSP, "w");
        // now send the FPGA shutdown opcode via CAM
        // opcode 0xF will do a register write on i2c1
        // 0x59 is the 7-bit i2c addr of BMS IC, 
        // set register 0 to 0 will turn it off
        // set register 0 to 3 to reactivate
        cam(0x0F, 0xB2, 0x00, 0x00, 0x00, cTemp);
        // To complete the shutdown, the Pi must be powered down
        // now by an external process.  Currently the design 
        // uses the tagMonitor script to do the Pi shutdown.
        // After the Pi turns off, the FPGA will disable discharging
        // and charging by sending a final i2c message to the BMS chip 
        // to pull the plug. 
        // A charger connection is required to wake up the tag after this event
        // and charging/discharging needs to subsequently be 
        // renabled.

        fprintf(g_rsp,"hdlCmd(): Powering the tag down!\n");
        fclose(g_rsp);
        return 0;
    }

    else {
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "\n"); // echo it
        fprintf(g_rsp, "Battery monitor did not recognize that command\n");
        fprintf(g_rsp, "\n");
        fclose(g_rsp);
        return 0;
    }
    return 1;
}
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
//-----------------------------------------------------------------------------
int getBattStatus(double *batteryData) {

    int fd, result;
    signed short voltage, current;

    if ((fd = i2cOpen(1, ADDR_GAS_GAUGE, 0)) < 0) {
        return (-1);
    }

    result = i2cReadByteData(fd, CELL_1_V_MS);
    voltage = result << 3;
    result = i2cReadByteData(fd, CELL_1_V_LS);
    voltage = (voltage | (result >> 5));
    voltage = (voltage | (result >> 5));
    batteryData[0] = 4.883e-3 * voltage;

    result = i2cReadByteData(fd, CELL_2_V_MS);
    voltage = result << 3;
    result = i2cReadByteData(fd, CELL_2_V_LS);
    voltage = (voltage | (result >> 5));
    voltage = (voltage | (result >> 5));
    batteryData[1] = 4.883e-3 * voltage;

    result = i2cReadByteData(fd, BATT_I_MS);
    current = result << 8;
    result = i2cReadByteData(fd, BATT_I_LS);
    current = current | result;
    batteryData[2] = 1000 * current * (1.5625e-6 / R_SENSE);

    i2cClose(fd);
    return (0);
}
//-----------------------------------------------------------------------------
int main(int argc, char *argv[]) {

    pthread_t cmdHdlThreadId = 0;
    
    pthread_create(&cmdHdlThreadId, NULL, &cmdHdlThread, NULL);
    
    if (gpioInitialise() < 0) {
        fprintf(stderr, "Battery monitor gpio failed to initialize properly");
    }
    
    while (!g_exit) { // main loop 
        usleep(100000);
        if (g_cmdPend) {
            hdlCmd();
            g_cmdPend = 0;
        }
    }
    
    pthread_cancel(cmdHdlThreadId);
    gpioTerminate();
    printf("Battery monitor program closing\n");
    return (0);   
}


