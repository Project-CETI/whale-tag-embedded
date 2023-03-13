#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "i2c.h"
#include "gpio.h"

#define ADDR_GAS_GAUGE 0x59
// DS2778 Gas Gauge Registers and Settings
#define PROTECT 0x00
#define BATT_CTL 0x60  
#define OVER_VOLTAGE 0x7F
#define CELL_1_V_MS 0x0C
#define CELL_1_V_LS 0x0D
#define CELL_2_V_MS 0x1C
#define CELL_2_V_LS 0x1D
#define BATT_I_MS 0x0E
#define BATT_I_LS 0x0F
#define R_SENSE 0.025

#define BATT_CTL_VAL 0X8E  //SETS UV CUTOFF TO 2.6V
#define OV_VAL 0x5A //SETS OV CUTOFF TO 4.2V

//-----------------------------------------------------------------------------
// GPIO For CAM

#define NUM_BYTES_MESSAGE 8
#define RESET "5" // GPIO 5
#define DIN "19"  // GPIO 19  FPGA -> HOST
#define DOUT "18" // GPIO 18  HOST-> FPGA
#define SCK "16"  // Moved from GPIO 1 to GPIO 16 to free I2C0
#define RESET_DIR "/sys/class/gpio/gpio5/direction"
#define DIN_DIR "/sys/class/gpio/gpio19/direction"
#define DOUT_DIR "/sys/class/gpio/gpio18/direction"
#define SCK_DIR "/sys/class/gpio/gpio16/direction"
#define RESET_VALUE "/sys/class/gpio/gpio5/value"
#define DIN_VALUE "/sys/class/gpio/gpio19/value"
#define DOUT_VALUE "/sys/class/gpio/gpio18/value"
#define SCK_VALUE "/sys/class/gpio/gpio16/value"
#define IN "in"
#define OUT "out"

int getBattStatus(double *batteryData) {

    int fd, result = -1;
    signed short voltage, current;

    if ((fd = i2c_open(1, ADDR_GAS_GAUGE)) < 0) {
        return result;
    }

    if ((result = i2c_read(fd, ADDR_GAS_GAUGE, CELL_1_V_MS, 1)) == -1 ) {
        return result;
    }
    voltage = result << 3;
    if ((result = i2c_read(fd, ADDR_GAS_GAUGE, CELL_1_V_LS, 1)) == -1 ) {
        return result;
    }
    voltage = (voltage | (result >> 5));
    voltage = (voltage | (result >> 5));
    batteryData[0] = 4.883e-3 * voltage;

    if ((result = i2c_read(fd, ADDR_GAS_GAUGE, CELL_2_V_MS, 1)) == -1 ) {
        return result;
    }

    voltage = result << 3;
    if ((result = i2c_read(fd, ADDR_GAS_GAUGE, CELL_2_V_LS, 1)) == -1 ) {
        return result;
    }
    voltage = (voltage | (result >> 5));
    voltage = (voltage | (result >> 5));
    batteryData[1] = 4.883e-3 * voltage;

    if ((result = i2c_read(fd, ADDR_GAS_GAUGE, BATT_I_MS, 1)) == -1 ) {
        return result;
    }
    current = result << 8;
    if ((result = i2c_read(fd, ADDR_GAS_GAUGE, BATT_I_LS, 1)) == -1 ) {
        return result;
    }
    current = current | result;
    batteryData[2] = 1000 * current * (1.5625e-6 / R_SENSE);

    i2c_close(fd);
    return (0);
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

    gpio_export(RESET);
    gpio_export(DOUT);
    gpio_export(DIN);
    gpio_export(SCK);

    gpio_set_direction(RESET_DIR, OUT);
    gpio_set_direction(DOUT_DIR, OUT);
    gpio_set_direction(DIN_DIR, IN);
    gpio_set_direction(SCK_DIR, OUT);

    // Initialize the GPIO lines
    gpio_set_value(RESET_VALUE, "1");
    gpio_set_value(DOUT_VALUE, "0");
    gpio_set_value(SCK_VALUE, "0");

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
            // setup data
            if (!!(data_byte & 0x80)) {
                gpio_set_value(DOUT_VALUE, "1");
            } else {
                gpio_set_value(DOUT_VALUE, "0");
            }
            usleep(100);
            gpio_set_value(SCK_VALUE, "1"); // pulse the clock
            usleep(100);
            gpio_set_value(SCK_VALUE, "0");
            usleep(100);
            data_byte = data_byte << 1;
        }
    }

    gpio_set_value(DOUT_VALUE, "0");
    usleep(2);

    usleep(2000);   //need to let i2c finish

    // Receive the response packet FPGA -> Pi
    for (j = 0; j < NUM_BYTES_MESSAGE; j++) {

        data_byte = 0;

        for (i = 0; i < 8; i++) {
            gpio_set_value(SCK_VALUE, "1"); // pulse the clock
            usleep(100);
            data_byte = ( !!gpio_get_value(DIN_VALUE) << (7 - i) | data_byte);
            gpio_set_value(SCK_VALUE, "0");
            usleep(100);
        }

        recv_packet[j] = data_byte;
    }
    for (j = 0; j < NUM_BYTES_MESSAGE; j++) {
        *(pResponse + j) = recv_packet[j];
    }

    gpio_unexport(RESET);
    gpio_unexport(DOUT);
    gpio_unexport(DIN);
    gpio_unexport(SCK);
}

int main(int argc, char *argv[]) {

    double batteryData[3] = {100.0, 100.0, 100.0};
    if (getBattStatus(batteryData)) {
        fprintf(stderr, "Error quering gas gauge\n");
    }

    if (argc <= 1) {
        fprintf(stdout, "Battery voltage 1: %.2f V \n", batteryData[0]);
        fprintf(stdout, "Battery voltage 2: %.2f V \n", batteryData[1]);
        fprintf(stdout, "Battery current: %.2f mA \n", batteryData[2]);
        return(0);
    }

    if (!strncmp(argv[1], "checkCell_1", strlen("checkCell_1"))) {
        fprintf(stdout, "%.2f\n", batteryData[0]);
        return(0);
    }

    if (!strncmp(argv[1], "checkCell_2", strlen("checkCell_2"))) {
        fprintf(stdout, "%.2f\n", batteryData[1]);
        return(0);
    }

    if (!strncmp(argv[1], "powerdown", strlen("powerdown"))) {
        fprintf(stdout, "Requesting powerdown from FPGA...\n");
        char cTemp[256] = {0};
        cam(0x0F, 0xB2, 0x00, 0x00, 0x00, cTemp);
        return 0;
    }

}
