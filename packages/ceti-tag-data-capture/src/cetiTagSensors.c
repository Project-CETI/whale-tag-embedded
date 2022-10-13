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
#include <unistd.h>

#include "cetiTagSensors.h"
#include "cetiTagWrapper.h"

static int presentState = ST_CONFIG;

int16_t quaternion[4];
double pressureSensorData[2];
double batteryData[3];
unsigned int rtcCount; 
int boardTemp, ambientLight;
int gpsPowerState = 0;


#define GPS_LOCATION_LENGTH (1024)
char gpsLocation[GPS_LOCATION_LENGTH];

const char * get_state_str(wt_state_t state) {
    if ( (state < ST_CONFIG) || (state > ST_UNKNOWN) ) {
        CETI_LOG("get_state_str(): presentState is out of bounds. Setting to ST_UNKNOWN. Current value: %d", presentState);
        state = ST_UNKNOWN;
    }
    return state_str[state];
}

//-----------------------------------------------------------------------------
// Control and Monitor Thread
//
//  - Periodically get data from non-audio sensors and save to .csv
//  - Keep track of the operational state of the tag
//-----------------------------------------------------------------------------
void *sensorThread(void *paramPtr) {
    FILE *snsData = NULL; // data file for non-audio sensor data

    char header[250] = "Timestamp(ms)";
    #if USE_RTC
    strcat(header, ", RTC count");
    #endif
    strcat(header, ", State");
    #if USE_BOARD_TEMPERATURE_SENSOR
    strcat(header, ", BoardTemp degC");
    #endif
    #if USE_PRESSURE_SENSOR
    strcat(header, ", WaterTemp degC");
    strcat(header, ", Pressure bar");
    #endif
    #if USE_BATTERY_GAUGE
    strcat(header, ", Batt_V1 V");
    strcat(header, ", Batt_V2 V");
    strcat(header, ", Batt_I mA");
    #endif
    #if USE_IMU
    strcat(header, ", Quat_i");
    strcat(header, ", Quat_j");
    strcat(header, ", Quat_k");
    strcat(header, ", Quat_Re");
    #endif
    #if USE_LIGHT_SENSOR
    strcat(header, ", AmbientLight");
    #endif
    #if USE_GPS
    strcat(header, ", GPS");
    #endif
    strcat(header, "\n");

    struct timeval te;
    long long milliseconds;
    int fd_access = 0;

    while (!g_exit) {
        gettimeofday(&te, NULL);
        milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;

        fd_access = access(SNS_FILE, F_OK);

        snsData = fopen(SNS_FILE, "at");
        if (snsData == NULL) {
            CETI_LOG("sensorThread(): failed to write to file: %s", SNS_FILE);
            return NULL;
        }

        if (fd_access == -1) {
            CETI_LOG("sensorThread(): could not find sensor file, creating a new one: %s", SNS_FILE);
            fprintf(snsData, "%s", header);
        }

        fprintf(snsData, "%lld,", milliseconds);
        #if USE_RTC
        getRtcCount(&rtcCount);
        fprintf(snsData, "%d,", rtcCount);
        #endif
        fprintf(snsData, "%s,", get_state_str(presentState));
        #if USE_BOARD_TEMPERATURE_SENSOR
        getBoardTemp(&boardTemp);
        fprintf(snsData, "%d,", boardTemp);
        #endif
        #if USE_PRESSURE_SENSOR
        getTempPsns(pressureSensorData);
        fprintf(snsData, "%.2f,", pressureSensorData[0]);
        fprintf(snsData, "%.2f,", pressureSensorData[1]);
        #endif
        #if USE_BATTERY_GAUGE
        getBattStatus(batteryData);
        fprintf(snsData, "%.2f,", batteryData[0]);
        fprintf(snsData, "%.2f,", batteryData[1]);
        fprintf(snsData, "%.2f,", batteryData[2]);
        #endif
        #if USE_IMU
        getQuaternion(quaternion);
        fprintf(snsData, "%d,", quaternion[0]);
        fprintf(snsData, "%d,", quaternion[1]);
        fprintf(snsData, "%d,", quaternion[2]);
        fprintf(snsData, "%d,", quaternion[3]);
        #endif
        #if USE_LIGHT_SENSOR
        getAmbientLight(&ambientLight);
        fprintf(snsData, "%d,", ambientLight);
        #endif
        #if USE_GPS
        getGpsLocation(gpsLocation);
        fprintf(snsData, "\"%s\"", gpsLocation);
        #endif

        fprintf(snsData, "\n");
        fclose(snsData);
        updateState();
        usleep(SNS_SMPL_PERIOD);
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// Recovery Board
//-----------------------------------------------------------------------------


int getGpsLocation(char *gpsLocation) {

//    The Recovery Board sends the GPS location sentence as an ASCII string
//    about once per second automatically via the serial
//    port. This function monitors the serial port and extracts the sentence for 
//    reporting in the .csv record. 
//
//    The Recovery Board may not be on at all times. Depending on the state
//    of the tag, it may be turned off to save power. See the state machine
//    function for current design. If the recovery board is off, the gps field
//    in the .csv file will indicate "No GPS update available"
//    
//    The messages coming in from the Recovery Board are asynchronous relative to 
//    the timing of execution of this function. So, partial messages will occur
//    if they are provided at a rate less than 2x the frequency of this
//    routine  - see SNS_SMPL_PERIOD which is nominally 1 second per cycle.  

    static int fd=PI_INIT_FAILED;  //handle for serial port. The port is left open for the life of the app
    static char buf[GPS_LOCATION_LENGTH];   //working buffer for serial data
    static char *pTempWr=buf;     //these need to be static in case of a partial message   
    static int bytes_total=0;     

    char *msg_start=NULL, *msg_end=NULL;  
    int bytes_avail,i,complete=0,pending=0;

    // Open the serial port using the pigpio lib

    if(fd <= PI_INIT_FAILED) {
        fd = serOpen("/dev/serial0",115200,0);
        if (fd < 0) {
            CETI_LOG("getGpsLocation(): Failed to open the serial port");
            return (-1);
        }
        CETI_LOG("getGpsLocation(): Successfuly opened the serial port");
    }

    // Check if any bytes are waiting in the UART buffer and store them temporarily.
    // It is possible to receive a partial message - no synch or handshaking in protocol

    bytes_avail = serDataAvailable(fd);
    bytes_total += bytes_avail; //keep a running count

    if (bytes_avail && (bytes_total < GPS_LOCATION_LENGTH) ) {
        serRead(fd,pTempWr,bytes_avail);   //add the new bytes to the buffer
        pTempWr = pTempWr + bytes_avail;   //advance the write pointer
    } else { //defensive - something went wrong, reset the buffer
        memset(buf,0,GPS_LOCATION_LENGTH);         // reset entire buffer conents
        bytes_total = 0;                   // reset the static byte counter
        pTempWr = buf;                     // reset the static write pointer
    }

    // Scan the whole buffer for a GPS message - delimited by 'g' and '\n'
    for (i=0; (i<=bytes_total && !complete); i++) {
        if( buf[i] == 'g' && !pending) {
                msg_start =  buf + i;
                pending = 1;
        }

        if ( buf[i] == '\n' && pending) {
                msg_end = buf + i - 1; //don't include the '/n'
                complete = 1;
        }  
    }

    if (complete) { //copy the message from the buffer
        memset(gpsLocation,0,GPS_LOCATION_LENGTH);
        strncpy(gpsLocation,msg_start+1,(msg_end-buf));
        memset(buf,0,GPS_LOCATION_LENGTH);             // reset buffer conents
        bytes_total = 0;                        // reset the static byte counter                   
        pTempWr = buf;                          // reset the static write pointer
        gpsLocation[strcspn(gpsLocation, "\r\n")] = 0;
    } else {  //No complete sentence was available on this cycle, try again next time through
        strcpy(gpsLocation,"No GPS update available");
    }
    
    return (0);
}

//-----------------------------------------------------------------------------
// BNO080  IMU
//-----------------------------------------------------------------------------

int getQuaternion(short *quaternion) {

    int numBytesAvail;
    char pktBuff[256] = {0};
    char shtpHeader[4] = {0};
    char commandBuffer[10] = {0};
    int retval;

    // parse the IMU quaternion vector input report

    if ((bbI2COpen(BB_I2C_SDA, BB_I2C_SCL, 200000)) < 0) {
        CETI_LOG("getQuaternion(): Failed to connect to the IMU");
        return -1;
    }

    // Byte   0    1    2    3    4   5   6    7    8      9     10     11
    //       |     HEADER      |            TIME       |  ID    SEQ
    //       STATUS....

    commandBuffer[0] = 0x04; // set address
    commandBuffer[1] = ADDR_IMU;
    commandBuffer[2] = 0x02; // start
    commandBuffer[3] = 0x01; // escape
    commandBuffer[4] = 0x06; // read
    commandBuffer[5] = 0x04; // #bytes lsb
    commandBuffer[6] = 0x00; // #bytes msb
    commandBuffer[7] = 0x03; // stop
    commandBuffer[8] = 0x00; // end

    retval = bbI2CZip(BB_I2C_SDA, commandBuffer, 9, shtpHeader, 4);
    // msb is "continuation bit, not part of count"
    numBytesAvail = MIN(256, ((shtpHeader[1] << 8) | shtpHeader[0]) & 0x7FFF);

    if (retval > 0 && numBytesAvail) {
        commandBuffer[5] = numBytesAvail & 0xff;
        commandBuffer[6] = (numBytesAvail >> 8) & 0xff;
        bbI2CZip(BB_I2C_SDA, commandBuffer, 9, pktBuff, numBytesAvail);
        if (retval > 0 && pktBuff[2] == 0x03) { // make sure we have the right channel
            quaternion[0] = (pktBuff[14] << 8) + pktBuff[13];
            quaternion[1] = (pktBuff[16] << 8) + pktBuff[15];
            quaternion[2] = (pktBuff[18] << 8) + pktBuff[17];
            quaternion[3] = (pktBuff[20] << 8) + pktBuff[19];
        }
        bbI2CClose(BB_I2C_SDA);
    } else {
        bbI2CClose(BB_I2C_SDA);
        setupIMU();
    }

    return (0);
}

//-----------------------------------------------------------------------------
// Battery gas gauge
//-----------------------------------------------------------------------------

int getBattStatus(double *batteryData) {

    int fd, result;
    signed short voltage, current;

    if ((fd = i2cOpen(1, ADDR_GAS_GAUGE, 0)) < 0) {
        CETI_LOG("getBattStatus(): Failed to connect to the fuel gauge");
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
// Ambient light sensor LiteON LTR-329ALS-01
//-----------------------------------------------------------------------------

int getAmbientLight(int *pAmbientLight) {

    int fd;

    if ( (fd=i2cOpen(1,ADDR_LIGHT,0) ) < 0 ) {
        CETI_LOG("getAmbientLight(): Failed to connect to the light sensor\n");
        return (-1);
    }

    // need to read both wavelengths
    *pAmbientLight = i2cReadWordData(fd,0x88);  //visible
    i2cReadWordData(fd,0x8A);  //infrared, not used
    i2cClose(fd);
    return (0);
}

//-----------------------------------------------------------------------------
// Board mounted temperature sensor SA56004
//-----------------------------------------------------------------------------

int getBoardTemp(int *pBoardTemp) {

    int fd;
    if ( (fd=i2cOpen(1,ADDR_TEMP,0) ) < 0 ) {
        CETI_LOG("getBoardTemp(): Failed to connect to the temperature sensor\n");
        return(-1);
    }

    *pBoardTemp = i2cReadByteData(fd,0x00);
    i2cClose(fd);
    return(0);
}

//-----------------------------------------------------------------------------
// State Machine and Controls
// * Details of state machine are documented in the high-level design
//-----------------------------------------------------------------------------

int updateState() {

    // Config file and associated parameters
    static FILE *cetiConfig = NULL;
    char cPress_1[16], cPress_2[16], cVolt_1[16], cVolt_2[16], cTimeOut[16];
    static double d_press_1, d_press_2, d_volt_1, d_volt_2;
    static int timeOut_minutes, timeout_seconds;
    char line[256];
    char *pTemp;

    // timing
    static unsigned int startTime = 0;
    // static int burnTimeStart;

    // Deployment sequencer FSM

    switch (presentState) {

    case (ST_CONFIG):
        // Load the deployment configuration
        CETI_LOG("updateState(): Configuring the deployment parameters");
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

        presentState = ST_START;
        break;

    case (ST_START):
        // Start recording
        #if USE_AUDIO
        start_audio_acq();
        #endif
        startTime = getTimeDeploy(); // new v0.5 gets start time from the csv
        CETI_LOG("updateState(): Deploy Start: %u", startTime);
        rcvryOn();                // turn on Recovery Board
        presentState = ST_DEPLOY;    // underway!
        break;

    case (ST_DEPLOY):

        // Waiting for 1st dive
        //	printf("State DEPLOY - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        #if USE_BATTERY_GAUGE
        if ((batteryData[0] + batteryData[1] < d_volt_1) ||
            (rtcCount - startTime > timeout_seconds)) {
            burnwireOn();
            presentState = ST_BRN_ON;
            break;
        }
        #endif

        #if USE_PRESSURE_SENSOR
        if (pressureSensorData[1] > d_press_2)
            presentState = ST_REC_SUB; // 1st dive after deploy
        #endif

        break;

    case (ST_REC_SUB):
        // Recording while sumberged
        //	printf("State REC_SUB - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        #if USE_BATTERY_GAUGE
        if ((batteryData[0] + batteryData[1] < d_volt_1) ||
            (rtcCount - startTime > timeout_seconds)) {
            // burnTimeStart = rtcCount ;
            burnwireOn();
            presentState = ST_BRN_ON;
            break;
        }
        #endif

        #if USE_PRESSURE_SENSOR
        if (pressureSensorData[1] < d_press_1) {
            presentState = ST_REC_SURF; // came to surface
            break;
        }
        #endif

        rcvryOff();
        break;

    case (ST_REC_SURF):
        // Recording while at surface, trying to get a GPS fix
        //	printf("State REC_SURF - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //batteryData[1]));

        #if USE_BATTERY_GAUGE
        if ((batteryData[0] + batteryData[1] < d_volt_1) ||
            (rtcCount - startTime > timeout_seconds)) {
            //	burnTimeStart = rtcCount ;
            burnwireOn();
            presentState = ST_BRN_ON;
            break;
        }
        #endif

        #if USE_PRESSURE_SENSOR
        if (pressureSensorData[1] > d_press_2) {
            rcvryOff();
            presentState = ST_REC_SUB; // back under....
            break;
        }

        if (pressureSensorData[1] < d_press_1) {
            rcvryOn();
        }
        #endif

        break;

    case (ST_BRN_ON):
        // Releasing
        //	printf("State BRN_ON - Deployment elapsed time is %d seconds;
        //  Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //  [1]));

        #if USE_BATTERY_GAUGE
        if (batteryData[0] + batteryData[1] < d_volt_2) {
            presentState = ST_SHUTDOWN; // critical battery
            break;
        }

        if (batteryData[0] + batteryData[1] < d_volt_1) {
            presentState = ST_RETRIEVE; // low battery
            break;
        }

        // update 220109 to leave burnwire on without time limit
        // Leave burn wire on for 45 minutes or until the battery is depleted
        // else if (rtcCount - burnTimeStart > (60*45)) {
        //	//burnwireOff();  //leaving it on no time limit -change 220109
        //	nextState = ST_RETRIEVE;
        //}
        #endif

        #if USE_PRESSURE_SENSOR
        // at surface, turn on the Recovery Board
        if (pressureSensorData[1] < d_press_1) {
            rcvryOn();
        }

        // still under or resubmerged
        if (pressureSensorData[1] > d_press_2) {
            rcvryOff();
        }

        #endif

        break;

    case (ST_RETRIEVE):
        //  Waiting to be retrieved.

        //	printf("State RETRIEVE - Deployment elapsed time is %d seconds;
        //  Battery at %.2f \n", (rtcCount - startTime), (batteryData[0] +
        //  batteryData[1]));

        #if USE_BATTERY_GAUGE
        // critical battery
        if (batteryData[0] + batteryData[1] < d_volt_2) {
            presentState = ST_SHUTDOWN;
            break;
        }

        // low battery
        if (batteryData[0] + batteryData[1] < d_volt_1) {
            burnwireOn(); // redundant, is already on
            rcvryOn();
        }
        #endif

        break;

    case (ST_SHUTDOWN):
        //  Shut everything off in an orderly way if battery is critical to
        //  reduce file system corruption risk
        burnwireOff();
        rcvryOff();
        CETI_LOG("updateState(): Battery critical, halting");
        system("systemctl poweroff");
        break;
    }
    return (0);
}

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

int burnwireOn(void) {

    int fd, result;

    // Open a connection to the io expander
    if ( (fd = i2cOpen(1,ADDR_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOn(): Failed to open I2C connection for IO Expander \n");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result & (~BW_nON & ~BW_RST); 
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}

int burnwireOff(void) {

    int fd, result;

    // Open a connection to the io expander
    if ( (fd = i2cOpen(1,ADDR_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOff(): Failed to open I2C connection for IO Expander \n");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result | (BW_nON | BW_RST); 
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}

int rcvryOn(void) {

    int fd, result;

    if ( (fd = i2cOpen(1,ADDR_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOn(): Failed to open I2C connection for IO Expander \n");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result & (~RCVRY_RP_nEN & ~nRCVRY_SWARM_nEN & ~nRCVRY_VHF_nEN); 
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}

int rcvryOff(void) {

    int fd, result;
    
    if ( (fd = i2cOpen(1,ADDR_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOn(): Failed to open I2C connection for IO Expander \n");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result | (RCVRY_RP_nEN | nRCVRY_SWARM_nEN | nRCVRY_VHF_nEN);  
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}


