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

// TODO: Remove Unused Registers
#define MAX17320_REG_STATUS             0x000
#define MAX17320_REG_PROTSTATUS			0x0D9
#define MAX17320_REG_PROTALRT			0x0AF
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
#define MAX17320_REG_COMMAND			0x060
#define MAX17320_REG_DEV_NAME           0x021
#define MAX17320_REG_REMAINING_WRITES	0x1FD
#define MAX17320_REG_CONFIG2			0x0AB
#define MAX17320_REG_DIETEMP            0x034

// Registers for Non-Volatile Writes
#define MAX17320_REG_NPACKCFG			0x1B5
#define MAX17320_REG_NNVCFG0			0x1B8
#define MAX17320_REG_NNVCFG1			0x1B9
#define MAX17320_REG_NNVCFG2			0x1BA
#define MAX17320_REG_NUVPRTTH			0x1D0
#define MAX17320_REG_NTPRTTH1			0x1D1
#define MAX17320_REG_NIPRTTH1			0x1D3
#define MAX17320_REG_NBALTH				0x1D4
#define MAX17320_REG_NPROTMISCTH		0x1D6
#define MAX17320_REG_NPROTCFG			0x1D7
#define MAX17320_REG_NJEITAV			0x1D9
#define MAX17320_REG_NOVPRTTH			0x1DA
#define MAX17320_REG_NDELAYCFG			0x1DC
#define MAX17320_REG_NODSCCFG			0x1DE
#define MAX17320_REG_NCONFIG			0x1B0
#define MAX17320_REG_NTHERMCFG			0x1CA
#define MAX17320_REG_NVEMPTY			0x19E
#define MAX17320_REG_NFULLSOCTHR		0x1C6

// Data for Non-Volatile Writes
// Refer to 'NV Write Decisions BMS FW' Spreadsheet for details
#define MAX17320_VAL_NPACKCFG			0xC204
#define MAX17320_VAL_NNVCFG0			0x0830
#define MAX17320_VAL_NNVCFG1			0x2100
#define MAX17320_VAL_NNVCFG2			0x822D
#define MAX17320_VAL_NUVPRTTH			0xA002
#define MAX17320_VAL_NTPRTTH1			0x280A
#define MAX17320_VAL_NIPRTTH1			0x03FD
#define MAX17320_VAL_NBALTH				0x0CAA
#define MAX17320_VAL_NPROTMISCTH		0x0313
#define MAX17320_VAL_NPROTCFG			0x0C08
#define MAX17320_VAL_NJEITAV			0xEC00
#define MAX17320_VAL_NOVPRTTH			0xB3A0
#define MAX17320_VAL_NDELAYCFG			0x0035
#define MAX17320_VAL_NODSCCFG			0x4058
#define MAX17320_VAL_NCONFIG			0x0290
#define MAX17320_VAL_NTHERMCFG			0x71BE
#define MAX17320_VAL_NVEMPTY			0x9659
#define MAX17320_VAL_NFULLSOCTHR		0x5005

// LSB Conversion Macros
#define R_SENSE_VAL						0.001 // Ω
#define CAPACITY_LSB					0.005 // mVh, must divide by R_sense to get mAh value
#define PERCENTAGE_LSB					1/256 // %
#define CELL_VOLTAGE_LSB				0.000078125 // V
#define PACK_VOLTAGE_LSB				0.0003125; // V
#define CURRENT_LSB_uV					1.5625 // uV, must divide by R_sense to get mA value
#define TEMPERATURE_LSB					1/256 // °C
#define RESISTANCE_LSB					1/4096 // Ω
#define TIME_LSB						5.625 // s
#define CURRENT_ALT_LSB					0.4 // mV, must divide by R_sense to get mA value
#define VOLTAGE_ALT_LSB					0.02 // V

