#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <stdio.h>
#include "i2c.h"

int i2c_open(unsigned char bus, unsigned char address) {
    if (bus < 0 && bus > 1) {
        return -1;
    }
    char filename[11] = "/dev/i2c-0";
    if (bus == 1) {
        filename[9] = '1';
    }
    
    int i2c_handle = open(filename, O_RDWR);
    if (i2c_handle < 0) {
        return -1;
    }
    if (ioctl(i2c_handle, I2C_SLAVE, address) < 0 ) {
        close(i2c_handle);
        return -1;
    }
    return i2c_handle;
}

void i2c_close(int i2c_handle) {
    if (i2c_handle >= 0) {
        close(i2c_handle);
    }
}

int i2c_read(int i2c_handle, unsigned char address, unsigned char reg, unsigned char data) {
    unsigned char outbuf[1], inbuf[1];
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data msgset[1];

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = outbuf;

    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD | I2C_M_NOSTART;
    msgs[1].len = 1;
    msgs[1].buf = inbuf;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 2;

    outbuf[0] = reg;
    inbuf[0] = 0;

    if (ioctl(i2c_handle, I2C_RDWR, &msgset) < 0) {
        return -1;
    }

    return inbuf[0];
}

int i2c_write(int i2c_handle, unsigned char address, unsigned char reg, unsigned char data) {

    unsigned char outbuf[2];

    struct i2c_msg msgs[1];
    struct i2c_rdwr_ioctl_data msgset[1];

    outbuf[0] = reg;
    outbuf[1] = data;

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = 2;
    msgs[0].buf = outbuf;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 1;

    if (ioctl(i2c_handle, I2C_RDWR, &msgset) < 0) {
        return -1;
    }

    return 0;
}
