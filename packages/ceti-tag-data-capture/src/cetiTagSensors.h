//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagSensors.h
// Description: Header for sensor-related functions (excluding the IMU)
//-----------------------------------------------------------------------------

#ifndef CETI_SNSRS_H
#define CETI_SNSRS_H

#include <inttypes.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pigpio.h>

#include "cetiTagLogging.h"
#include "cetiTagWrapper.h"

#define SNS_SMPL_PERIOD 1000000 // microseconds

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

// Peripheral 7-bit Slave Addresses
#define ADDR_GAS_GAUGE 0x59
#define ADDR_RTC 0x68
#define ADDR_LIGHT 0x29
#define ADDR_DEPTH 0x40
#define ADDR_TEMP 0x4C

#define ADDR_IMU 0x4A
#define BUS_IMU 0x00

// IO Expander
#define IOX_INPUT 0x00
#define IOX_OUTPUT 0x01
#define IOX_POLARITY 0x02
#define IOX_CONFIG 0x03
#define RDY_LED 0x80
#define nBW_ON 0x01
#define BW_RST 0x02
#define RF_ON_BB 0x04 // breadboard version
#define RF_ON 0x40    // alpha version

// Keller 4LD Pressure Sensor 200 bar
// Reference pressure is a 1 bar abs
#define PMIN 0   // bar
#define PMAX 200 // bar

// Bitwise items
#define BIT_0 0x01
#define BIT_1 0x02
#define BIT_2 0x04
#define BIT_3 0x08
#define BIT_4 0x10
#define BIT_5 0x20
#define BIT_6 0x40
#define BIT_7 0x80

#endif