// Other Macros
#define MAX17320_TIMEOUT                1000
#define SECOND_TO_HOUR					3600
#define CLEAR_WRITE_PROT                0x0000
#define CLEARED_WRITE_PROT              0x0004
#define LOCKED_WRITE_PROT               0x00F9
#define DETERMINE_REMAINING_UPDATES     0xE29B   
#define INITIATE_BLOCK_COPY             0xE904
#define TRECALL_US						5000
#define MAX17320_RESET 					0x000F
#define MAX17320_RESET_FW				0x8000
#define CHARGE_ON						0xFEFF
#define CHARGE_OFF						0x0100
#define DISCHARGE_ON					0xFDFF
#define DISCHARGE_OFF          			0x0200   

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

typedef struct {
	bool ship;
	bool perm_fail;
	bool die_overtemperature; // chg & dis fault
	bool res_d_fault; // unexplained in the datasheet
	// Discharge faults
	bool overdischarge_current;
	bool undervoltage;
	bool too_hot_discharge;
	// Charge faults
	bool overcharge_current;
	bool overvoltage;
	bool too_hot_charge;
	bool too_cold_charge;
	bool capacity_overflow;
	bool full;
	bool watchdog_timer;
	bool imbalance;
	bool prequal_timeout;
} max17320_Reg_ProtStatus;

typedef struct {
	bool leak_detection;
	bool perm_fail;
	bool die_overtemperature; // chg & dis fault
	bool res_d_fault; // unexplained in the datasheet
	// Discharge faults
	bool overdischarge_current;
	bool undervoltage;
	bool too_hot_discharge;
	// Charge faults
	bool overcharge_current;
	bool overvoltage;
	bool too_hot_charge;
	bool too_cold_charge;
	bool capacity_overflow;
	bool full;
	bool watchdog_timer;
	bool imbalance;
	bool prequal_timeout;
} max17320_Reg_ProtAlrt;

// Device struct
typedef struct __MAX17320_HandleTypeDef {
	max17320_Reg_Status status;
	max17320_Reg_ProtStatus prot_status;
	max17320_Reg_ProtAlrt prot_alert;
	double remaining_capacity; // mAh
    double state_of_charge; // %

    double cell_1_voltage; // Voltage of cell connected between MID and NEG
    double cell_2_voltage; // Voltage of cell connected between POS and MID
    double total_battery_voltage; // Total battery pack voltage between POS and NEG
    double pack_side_voltage; // Pack-side voltage input into system

    double temperature; // °C
	double die_temperature; // °C

    double battery_current; // A
    double average_current; // A

    double time_to_empty; // h
    double time_to_full; // h

	uint8_t remaining_writes;
} MAX17320_HandleTypeDef;

int max17320_init(MAX17320_HandleTypeDef *dev);
int max17320_clear_write_protection(MAX17320_HandleTypeDef *dev);
int max17320_lock_write_protection(MAX17320_HandleTypeDef *dev);
int max17320_get_status(MAX17320_HandleTypeDef *dev);
int max17320_get_prot_status(MAX17320_HandleTypeDef *dev);
int max17320_get_prot_alrt(MAX17320_HandleTypeDef *dev);
int max17320_get_remaining_capacity(MAX17320_HandleTypeDef *dev);
int max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev);
int max17320_get_voltages(MAX17320_HandleTypeDef *dev);
int max17320_get_temperature(MAX17320_HandleTypeDef *dev);
int max17320_get_battery_current(MAX17320_HandleTypeDef *dev);
int max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev);
int max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev);
int max17320_get_time_to_full(MAX17320_HandleTypeDef *dev);
int max17320_get_remaining_writes(MAX17320_HandleTypeDef *dev);
int max17320_gauge_reset(MAX17320_HandleTypeDef *dev);
int max17320_reset(MAX17320_HandleTypeDef *dev);
int max17320_nonvolatile_write(MAX17320_HandleTypeDef *dev);
int max17320_verify_nonvolatile(MAX17320_HandleTypeDef *dev);
int max17320_enable_charging(MAX17320_HandleTypeDef *dev);
int max17320_enable_discharging(MAX17320_HandleTypeDef *dev);
int max17320_disable_charging(MAX17320_HandleTypeDef *dev);
int max17320_disable_discharging(MAX17320_HandleTypeDef *dev);

extern MAX17320_HandleTypeDef dev;

#endif // MAX17320_H