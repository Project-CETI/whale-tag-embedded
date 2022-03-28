//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagFuncs.c
// Description: Functions used by the top wrapper
//-----------------------------------------------------------------------------
#include <pigpio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cetiTagLogging.h"
#include "cetiTagSensors.h"
#include "cetiTagWrapper.h"

//-----------------------------------------------------------------------------
// Command interpreter and handler
//-----------------------------------------------------------------------------
int hdlCmd(void) {
    int i, n;
    char cArg[256], cTemp[256]; // strings and some
    char *pTemp1, *pTemp2;      // working pointers
    //-----------------------------------------------------------------------------
    // Part 1 - quit or any other special commands here
    if (!strncmp(g_command, "quit", 4)) {
        printf("Quitting\n");
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "Stopping the App\n"); // echo it back
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
        fprintf(g_rsp, "Running Debug Routine(s)\n"); // echo it back
        fclose(g_rsp);
        CETI_LOG("debug %d", 1234);
        return 0;
    }

    if (!strncmp(g_command, "rfOn", 4)) {
        printf("hdlCmd(): Turn on power for the GPS and Xbee\n");
        rfOn();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Turned RF supply on\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "rfOff", 5)) {
        printf("hdlCmd(): Turn on power for the GPS and Xbee\n");
        rfOff();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Turned RF supply off\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "getRotation", 11)) {

        rotation_t rotation;

        printf("hdlCmd(): Retrieve an IMU Vector Rotation input report\n");
        setupIMU();
        getRotation(&rotation);
        printf("Testing the function 0x%02X 0x%02X \n", rotation.reportID,
               rotation.sequenceNum);
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Finished getting the Rotation report\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "setupIMU", 8)) {
        printf("hdlCmd(): Turning on IMU features\n");
        setupIMU();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Finished turning on IMU features\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "learnIMU", 8)) {
        printf("hdlCmd(): Experimenting with IMU\n");
        learnIMU(); // a sandbox function in sensors.c
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Finished running IMU experiments\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "initTag", 4)) {

        printf("hdlCmd():Initializing the Tag\n");

        if (!initTag()) {
            CETI_LOG("Tag initialization successful");
            g_rsp = fopen(RSP, "w");
            fprintf(g_rsp, "hdlCmd(): Tag initialization successful\n");
        } else {
            CETI_LOG("Tag Initialization Failed");
            g_rsp = fopen(RSP, "w");
            fprintf(g_rsp,
                    "hdlCmd(): Tag initialization Failed\n"); // echo it back
        }
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "configFPGA", 10)) {
        printf("hdlCmd(): Configuring the FPGA\n");

        if (!loadFpgaBitstream()) {
            CETI_LOG("FPGA Configuration Succeeded");
            g_rsp = fopen(RSP, "w");
            fprintf(g_rsp,
                    "hdlCmd(): Configuring FPGA Succeeded\n"); // echo it back
        } else {
            CETI_LOG("FPGA Configuration Failed");
            g_rsp = fopen(RSP, "w");
            fprintf(
                g_rsp,
                "hdlCmd(): Configuring FPGA Failed - Try Again\n"); // echo it
                                                                    // back
        }
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "verFPGA", 7)) {

        printf("hdlCmd(): Querying FPGA Version\n");

        cam(0x10, 0, 0, 0, 0, cTemp);

        CETI_LOG("FPGA Version: 0x%02X%02X\n", cTemp[4], cTemp[5]);

        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "FPGA Version: 0x%02X%02X\n", cTemp[4], cTemp[5]);

        printf("hdlCmd(): FPGA Version %02X%02X\n", cTemp[4],
               cTemp[5]); // should be FE
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "checkCAM", 8)) {
        printf("hdlCmd(): Testing CAM Link\n");
        cam(0, 0, 0, 0, 0, cTemp);

        g_rsp = fopen(RSP, "w");

        if (cTemp[4] == 0xAA)
            fprintf(g_rsp, "CAM Link OK\n");
        else
            fprintf(g_rsp, "CAM Link Failure\n");

        printf("hdlCmd(): camcheck - expect 0xAA: %02X \n",
               cTemp[4]); // should be FE
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "startAcq", 8)) {
        printf("hdlCmd(): Starting Acquisition\n");
        start_acq();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Acquisition Started\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "stopAcq", 7)) {
        printf("hdlCmd(): Stopping Acquisition\n");
        stop_acq();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Acquisition Stopped\n"); // echo it
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
        fprintf(g_rsp, "hdlCmd(): FPGA Reset Completed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_192", 6)) {
        printf("hdlCmd(): Set sampling rate to 192 kHz\n");
        setup_192kHz();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_96", 5)) {
        printf("hdlCmd(): Set sampling rate to 96 kHz\n");
        setup_96kHz();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_48", 5)) {
        printf("hdlCmd(): Set sampling rate to 48 kHz\n");
        setup_48kHz();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_dflt", 7)) {
        printf("hdlCmd(): Set sampling rate to default (750 Hz)\n");
        setup_default();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
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
                   ' '); // find space char - separates the params in the script
        pTemp1++;        // this is pointing to the period argument now
        pTemp2 = strchr(g_command, 0x0A); // and this is the end of the command
        n = pTemp2 - pTemp1;              // will be 1,2 or 3 chars long

        for (i = 0; i < n; i++)
            cArg[i] =
                *(pTemp1 + i); // this is the period in microseconds as a string
        cArg[n] = '\0';        // append null term
        setup_sim_rate(atoi(cArg));
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp,
                "hdlCmd(): Setup the Simulated Sampling Rate\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "resetFIFO", 9)) {
        printf("hdlCmd(): Resetting the FIFO\n");
        reset_fifo();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): FIFO Reset\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    else {
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "\n"); // echo it
        fprintf(g_rsp, "CETI Tag Electronics Available Commands\n");
        fprintf(g_rsp,
                "---------------------------------------------------------\n");
        fprintf(g_rsp, "initTag	   Initialize the Tag\n");

        fprintf(g_rsp, "configFPGA  Load FPGA bitstream\n");
        fprintf(g_rsp, "verFPGA  Get FPGA version\n");

        fprintf(g_rsp, "resetFPGA   Reset FPGA state machines\n");
        fprintf(g_rsp, "resetFIFO   Reset audio HW FIFO\n");
        fprintf(g_rsp, "checkCAM    Verify hardware control link\n");

        fprintf(g_rsp, "startAcq    Start acquiring samples from the FIFO\n");
        fprintf(g_rsp, "stopAcq     Stop acquiring samples from  the FIFO\n");
        fprintf(g_rsp, "sr_192      Set sampling rate to 192 kHz \n");
        fprintf(g_rsp, "sr_96       Set sampling rate to 96 kHz\n");
        fprintf(g_rsp, "sr_48       Set sampling rate to 48 kHz \n");
        fprintf(g_rsp,
                "sr_dflt     Set sampling rate to 750 Hz (ADC default)\n");
        fprintf(g_rsp, "sr_sim      Set simulated data sampling rate (only for "
                       "use with simulator FPGA)\n");

        fprintf(g_rsp,
                "learnIMU    Dev only - sandbox for exploring IMU BNO08x\n");
        fprintf(g_rsp, "setupIMU    Dev only - bringing up IMU BNO08x\n");
        fprintf(g_rsp, "getRotation Dev only - bringing up IMU BNO08x\n");

        fprintf(g_rsp, "rfon        Testing control of power to GPS and XB\n");
        fprintf(g_rsp, "rfoff       Testing control of power to GPS and XB\n");

        fprintf(g_rsp, "\n");
        fclose(g_rsp);
        return 0;
    }
    return 1;
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
        fprintf(stderr, "loadFpgaBitstream(): Failed to allocate memory for "
                        "the configuration file\n");
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
        // printf("received %d: %02X \n",j,recv_packet[j]);
        *(pResponse + j) = recv_packet[j]; // fill in the response
    }
}

