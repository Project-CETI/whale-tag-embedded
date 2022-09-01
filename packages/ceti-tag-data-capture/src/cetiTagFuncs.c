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

    char cTemp[256]; // strings and some

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

    if (!strncmp(g_command, "testSerial", 10)) {
        printf("hdlCmd(): Testing Recovery Serial Link\n");
        testRecoverySerial();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Testing Recovery Serial Link\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "bwOn", 4)) {
        printf("hdlCmd(): Turn on burnwire\n");
        burnwireOn();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Turned burnwire on\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "bwOff", 5)) {
        printf("hdlCmd(): Turn off burnwire\n");
        burnwireOff();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Turned burnwire off\n");
        fclose(g_rsp);
        return 0;
    }    


    if (!strncmp(g_command, "rcvryOn", 7)) {
        printf("hdlCmd(): Turn on power to the Recovery Board\n");
        rcvryOn();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Turned Recovery Board on\n");
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "rcvryOff", 8)) {
        printf("hdlCmd(): Turn off power to the Recovery Board\n");
        rcvryOff();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Turned Recovery Board off\n");
        fclose(g_rsp);
        return 0;
    }

    if ( !strncmp(g_command,"resetIMU",8) ) {
        printf("hdlCmd(): Resetting the IMU\n");
        resetIMU();
        g_rsp = fopen(RSP,"w");
        fprintf(g_rsp,"hdlCmd(): Reset the IMU \n"); //echo it
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
        start_audio_acq();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): Acquisition Started\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "stopAcq", 7)) {
        printf("hdlCmd(): Stopping Acquisition\n");
        stop_audio_acq();
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
        setup_audio_192kHz();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_96", 5)) {
        printf("hdlCmd(): Set sampling rate to 96 kHz\n");
        setup_audio_96kHz();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_48", 5)) {
        printf("hdlCmd(): Set sampling rate to 48 kHz\n");
        setup_audio_48kHz();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "sr_dflt", 7)) {
        printf("hdlCmd(): Set sampling rate to default (750 Hz)\n");
        setup_audio_default();
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "hdlCmd(): ADC Setting Changed\n"); // echo it
        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "resetFIFO", 9)) {
        printf("hdlCmd(): Resetting the FIFO\n");
        reset_audio_fifo();
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
        fprintf(g_rsp, "initTag	    Initialize the Tag\n");

        fprintf(g_rsp, "configFPGA  Load FPGA bitstream\n");
        fprintf(g_rsp, "verFPGA     Get FPGA version\n");

        fprintf(g_rsp, "resetFPGA   Reset FPGA state machines\n");
        fprintf(g_rsp, "resetFIFO   Reset audio HW FIFO\n");
        fprintf(g_rsp, "checkCAM    Verify hardware control link\n");

        fprintf(g_rsp, "startAcq    Start acquiring samples from the FIFO\n");
        fprintf(g_rsp, "stopAcq     Stop acquiring samples from  the FIFO\n");
        fprintf(g_rsp, "sr_192      Set sampling rate to 192 kHz \n");
        fprintf(g_rsp, "sr_96       Set sampling rate to 96 kHz\n");
        fprintf(g_rsp, "sr_48       Set sampling rate to 48 kHz \n");
        
        fprintf(g_rsp,"resetIMU    Pulse the IMU reset line \n");
        fprintf(g_rsp,
                "learnIMU    Dev only - sandbox for exploring IMU BNO08x\n");
        fprintf(g_rsp, "setupIMU    Dev only - bringing up IMU BNO08x\n");
        fprintf(g_rsp, "getRotation Dev only - bringing up IMU BNO08x\n");

        fprintf(g_rsp, "bwOn        Turn on the burnwire current\n");
        fprintf(g_rsp, "bwOff       Turn off the burnwire current\n");

        fprintf(g_rsp, 
                "rcvryOn     Turn on the Recovery Board\n");
        fprintf(g_rsp, 
                "rcvryOff    Turn off the Recovery Board\n");

        fprintf(g_rsp, 
                "testSerial  Test Recovery Board serial link\n");

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
// Initialize and start up for various i2c peripherals
//-----------------------------------------------------------------------------
// 
// At boot, system should check to make sure all devices are responsive and
// provide an indication (green LED) to the user that the Tag passed POST.  If a
// periph is missing, it should be logged as such, and if possible the Tag
// should move on...a restart during autonomous operation should do as much as
// possible to continue collecting data, even if something is broken.

int initI2cDevices() // In work
{
    int fd;


    // light sensor
    #if USE_LIGHT_SENSOR
    if ( (fd=i2cOpen(1,ADDR_LIGHT,0) ) < 0 ) {
        printf("initI2cDevices(): Failed to connect to the light sensor\n");
        return (-1);
    }
    else {
        i2cWriteByteData(fd,0x80,0x1); //wake the light sensor up
    }
    #endif


    // Battery protector UV and OV cutoffs
    #if USE_BATTERY_GAUGE
    if ( (fd=i2cOpen(1,ADDR_GAS_GAUGE,0) ) < 0 ) {
        printf("initI2cDevices(): Failed to connect to the light sensor\n");
        return (-1);
    }
    else {
        i2cWriteByteData(fd,BATT_CTL,BATT_CTL_VAL); //establish undervoltage cutoff
        i2cWriteByteData(fd,OVER_VOLTAGE,OV_VAL); //establish undervoltage cutoff
    }
    #endif


    // IMU
    #if USE_IMU
    resetIMU();
    #endif

    // Burn wire
    #if USE_BURNWIRE
    burnwireOff();
    #endif


    // Other devices

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


    CETI_LOG("initTag(): Initializing and checking hardware peripherals");

    // Configure the FPGA and log version
    #if USE_FPGA
    char cTemp[16];
    if (!loadFpgaBitstream()) {
        cam(0x10, 0, 0, 0, 0, cTemp);
        CETI_LOG("initTag(): FPGA initial configuration successful, Ver: 0x%02X%02X ",
                 cTemp[4], cTemp[5]);
    } else {
        CETI_LOG("initTag(): FPGA initial configuration failed");
        return (-1);
    }
    #endif

    // Set the ADC Up with defaults (96 kHz)
    #if USE_AUDIO
    if (!setup_audio_96kHz())
        CETI_LOG("initTag(): Succesfully set sampling rate to 96 kHz");
    else {
        CETI_LOG("initTag(): ADC initial configuration failed - ADC register did not read "
                 "back as expected");
        return (-1);
    }
    #endif

    // Configure other peripherals here as needed

    if (!initI2cDevices())
        CETI_LOG("initTag(): Succesfully initialized I2c devices");
    else {
        CETI_LOG("I2C general failure");
        return (-1);
    }

    return (0); // Done, the tag is ready
}

//-----------------------------------------------------------------------------
// RTC second counter
//-----------------------------------------------------------------------------

int getRtcCount(unsigned int *pRtcCount) {

    int fd;
    int rtcCount, rtcShift = 0;
    char rtcCountByte[4];

    if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
        CETI_LOG("getRtcCount(): Failed to connect to the RTC");
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

        *pRtcCount = rtcCount;
    }

    i2cClose(fd);
    return (0);
}

