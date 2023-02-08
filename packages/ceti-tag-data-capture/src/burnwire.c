//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "burnwire.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
int init_burnwire() {

  if(burnwireOff() < 0) {
    CETI_LOG("init_burnwire(): XXXX Failed to turn off the burnwire XXXX");
    return (-1);
  }
  CETI_LOG("init_burnwire(): Successfully initialized the burnwire");
  return 0;
}

//-----------------------------------------------------------------------------
// Burnwire interface
//-----------------------------------------------------------------------------

int burnwireOn(void) {

    int fd, result;

    // Open a connection to the io expander
    if ( (fd = i2cOpen(1,ADDR_MAINTAG_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOn(): XXXX Failed to open I2C connection for IO Expander XXXX");
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
    if ( (fd = i2cOpen(1,ADDR_MAINTAG_IOX,0)) < 0 ) {
        CETI_LOG("burnwireOff(): XXXX Failed to open I2C connection for IO Expander XXXX");
        return -1;
    }
    result = i2cReadByte(fd);
    result = result | (BW_nON | BW_RST);
    i2cWriteByte(fd,result);
    i2cClose(fd);
    return 0;
}