//-----------------------------------------------------------------------------
// Initialize and start up for various CETI sensors - sandbox
//-----------------------------------------------------------------------------
// In work, developing ideas
// At boot, system should check to make sure all devices are responsive and
// provide an indication (green LED) to the user that the Tag passed POST.  If a
// periph is missing, it should be logged as such, and if possible the Tag
// should move on...a restart during autonomous operation should do as much as
// possible to continue collecting data, even if something is broken.

int initI2cDevices() // In work
{

    int fd;
    int result;

    // Open a connection to the io expander on the power board
    if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_PWRBD, 0)) < 0) {
        printf("Failed to open I2C connection for IO Expander on the Power "
               "Board\n");
        return -1;
    }
    i2cWriteByteData(fd, IOX_CONFIG,
                     0x04); // make all pins outputs except for RF

    result = i2cReadByteData(fd, IOX_CONFIG);
    if (result != 0x04) {
        printf("iinitI2CDevices(): IO Expander on Power Board did not "
               "initialize as expected\n");
        return -1;
    }
    // printf("Read configuration register on the PB IOX: 0x%02X\n",result);

    burnwireOff();

    // Make sure ready LED is initialized to OFF and that the RF power is OFF
    result = i2cReadByteData(fd, IOX_OUTPUT);
    result = result & ~RDY_LED;
    i2cWriteByteData(fd, IOX_OUTPUT, result);

    i2cClose(fd);

    return 0;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
