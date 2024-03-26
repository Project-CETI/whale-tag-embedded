//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Saksham Ahuja, Matt Cummings, Shanaya Barretto, Michael Salino-Hugg
//-----------------------------------------------------------------------------

#ifndef MAX17320_H
#define MAX17320_H

// Include files
#define _GNU_SOURCE // change how sched.h will be included

#include <stdbool.h>
#include "launcher.h"      // for g_stopAcquisition, sampling rate, data filepath, and CPU affinity
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/logging.h"

// Device Address
// Addresses appear shifted 1 bit right on the pi, datasheet reports addresses 0x6C & 0x16
#define MAX17320_ADDR                   0x36  // For internal memory range 000h-0FFh
#define MAX17320_ADDR_SEC               0x0b  // For internal memory range 180h-1FFh

#define MAX17320_REG_STATUS             0x000
#define MAX17320_REG_REP_CAPACITY       0x005
#define MAX17320_REG_REP_SOC            0x006
#define MAX17320_REG_CELL1_VOLTAGE      0x0D8
#define MAX17320_REG_CELL2_VOLTAGE      0x0D7
#define MAX17320_REG_TOTAL_BAT_VOLTAGE  0x0DA
#define MAX17320_REG_PACK_SIDE_VOLTAGE	0x0DB
#define MAX17320_REG_TEMPERATURE		0x01B
#define MAX17320_REG_BATT_CURRENT		0x01C
#define MAX17320_REG_AVG_BATT_CURRENT	0x01D
#define MAX17320_REG_TIME_TO_EMPTY		0x011
#define MAX17320_REG_TIME_TO_FULL		0x020
#define MAX17320_REG_CURRENT_ALT_THR	0x0AC
#define MAX17320_REG_VOLTAGE_ALT_THR	0x001
#define MAX17320_REG_COMM_STAT			0x061
#define MAX17320_REG_DEV_NAME           0x021

// LSB Conversion Macros
#define R_SENSE_VAL						0.001 // Ω
#define CAPACITY_LSB					0.005 // mVh, must divide by R_sense to get mAh value
#define PERCENTAGE_LSB					1/256 // %
#define CELL_VOLTAGE_LSB				0.000078125 // V
#define PACK_VOLTAGE_LSB				0.0003125; // V
#define CURRENT_LSB						0.0015625 // mV, must divide by R_sense to get mA value
#define TEMPERATURE_LSB					1/256 // °C
#define RESISTANCE_LSB					1/4096 // Ω
#define TIME_LSB						5.625 // s
#define CURRENT_ALT_LSB					0.4 // mV, must divide by R_sense to get mA value
#define VOLTAGE_ALT_LSB					0.02 // V

// Alert and Other Thresholds
#define MAX17320_MIN_CURRENT_THR		0x00
#define MAX17320_MAX_CURRENT_THR		0x00
#define MAX17320_MIN_VOLTAGE_THR		4.0 // V
#define MAX17320_MAX_VOLTAGE_THR		3.0 // V
#define MAX17320_MIN_TEMP_THR			0x00 // Not defined yet
#define MAX17320_MAX_TEMP_THR			0x00
#define MAX17320_MIN_SOC_THR			0x00 // Not defined yet
#define MAX17320_MAX_SOC_THR			0x00
#define MAX17320_CELL_BAL_THR			0b011 // Corresponds to a 10.0 mV threshold

// Other Macros
#define MAX17320_TIMEOUT                1000
#define SECOND_TO_HOUR					3600

// 8-bit to 16-bit conversion
#define TO_16_BIT(b1, b2)				((uint16_t)(b2 << 8) | (uint16_t)b1)

// shift a value (val) but amount (s) and width (w)
#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1)) 

// Register Structs
typedef struct {
	bool power_on_reset;
	bool min_current_alert;
	bool max_current_alert;
	bool state_of_charge_change_alert; // Alerts 1% change
	bool min_voltage_alert;
	bool min_temp_alert;
	bool min_state_of_charge_alert;
	bool max_voltage_alert;
	bool max_temp_alert;
	bool max_state_of_charge_alert;
	bool protection_alert;

} max17320_Reg_Status;

// Device struct
typedef struct __MAX17320_HandleTypeDef {
	max17320_Reg_Status status;
	float remaining_capacity; // mAh
    float state_of_charge; // %

    float cell_1_voltage; // Voltage of cell connected between MID and NEG
    float cell_2_voltage; // Voltage of cell connected between POS and MID
    float total_battery_voltage; // Total battery pack voltage between POS and NEG
    float pack_side_voltage; // Pack-side voltage input into system

    float temperature; // °C

    float battery_current; // A
    float average_current; // A

    float time_to_empty; // h
    float time_to_full; // h

} MAX17320_HandleTypeDef;

int max17320_init(MAX17320_HandleTypeDef *dev);
int max17320_clear_write_protection(MAX17320_HandleTypeDef *dev);
int max17320_get_status(MAX17320_HandleTypeDef *dev);
int max17320_get_remaining_capacity(MAX17320_HandleTypeDef *dev);
int max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev);
int max17320_get_voltages(MAX17320_HandleTypeDef *dev);
int max17320_get_temperature(MAX17320_HandleTypeDef *dev);
int max17320_get_battery_current(MAX17320_HandleTypeDef *dev);
int max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev);
int max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev);
int max17320_get_time_to_full(MAX17320_HandleTypeDef *dev);
int max17320_set_alert_thresholds(MAX17320_HandleTypeDef *dev);
int max17320_configure_cell_balancing(MAX17320_HandleTypeDef *dev);

#endif // MAX17320_H