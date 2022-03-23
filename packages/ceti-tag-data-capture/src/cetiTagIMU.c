//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Version    Date    	Description				Author
//  0.00    12/02/21   Start work 		 	  Cummings
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagIMU.c
// Description: BNO08x IMU routines
//-----------------------------------------------------------------------------
//
// The BNO08x uses the  CEVA/Hillcrest Labs Sensor Hub SH-2 software to exchange information with a host. 
// The SH-2 software is designed to work with a range of physical sensors and produce data from
// virtual sensors that offer a variety of measurment capabilities while sharing
// a common host interface architecture. 

// SH-2 is a generalized program in the sense that it can work with a variety of 
// compatinle CEVA/Hillcrest products. For CETI, we are only concerned with 
// features supported by BNO08x IMU.
//
// For the CETI Tag, the I2C interface is used to exchange packets to/from the host, which
// is a Raspberry Pi Zero. 
//
// To understand how SH-2 works with the BNO08x IMUs, there are three key documents:
//   1) The BNO080x Data Sheet
//   2) CEVA/Hillcrest Labs 1000-3535  Sensor Hub Transport Protocol - describes how data cargo is arranged for
//      transport without getting into the actual meaning of the data.
//   3) CEVA/Hillcrest Labs 1000-3625 SH-2 Reference Manual - describes the details/meaning of the data itself,
//      Some of this is also available in the BNO08x data sheet as well.
//  

#include "cetiTagIMU.h"

// Global Variables
char shtpHeader[4];
char shtpData[256];

u_int16_t	numBytesAvail;

//-----------------------------------------------------------------------------
// This is a sandbox function to learn how to get data from the device
// Very preliminary - not using HINT or RESET features at all. No error checking

int learnIMU()  
{
	int fd;
	if ( (fd = i2cOpen(1,ADDR_IMU,0)) < 0 ) {
		printf("learnIMU(): Failed to connect to the IMU\n");
		return -1;
	}
	else printf("OK, have the IMU connected!\n");

	// Get the shtp header
	i2cReadDevice(fd,shtpHeader,4);  //pigpio i2c read raw
	printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n", shtpHeader[0],shtpHeader[1],shtpHeader[2],shtpHeader[3]);

	// Figure out how many bytes are waiting
	numBytesAvail = (shtpHeader[1] << 8 | shtpHeader[0]);
	printf("learnIMU(): Bytes Avail are 0x%04X \n", numBytesAvail);

	// Get all bytes that are waiting (flush it out)
	while (numBytesAvail > 0) {
			i2cReadDevice(fd,shtpData,numBytesAvail); 		
			i2cReadDevice(fd,shtpHeader,4);  
			printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n", shtpHeader[0],shtpHeader[1],shtpHeader[2],shtpHeader[3]);	
	}

	// Ask for product ID report	
	shtpData[0]=0x06;   // header - packet length LSB
	shtpData[1]=0x00;   // header - packet length MSB
	shtpData[2]=0x02;   // header - command channel
	shtpData[3]=0x00; 	// header - sequence no
	shtpData[4]=SHTP_REPORT_PRODUCT_ID_REQUEST;   
	shtpData[5]=0x00;   // reserved

	i2cWriteDevice(fd,shtpData,6);
	
	i2cReadDevice(fd,shtpHeader,4);  
	printf("learnIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n", shtpHeader[0],shtpHeader[1],shtpHeader[2],shtpHeader[3]);

	i2cClose(fd);

	return 0;
}

int setupIMU()  // This enables just the rotation vector feature
{
	
	char setFeatureCommand[21];
	int i;
	int fd;

	if ( (fd = i2cOpen(1,ADDR_IMU,0)) < 0 ) {
	printf("setupIMU(): Failed to connect to the IMU\n");
	return -1;
	}
	else printf("setupIMU(): IMU connection opened\n");

	// Build the control packet
	for(i=0;i<21;i++) setFeatureCommand[i]=0;
	setFeatureCommand[0]=0x15;  		//Packet length LSB (21 bytes)
	setFeatureCommand[2]=CHANNEL_CONTROL;  	
	setFeatureCommand[4]=SHTP_REPORT_SET_FEATURE_COMMAND;
	setFeatureCommand[5]=SENSOR_REPORTID_ROTATION_VECTOR;  

	setFeatureCommand[9]=0x20;   //Report interval LS (microseconds)
	setFeatureCommand[10]=0xA1;  //0x0007A120 is 0.5 seconds
	setFeatureCommand[11]=0x07;  
	setFeatureCommand[12]=0x00;  //Report interval MS

	i2cWriteDevice(fd,setFeatureCommand,21);	
	i2cReadDevice(fd,shtpHeader,4);  
	printf("setupIMU(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n", shtpHeader[0],shtpHeader[1],shtpHeader[2],shtpHeader[3]);
	i2cClose(fd);

	return 0;
}


int getRotation(rotation_t *pRotation)
{

	// int i=0;
	//int fifo_wr_count=0;
	//int fifo_rd_count;
	int fd;
	int numBytesAvail;
	char pktBuff[256]; 
	//char buff[256]; 
	char shtpHeader[4];

// parse the IMU rotation vector input report

	if ( (fd = i2cOpen(1,ADDR_IMU,0)) < 0 ) {
	printf("getRotation(): Failed to connect to the IMU\n");
	return -1;
	}
	else printf("getRotation(): IMU connection opened\n");
	
    // Byte   0    1    2    3    4   5   6    7    8      9     10     11
    //       |     HEADER      |            TIME       |  ID    SEQ   STATUS....

	while(1) {

		i2cReadDevice(fd,shtpHeader,4);  

		//printf("getRotation(): Header is 0x%02X  0x%02X  0x%02X  0x%02X \n", shtpHeader[0],shtpHeader[1],shtpHeader[2],shtpHeader[3]);
		
		numBytesAvail = ( (shtpHeader[1] << 8 ) | shtpHeader[0]) & 0x7FFF; //msb is "continuation bit, not part of count"
		if (numBytesAvail > 256) numBytesAvail = 256;

		if (numBytesAvail == 0) setupIMU(); //restart the sensor, reports somehow stopped coming
	
		if(numBytesAvail) { 
			i2cReadDevice(fd,pktBuff,numBytesAvail); 
			if (pktBuff[2] == 0x03) {             //make sure we have the right channel
				pRotation->reportID = pktBuff[9];     //testing, should come back 0x05 
				pRotation->sequenceNum = pktBuff[10];
				printf("getRotation(): report ID 0x%02X  sequ 0x%02X i %02X%02X j %02X%02X k %02X%02X r %02X%02X\n", 
					pktBuff[9],pktBuff[10],
					pktBuff[14],pktBuff[13],pktBuff[16],pktBuff[15],pktBuff[18],pktBuff[17],pktBuff[20],pktBuff[19] ); 
			}
		}
		usleep(800000);
	}

	i2cClose(fd);

return 0;

}
