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
int demoGPS(void)
{

	//char buf[4096]; 
	char testbuf[16]= "Test Tx\n";

	int fd;
	//int bytes_avail;
	//int i;

	fd = serOpen("/dev/serial0",9600,0);

	if(fd < 0) {
		printf ("demoGPS(): Failed to open the serial port\n");
		return (-1);
	}
	else {
		printf("demoGPS(): Successfully opened the serial port\n");
	}

	while(1) {
		usleep (1000000);
		
		//bytes_avail = (serDataAvailable(fd));		
		//printf("%d bytes are available from the serial port\n",bytes_avail);		
		//serRead(fd,buf,bytes_avail);
		//printf("%s",buf);

		printf("Trying to write to the serial port with pigpio\n");
		serWrite(fd,testbuf,8); //test transmission

	}


	return (0);
}