int resetRtcCount() {
    int fd;

    CETI_LOG("resetRtcCount(): Exectuting");

    if ((fd = i2cOpen(1, ADDR_RTC, 0)) < 0) {
        CETI_LOG("getRtcCount(): Failed to connect to the RTC");
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

unsigned int getTimeDeploy(void) {
    // open sensors.csv and get the first RTC timestamp for use
    // by state machine deployment timings.  

    FILE *sensorsCsvFile = NULL;
    char *pTemp;

    char line[512];
    
    char strTimeDeploy[16];
    
    unsigned int timeDeploy;

    sensorsCsvFile = fopen(SNS_FILE, "r");
    if (sensorsCsvFile == NULL) {
        CETI_LOG("getTimeDeploy():cannot open sensor csv output file");
        return (-1);
    }

    fgets(line, 512, sensorsCsvFile); // first line is always the header    
    fgets(line, 512, sensorsCsvFile); // first line of the actual data

    // parse out the RTC count, which is in the 2nd column of the CSV
    for(pTemp = line; *pTemp != ',' ; pTemp++);  //find first comma
    strncpy(strTimeDeploy, pTemp+1, 10);         //copy the string    
    strTimeDeploy[10] = '\0';                    //append terminator
    timeDeploy = strtoul(strTimeDeploy,NULL,0);  //convert to uint

    fclose(sensorsCsvFile);
    return (timeDeploy);
}


int testRecoverySerial(void) {   

//Tx a test message on UART, loopback to confirm FPGA and IO connectivity

    int i;

    char buf[4096]; 
    char testbuf[16]= "Hello Whales!\n";

    int fd;
    int bytes_avail;

    fd = serOpen("/dev/serial0",9600,0);

    if(fd < 0) {
        printf ("testRecoverySerial(): Failed to open the serial port\n");
        return (-1);
    }
    else {
        printf("testRecoverySerial(): Successfully opened the serial port\n");
    }

    for(i=0;i<128;i++) {
        usleep (1000000);
        
        bytes_avail = (serDataAvailable(fd));     
        printf("%d bytes are available from the serial port\n",bytes_avail);      
        serRead(fd,buf,bytes_avail);
        printf("%s",buf);

        printf("Trying to write to the serial port with pigpio\n");
        serWrite(fd,testbuf,14); //test transmission

    }


    return (0);




}