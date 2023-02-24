#include <linux/i2c-dev.h>
#include <fcntl.h>    /* For O_RDWR */
#include <unistd.h>   /* For open(), */
#include <sys/ioctl.h>

int i2c_open(unsigned char bus, unsigned char address);
void i2c_close(int i2c_handle);

unsigned char i2c_read(int i2c_handle, unsigned char address, unsigned char reg, unsigned char data);
int i2c_write(int i2c_handle, unsigned char address, unsigned char reg, unsigned char data);
