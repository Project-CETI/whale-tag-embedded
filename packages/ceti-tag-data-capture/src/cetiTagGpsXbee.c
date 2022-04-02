//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    	Description				Author
//  0.00    12/23/21   Begin integration  	  Cummings
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagGpsXbee.c
// Description: GPS and Xbee data link functions
//-----------------------------------------------------------------------------

#include "cetiTagGpsXbee.h"

// demo
int demoGPS(void) {

    int fd = serOpen("/dev/serial0", 9600, 0);
    if (fd < 0) {
        CETI_LOG("demoGPS(): Failed to open the serial port");
        return (-1);
    }

    printf("demoGPS(): Successfully opened the serial port\n");
    CETI_LOG("demoGPS(): Successfully opened the serial port");
    while (1) {
        usleep(1000000);
        printf("Trying to write to the serial port with pigpio\n");
        CETI_LOG("Trying to write to the serial port with pigpio");
        serWrite(fd, "Test Tx\n", 8);
    }

    return (0);
}