// 1) configure the FPGA and log the revision
// 2) TODO check all I2C peripherals are present and accounted for (adding as
// each device is integrated during dev) 3) setup default sampling rate (this
// also checks the CAM link is good) 5) turn on indicator for user to ascertain
// if the tag is ready to go or not 6) start acquiring data

int initTag(void) {

    char cTemp[16];

    CETI_LOG("Application Started");
    printf("main(): Initializing and Checking Hardware Peripherals\n");

    // Configure the FPGA and log version
    if (!loadFpgaBitstream()) {
        cam(0x10, 0, 0, 0, 0, cTemp);
        CETI_LOG("FPGA initial configuration successful, Ver: 0x%02X%02X ",
                 cTemp[4], cTemp[5]);
    } else {
        CETI_LOG("FPGA initial configuration failed");
        return (-1);
    }

    // Set the ADC Up with defaults (96 kHz)
    if (!setup_96kHz())
        CETI_LOG("Succesfully set sampling rate to 96 kHz");
    else {
        CETI_LOG("ADC initial configuration failed - ADC register did not read "
                 "back as expected");
        return (-1);
    }

    // Configure other peripherals

    if (!initI2cDevices())
        CETI_LOG("Succesfully initialized I2c devices");
    else {
        CETI_LOG("I2C general failure");
        return (-1);
    }

    return (0); // Done, the tag is ready
}

//-----------------------------------------------------------------------------
// RTC second counter
//-----------------------------------------------------------------------------

int getRtcCount(int *pRtcCount) {

    int fd;
    int rtcCount, rtcShift = 0;
    char rtcCountByte[4];

    if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
        printf("getRtcCount(): Failed to connect to the RTC");
        return (-1);
    }

    else { // read the time of day counter and assemble the bytes in 32 bit int

        rtcCountByte[0] = i2cReadByteData(fd, 0x00);
        rtcCountByte[1] = i2cReadByteData(fd, 0x01);
        rtcCountByte[2] = i2cReadByteData(fd, 0x02);
        rtcCountByte[3] = i2cReadByteData(fd, 0x03);

        rtcCount = (rtcCountByte[0]);
        rtcShift = (rtcCountByte[1] << 8);
        rtcCount = rtcCount | rtcShift;
        rtcShift = (rtcCountByte[2] << 16);
        rtcCount = rtcCount | rtcShift;
        rtcShift = (rtcCountByte[3] << 24);
        rtcCount = rtcCount | rtcShift;

        // printf("RTC Count is %d\n",rtcCount);

        *pRtcCount = rtcCount;
    }

    i2cClose(fd);
    return (0);
}

int resetRtcCount() {
    int fd;

    if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
        printf("getRtcCount(): Failed to connect to the RTC\n");
        return (-1);
    }

    else {
        i2cWriteByteData(fd, 0x00, 0x00);
        i2cWriteByteData(fd, 0x01, 0x00);
        i2cWriteByteData(fd, 0x02, 0x00);
        i2cWriteByteData(fd, 0x03, 0x00);
    }
    i2cClose(fd);
    return (0);
}

int getTimeDeploy(void) {
    // open sensors.csv and get the first timestamp

    FILE *sensorsCsvFile = NULL;
    char line[512];
    char strTimeDeploy[16];
    double timeDeploy;

    sensorsCsvFile = fopen("../data/sensors/sensors.csv", "r");
    if (sensorsCsvFile == NULL) {
        fprintf(stderr, "main():cannot open sensor csv output file\n");
        return (-1);
    }

    fgets(line, 512, sensorsCsvFile); // first line is the header
    fgets(line, 512, sensorsCsvFile); // first line of data
    strncpy(strTimeDeploy, line, 10); // time stamp
    strTimeDeploy[10] = '\n';
    timeDeploy = atol(strTimeDeploy);

    // printf("the line is %s \n",line);
    // printf("the deploy time string is %s \n",strTimeDeploy);
    // printf("the deploy time float is %f \n",timeDeploy);

    fclose(sensorsCsvFile);
    return (timeDeploy);
}
