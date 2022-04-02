//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagSensors.c
// Description: Sensor-related functions and file handling (IMU functions
// excluded, in a separate module)
//-----------------------------------------------------------------------------

#include <sys/time.h>
#include <sys/param.h>
#include <zlib.h>

#include "cetiTagSensors.h"
#include "cetiTagWrapper.h"

static int presentState = ST_CONFIG;

int16_t quaternion[4];
double pressureSensorData[2];
double batteryData[3];
int rtcCount, boardTemp, ambientLight;
char gpsLocation[128];
int gpsPowerState = 0;

#define MAX_STATE_STRING_LEN (32)
char state_str[][MAX_STATE_STRING_LEN] = {
    "CONFIG",   "START",  "DEPLOY",   "REC_SUB",
    "REC_SURF", "BRN_ON", "RETRIEVE", "SHUTDOWN",
};

//-----------------------------------------------------------------------------
// Control and Monitor Thread
//
//  - Periodically get data from non-audio sensors and save to .csv
//  - Keep track of the operational state of the tag
//-----------------------------------------------------------------------------
void *sensorThread(void *paramPtr) {
    gzFile snsData; // data file for non-audio sensor data
    char header[] =
        "Timestampe(ms), RTC count, State, BoardTemp degC, WaterTemp degC, "
        "Pressure bar, Batt_V1 V, Batt_V2 V, Batt_I mA, Quat_i, Quat_j, "
        "Quat_k, Quat_Re, AmbientLight, GPS";
    struct timeval te;
    long long milliseconds;

    while (1) {

        gettimeofday(&te, NULL);
        milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
        getRtcCount(&rtcCount);
        getBoardTemp(&boardTemp);
        getTempPsns(pressureSensorData);
        getBattStatus(batteryData);
        getQuaternion(quaternion);
        getAmbientLight(&ambientLight);
        getGpsLocation(gpsLocation);

        snsData = gzopen(SNS_FILE, "wb");
        fopen(SNS_FILE, "a");
        if (snsData == NULL) {
            CETI_LOG("sensorThread(): could not find sensor file, creating a new one: %s", SNS_FILE);
            snsData = gzopen(SNS_FILE, "wt");
            gzprintf(snsData, "%s", header);
        }

        gzprintf(snsData, "%lld,", milliseconds);
        gzprintf(snsData, "%d,", rtcCount);
        gzprintf(snsData, "%s,", state_str[presentState]);
        gzprintf(snsData, "%d,", boardTemp);
        gzprintf(snsData, "%.2f,", pressureSensorData[0]);
        gzprintf(snsData, "%.2f,", pressureSensorData[1]);
        gzprintf(snsData, "%.2f,", batteryData[0]);
        gzprintf(snsData, "%.2f,", batteryData[1]);
        gzprintf(snsData, "%.2f,", batteryData[2]);
        gzprintf(snsData, "%d,", quaternion[0]);
        gzprintf(snsData, "%d,", quaternion[1]);
        gzprintf(snsData, "%d,", quaternion[2]);
        gzprintf(snsData, "%d,", quaternion[3]);
        gzprintf(snsData, "%d,", ambientLight);
        gzprintf(snsData, "\"%s\"\n", gpsLocation);
        gzclose(snsData);
        presentState = updateState(presentState);
        usleep(SNS_SMPL_PERIOD);
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// U-blox ZED F9P
//-----------------------------------------------------------------------------

int getGpsLocation(char *gpsLocation) {

    // Extract GNRMC only from the serial port stream
    // Return "GPS Module Not Active" message if the tag is in any state where
    // RF is not turned on

    char buf[4096] = {0};
    char buf2[256] = {0};
    int bytes_avail = 0;
    char *pTemp = NULL, *gnrmc = NULL;
    char match = 0;
    int fd = serOpen("/dev/serial0", GPS_BAUD, 0);

    if (!gpsPowerState) {
        strcpy(gpsLocation, "GPS module not active");
        return (-1);
    }

    if (fd < 0) {
        CETI_LOG("getGpsLocation(): Failed to open the serial port");
        return (-1);
    }

    // printf("getGpsLocation(): Successfully opened the serial port\n");
    while (bytes_avail < 1024) { // wait for some data from the module
        bytes_avail = serDataAvailable(fd);
        usleep(1000);
    }

    // printf("%d bytes are available from the serial port\n",bytes_avail);
    memset(buf, 0, 4096);
    serRead(fd, buf, bytes_avail); //

    //   Search for GNRMC sentence
    pTemp = buf;
    // scan the message stream for GNRMC sentence
    for (int i = 0; (i <= bytes_avail && !match); i++) {
        if (!strncmp(pTemp + i, "$GNRMC", 6)) {
            gnrmc = pTemp + i; // mark the start of the sentence
            match = 1;
            continue;
        }
    }
    if (match) {
        pTemp = strchr(gnrmc, '*');
        if (!pTemp) {
            // TODO
        }
        // length of the sentence-4 chars
        snprintf(buf2, pTemp - gnrmc + 4, "%s \n", gnrmc);
        strcpy(gpsLocation, buf2);
    }
    serClose(fd);
    return (0);
}

//-----------------------------------------------------------------------------
// BNO080  IMU
//-----------------------------------------------------------------------------

int getQuaternion(short *quaternion) {

    int fd;
    int numBytesAvail;
    char pktBuff[256] = {0};
    char shtpHeader[4] = {0};

    // parse the IMU quaternion vector input report

    if ((fd = i2cOpen(1, ADDR_IMU, 0)) < 0) {
        CETI_LOG("getRotation(): Failed to connect to the IMU");
        return -1;
    }

    // Byte   0    1    2    3    4   5   6    7    8      9     10     11
    //       |     HEADER      |            TIME       |  ID    SEQ
    //       STATUS....

    i2cReadDevice(fd, shtpHeader, 4);

    // msb is "continuation bit, not part of count"
    numBytesAvail = MIN(256, ((shtpHeader[1] << 8) | shtpHeader[0]) & 0x7FFF);

    if (numBytesAvail) {
        i2cReadDevice(fd, pktBuff, numBytesAvail);
        if (pktBuff[2] == 0x03) { // make sure we have the right channel
            quaternion[0] = (pktBuff[14] << 8) + pktBuff[13];
            quaternion[1] = (pktBuff[16] << 8) + pktBuff[15];
            quaternion[2] = (pktBuff[18] << 8) + pktBuff[17];
            quaternion[3] = (pktBuff[20] << 8) + pktBuff[19];
        }
    } else {
        setupIMU();
    }

    i2cClose(fd);
    return (0);
}

//-----------------------------------------------------------------------------
// Battery gas gauge
//-----------------------------------------------------------------------------

int getBattStatus(double *batteryData) {

    int fd, result;
    signed short voltage, current;

    if ((fd = i2cOpen(1, ADDR_GAS_GAUGE, 0)) < 0) {
        CETI_LOG("getBattStatus(): Failed to connect to the gas gauge");
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
// Keller pressure sensor
//-----------------------------------------------------------------------------

int getTempPsns(double *presSensorData) {

    int fd;
    short int temperature, pressure;
    char presSensData_byte[5];

    if ((fd = i2cOpen(1, ADDR_DEPTH, 0)) < 0) {
        CETI_LOG("getTempPsns(): Failed to connect to the depth sensor");
        return (-1);
    }

    i2cWriteByte(fd, 0xAC); // measurement request from the device
    usleep(10000);          // wait 10 ms for the measurement to finish

    i2cReadDevice(fd, presSensData_byte, 5); // read the measurement

    temperature = presSensData_byte[3] << 8;
    temperature = temperature + presSensData_byte[4];

    // convert to deg C
    // convert to bar - see Keller data sheet for the particular sensor in use
    presSensorData[0] = ((temperature >> 4) - 24) * .05 - 50; 
    pressure = presSensData_byte[1] << 8;
    pressure = pressure + presSensData_byte[2];
    // convert to bar
    presSensorData[1] = ((PMAX - PMIN) / 32768.0) * (pressure - 16384.0);

    i2cClose(fd);
    return (0);
}

//-----------------------------------------------------------------------------
// Ambient light sensor
//-----------------------------------------------------------------------------

int getAmbientLight(int *pAmbientLight) {
    int fd;
    if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_SNSBD, 0)) < 0) {
        CETI_LOG("getAmbientLight(): Failed to connect to the IO Expander");
        return (-1);
    }

    *pAmbientLight = (!!(i2cReadByteData(fd, 0x00) & BIT_4));
    i2cClose(fd);
    return (0);
}

//-----------------------------------------------------------------------------
// Board mounted temperature sensor SA56004
//-----------------------------------------------------------------------------

int getBoardTemp(int *pBoardTemp) {
    int fd;
    if ((fd = i2cOpen(1, ADDR_TEMP, 0)) < 0) {
        CETI_LOG("getBoardTemp(): Failed to connect to the temperature sensor");
        return (-1);
    }

    int boardTemp = i2cReadByteData(fd, 0x00);
    i2cClose(fd);
    *pBoardTemp = boardTemp;
    return (0);
}

//-----------------------------------------------------------------------------
// State Machine and Controls
// * Details of state machine are documented in the high-level design
//-----------------------------------------------------------------------------

int updateState(int presentState) {

    int nextState;

    // Config file and associated parameters
    static FILE *cetiConfig = NULL;
    char cPress_1[16], cPress_2[16], cVolt_1[16], cVolt_2[16], cTimeOut[16];
    static double d_press_1, d_press_2, d_volt_1, d_volt_2;
    static int timeOut_minutes, timeout_seconds;
    char line[256];
    char *pTemp;

    // i2c helper variables
    int fd;
    int result;

    // timing
    static int startTime = 0;
    // static int burnTimeStart;

    // Deployment sequencer FSM

    switch (presentState) {

    case (ST_CONFIG):

        // Load the deployment configuration

        // First, get deployment config values provided by the configuration
        // file
        // printf(" updateState(): debug - read config\n" );

        cetiConfig = fopen(CETI_CONFIG_FILE, "r");
        if (cetiConfig == NULL) {
            CETI_LOG("updateState():cannot open sensor csv output file");
            return (-1);
        }

        while (fgets(line, 256, cetiConfig) != NULL) {

            if (*line == '#')
                continue; // ignore comment lines

            if (!strncmp(line, "P1=", 3)) {
                pTemp = (line + 3);
                strcpy(cPress_1, pTemp);
            }
            if (!strncmp(line, "P2=", 3)) {
                pTemp = (line + 3);
                strcpy(cPress_2, pTemp);
            }
            if (!strncmp(line, "V1=", 3)) {
                pTemp = (line + 3);
                strcpy(cVolt_1, pTemp);
            }
            if (!strncmp(line, "V2=", 3)) {
                pTemp = (line + 3);
                strcpy(cVolt_2, pTemp);
            }
            if (!strncmp(line, "T0=", 3)) {
                pTemp = (line + 3);
                strcpy(cTimeOut, pTemp);
            }
        }

        fclose(cetiConfig);

        // printf("P1 is %s\n",cPress_1);
        // printf("P2 is %s\n",cPress_2);
        // printf("V1 is %s\n",cVolt_1);
        // printf("V2 is %s\n",cVolt_2);
        // printf("Timeout is %s\n",cTimeOut);

        d_press_1 = atof(cPress_1);
        d_press_2 = atof(cPress_2);
        d_volt_1 = atof(cVolt_1);
        d_volt_2 = atof(cVolt_2);
        timeOut_minutes =
            atol(cTimeOut); // the configuration file units are minutes
        timeout_seconds = timeOut_minutes * 60;

        // printf("P1 is %.2f\n",d_press_1);
        // printf("P2 is %.2f\n",d_press_2);
        // printf("V1 is %.2f\n",d_volt_1);
        // printf("V2 is %.2f\n",d_volt_2);
        // printf("Timeout is %d\n",timeOut);

        nextState = ST_START;

        break;

    case (ST_START):

        // Start recording and turn on green LED - ready to attach

        if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_PWRBD, 0)) < 0) {
            CETI_LOG("Failed to open I2C connection for IO Expander on the Power Board");
            return (-1);
        }

        result = i2cReadByteData(fd, IOX_OUTPUT);
        result = result | RDY_LED;
        i2cWriteByteData(fd, IOX_OUTPUT, result);
        i2cClose(fd);
        start_acq(); // kick off the audio recording

        startTime = getTimeDeploy(); // new v0.5 gets start time from the csv
        CETI_LOG("Deploy Start: %d", startTime);
        nextState = ST_DEPLOY; // underway!
        rfOn();                // turn on the GPS and XBee power supply

        break;

    case (ST_DEPLOY):

        // Waiting for 1st dive
        //	printf("State DEPLOY - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        if ((batteryData[0] + batteryData[1] < d_volt_1) ||
            (rtcCount - startTime > timeout_seconds)) {

            // burnTimeStart = rtcCount ;
            burnwireOn();
            nextState = ST_BRN_ON;
        }

        else if (pressureSensorData[1] > d_press_2)
            nextState = ST_REC_SUB; // 1st dive after deploy

        else {

            xbTx();
            nextState = ST_DEPLOY;
        }

        break;

    case (ST_REC_SUB):
        // Recording while sumberged
        //	printf("State REC_SUB - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        if ((batteryData[0] + batteryData[1] < d_volt_1) ||
            (rtcCount - startTime > timeout_seconds)) {
            // burnTimeStart = rtcCount ;
            burnwireOn();
            nextState = ST_BRN_ON;
        }

        else if (pressureSensorData[1] < d_press_1)
            nextState = ST_REC_SURF; // came to surface

        else {
            nextState = ST_REC_SUB;
            rfOff();
        }

        break;

    case (ST_REC_SURF):
        // Recording while at surface, trying to get a GPS fix
        //	printf("State REC_SURF - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        if ((batteryData[0] + batteryData[1] < d_volt_1) ||
            (rtcCount - startTime > timeout_seconds)) {
            //	burnTimeStart = rtcCount ;
            burnwireOn();
            nextState = ST_BRN_ON;
        }

        else if (pressureSensorData[1] > d_press_2) {
            rfOff();
            nextState = ST_REC_SUB; // back under....
        }

        else if (pressureSensorData[1] < d_press_1) {
            rfOn();
            xbTx();
            nextState = ST_REC_SURF;
        }

        else
            nextState = ST_REC_SURF; // hysteresis zone

        break;

    case (ST_BRN_ON):
        // Releasing
        //	printf("State BRN_ON - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        if (batteryData[0] + batteryData[1] < d_volt_2)
            nextState = ST_SHUTDOWN; // critical battery

        else if (batteryData[0] + batteryData[1] < d_volt_1)
            nextState = ST_RETRIEVE; // low batter

        // update 220109 to leave burnwire on without time limit
        // Leave burn wire on for 45 minutes or until the battery is depleted
        // else if (rtcCount - burnTimeStart > (60*45)) {
        //	//burnwireOff();  //leaving it on no time limit -change 220109
        //	nextState = ST_RETRIEVE;
        //}

        else
            nextState = ST_BRN_ON; // dwell with burnwire left on until battery
                                   // reaches low threshold

        if (pressureSensorData[1] <
            d_press_1) { // at surface, try to get a fix and transmit it
            rfOn();
            xbTx(); // do this once per minute only to conserve energy
        } else if (pressureSensorData[1] >
                   d_press_2) { // still under or resubmerged
            rfOff();
        }

        break;

    case (ST_RETRIEVE):
        //  Waiting to be retrieved. Transmit GPS coordinates periodically

        //	printf("State RETRIEVE - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        if (batteryData[0] + batteryData[1] < d_volt_2) { // critical battery
            nextState = ST_SHUTDOWN;
        }

        else if (batteryData[0] + batteryData[1] < d_volt_1) { // low battery
            burnwireOn(); // redundant, is already on
            rfOn();
            xbTx();
            nextState = ST_RETRIEVE; // dwell in this state with burnwire left
                                     // on
        }

        else
            nextState = ST_RETRIEVE; // default guard

        break;

    case (ST_SHUTDOWN):
        //  Shut everything off in an orderly way if battery is critical to
        //  reduce file system corruption risk
        burnwireOff();
        system("sudo halt"); //
        break;

    default:
        nextState = presentState; // something is wrong if the program gets here
    }
    return (nextState);
}

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

