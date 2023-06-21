//-----------------------------------------------------------------------------
// Command interpreter and handler
//-----------------------------------------------------------------------------

#include <pigpio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pressureLogger.h"


int hdlCmd(void) {

double pressureSensorData[2];

    //-----------------------------------------------------------------------------
    // Part 1 - quit or any other special commands here
    if (!strncmp(g_command, "quit", 4)) {
        printf("Quitting\n");
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "Stopping the App\n"); // echo it back
        fclose(g_rsp);
        g_exit = 1;
        return 0;
    }
    //-----------------------------------------------------------------------------
    // Part 2 - Client commands
    if (!strncmp(g_command, "debug", 5)) {
        printf("Debug Placeholder is Executing\n");
        g_rsp = fopen(RSP, "w");
        printf("Debug Placeholder is Finished\n");
        fprintf(g_rsp, "Running Debug Routine(s)\n"); // echo it back
        fclose(g_rsp);
        return 0;
    }  

    if (!strncmp(g_command, "rd_0", 4)) {
        printf("Reading Pressure Sensor on i2c-0\n");
        g_rsp = fopen(RSP, "w");
        
        if (!getTempPsns_0(pressureSensorData)) {
            fprintf(g_rsp, "Temp %.2f deg. C\n", pressureSensorData[0]);
            fprintf(g_rsp,  "Pressure %.2f bar \n", pressureSensorData[1]); 
        }

        else fprintf(g_rsp, "Sensor read failed on i2c bus 0, check power and connections\n");

        fclose(g_rsp);
        return 0;
    }

    if (!strncmp(g_command, "rd_1", 4)) {
        printf("Reading Pressure Sensor on i2c-1\n");
        g_rsp = fopen(RSP, "w");
        if (!getTempPsns_1(pressureSensorData)) {

            fprintf(g_rsp, "Temp %.2f deg. C\n", pressureSensorData[0]);
            fprintf(g_rsp,  "Pressure %.2f bar \n", pressureSensorData[1]);
        }

    else fprintf(g_rsp, "Sensor read failed on i2c bus 1, check power and connections\n");

        fclose(g_rsp);

        return 0;
    }  

    else {
        g_rsp = fopen(RSP, "w");
        fprintf(g_rsp, "\n"); // echo it
        fprintf(g_rsp, "CETI Tag Electronics Available Commands\n");
        fprintf(g_rsp,
                "---------------------------------------------------------\n");
        fprintf(g_rsp, "quit        Exit the application gracefully\n");

        fprintf(g_rsp, "rd_0        Read Keller sensor connected to i2c0\n");
        fprintf(g_rsp, "rd_1        Read Keller sensor connected to i2c1\n");

        fprintf(g_rsp, "debug       Dummy function for debugging\n");       

        fprintf(g_rsp, "\n");
        fclose(g_rsp);
        return 0;
    }
    return 1;
}

//-----------------------------------------------------------------------------
// Keller pressure sensors
//-----------------------------------------------------------------------------

int getTempPsns_0 (double * presSensorData) {

    int fd;
    short int temperature,pressure;
    char presSensData_byte[5];
            
    if ( (fd=i2cOpen(0,ADDR_DEPTH,0) ) < 0 ) {
        printf("getTempPsns_0(): Failed to connect to the depth sensor on i2c0\n");
        return (-1);
    }

    else {  

        printf("Read Pressure Sensor on i2c-0\n");

        if ( i2cWriteByte(fd,0xAC)   != 0 ) { //measurement request from the device
            printf("getTempPsns_0(): Failed to write to the depth sensor on i2c0\n");
            return (-1);
        }

        else {
        
            usleep(10000); //wait 10 ms for the measurement to finish

            i2cReadDevice(fd,presSensData_byte,5);  //read the measurement

            temperature = presSensData_byte[3] << 8; 
            temperature = temperature + presSensData_byte[4];
            presSensorData[0] = ((temperature >> 4) - 24 ) * .05 - 50;  //convert to deg C
            // convert to bar - see Keller data sheet for the particular sensor in use
            pressure = presSensData_byte[1] << 8;
            pressure = pressure + presSensData_byte[2];
            presSensorData[1] = ( (PMAX-PMIN)/32768.0 ) * (pressure - 16384.0)  ; //convert to bar

        }

        i2cClose(fd);
        return(0);
    }
}

int getTempPsns_1 (double * presSensorData) {

    int fd;
    short int temperature,pressure;
    char presSensData_byte[5];
            
    if ( (fd=i2cOpen(1,ADDR_DEPTH,0) ) < 0 ) {
        printf("getTempPsns_1(): Failed to connect to the depth sensor on i2c1\n");
        return (-1);
    }

    else {  

        printf("Read Pressure Sensor on i2c-1\n");

        if ( i2cWriteByte(fd,0xAC)  != 0 )  { //measurement request from the device
            printf("getTempPsns_0(): Failed to write to the depth sensor on i2c0\n");
            return (-1);
        }

        else {

            usleep(10000); //wait 10 ms for the measurement to finish

            i2cReadDevice(fd,presSensData_byte,5);  //read the measurement

            temperature = presSensData_byte[3] << 8; 
            temperature = temperature + presSensData_byte[4];
            presSensorData[0] = ((temperature >> 4) - 24 ) * .05 - 50;  //convert to deg C
            // convert to bar - see Keller data sheet for the particular sensor in use
            pressure = presSensData_byte[1] << 8;
            pressure = pressure + presSensData_byte[2];
            presSensorData[1] = ( (PMAX-PMIN)/32768.0 ) * (pressure - 16384.0)  ; //convert to bar

        }

        i2cClose(fd);
        return(0);
    }
}