int burnwireOn(void) {
    int fd;
    int result;
    // Open a connection to the io expander on the power board
    if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_PWRBD, 0)) < 0) {
        CETI_LOG("Failed to open I2C connection for IO Expander on the Power Board");
        return -1;
    }
    // to turn it on, set bit 0 and bit 1 both low
    result = i2cReadByteData(fd, IOX_OUTPUT);
    result = result & ~(BW_RST | nBW_ON);
    CETI_LOG("Turn BW ON writing %02X to the IOX", result);
    i2cWriteByteData(fd, IOX_OUTPUT, result);
    i2cClose(fd);
    return 0;
}

int burnwireOff(void) {
    int fd, result;
    // Open a connection to the io expander on the power board
    if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_PWRBD, 0)) < 0) {
        CETI_LOG("Failed to open I2C connection for IO Expander on the Power Board");
        return -1;
    }
    // to turn it off, set bit1 high
    result = i2cReadByteData(fd, IOX_OUTPUT);
    result = result | BW_RST;
    CETI_LOG("Turn BW OFF writing %02X to the IOX", result);
    i2cWriteByteData(fd, IOX_OUTPUT, result);
    i2cClose(fd);
    return 0;
}

int rfOn(void) {

    int fd;
    int result;

    CETI_LOG("rfOn() - Turn on power for GPS and XBee modules");
    // Open a connection to the io expander on the power board
    if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_SNSBD, 0)) < 0) {
        CETI_LOG("Failed to open I2C connection for IO Expander on the Sensor Board");
        return -1;
    }
    // Start with the output low before enabling, otherwise surge is excessive.
    // This is a workaround
    result = i2cReadByteData(fd, IOX_OUTPUT);
    result = result & ~RF_ON;
    i2cWriteByteData(fd, IOX_OUTPUT, result);

    // Now, configure the pin as an output
    result = i2cReadByteData(fd, IOX_CONFIG);
    result = result & ~RF_ON;
    i2cWriteByteData(fd, IOX_CONFIG, result);

    // Now, turn it on
    result = i2cReadByteData(fd, IOX_OUTPUT);
    result = result | RF_ON;
    i2cWriteByteData(fd, IOX_OUTPUT, result);

    i2cClose(fd);
    gpsPowerState = 1;
    return 0;
}

int rfOff(void) {

    int fd;
    int result;

    // Open a connection to the io expander on the power board
    if ((fd = i2cOpen(1, ADDR_IO_EXPANDER_SNSBD, 0)) < 0) {
        CETI_LOG("Failed to open I2C connection for IO Expander on the Power Board");
        return -1;
    }
    result = i2cReadByteData(fd, IOX_OUTPUT);
    result = result & ~RF_ON;
    i2cWriteByteData(fd, IOX_OUTPUT, result);

    i2cClose(fd);
    gpsPowerState = 0;

    return 0;
}

int xbTx(void) {

    int fd;
    static int i;

    if (i < 2) {
        i++;
        return (0);
    }

    fd = serOpen("/dev/serial0", 9600, 0);

    if (fd < 0) {
        CETI_LOG("xbTx(): Failed to open the serial port");
        return (-1);
    }

    // printf("xbTx(): Writing GPS sentence to XBee\n");
    serWrite(fd, gpsLocation, strlen(gpsLocation));
    serWrite(fd, "\n", 1);

    i = 0;
    serClose(fd);
    return (0);
}
