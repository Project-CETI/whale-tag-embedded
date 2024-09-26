//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Saksham Ahuja, Matt Cummings, Shanaya Barretto, Michael Salino-Hugg
//-----------------------------------------------------------------------------

#ifndef MAX17320_H
#define MAX17320_H

// Include files
#include "../utils/error.h" // for WTResult
#include <stdint.h>

#define MAX17320_CELL_COUNT 2

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

#define MAX17320_REG_DESIGN_CAP			0x018

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
#define MAX17320_REG_nRSense			0x1CF

// Data for Non-Volatile Writes
// Refer to 'NV Write Decisions BMS FW' Spreadsheet for details
#define MAX17320_VAL_NPACKCFG			0xC204
#define MAX17320_VAL_NNVCFG0			0x0830
#define MAX17320_VAL_NNVCFG1			0x2100
#define MAX17320_VAL_NNVCFG2			0x822D
#define MAX17320_VAL_NUVPRTTH			0xA002
#define MAX17320_VAL_NTPRTTH1			0x280A
#define MAX17320_VAL_NIPRTTH1			0x32CE   // MRC - change to OCCP 2A, OCDP 2A if Rsense 0.010
#define MAX17320_VAL_NBALTH				0x0CA0	 // MRC - change so that imbalance doesn't block charging (consider putting in a limit at some point)
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
#define R_SENSE_VAL						0.010 // Ω  MRC changed from 0.001 for integration
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
#define CLEAR_WRITE_PROT                0x0000
#define CLEARED_WRITE_PROT              0x0004
#define LOCKED_WRITE_PROT               0x00F9
#define DETERMINE_REMAINING_UPDATES     0xE29B   
#define INITIATE_BLOCK_COPY             0xE904
#define TRECALL_US						5000
#define MAX17320_RESET 					0x000F
#define MAX17320_RESET_FW				0x8000
#define CHARGE_OFF						0x0100
#define DISCHARGE_OFF          			0x0200   

#define DESIGN_CAP 						0x2710	//5000 mAH if R_SENSE_VAL = 0.010

// shift a value (val) but amount (s) and width (w)
#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1)) 


WTResult max17320_read(uint16_t memory, uint16_t *storage);
WTResult max17320_write(uint16_t memory, uint16_t data);
WTResult max17320_clear_write_protection(void);
WTResult max17320_get_remaining_capacity_mAh(double *pCapacity_mAh);
WTResult max17320_get_state_of_charge(double *pSoc);
WTResult max17320_get_cell_voltage_v(int cell_index, double *vCells_v);
WTResult max17320_get_cell_temperature_c(int cell_index, double *tCells_c);
WTResult max17320_get_die_temperature_c(double *pDieTemp_c);
WTResult max17320_get_current_mA(double *pCurrent_mA);
WTResult max17320_get_average_current_mA(double *pAvgI_mA);
WTResult max17320_get_time_to_empty_s(double *pTimeToEmpty_s);
WTResult max17320_get_time_to_full_s(double *pTimeToFull_s);
WTResult max17320_swap_shadow_ram(void);
WTResult max17320_gauge_reset(void);
WTResult max17320_reset(void);
WTResult max17320_get_remaining_writes(uint8_t *pRemainingWrites);
WTResult max17320_enable_charging(void);
WTResult max17320_enable_discharging(void);
WTResult max17320_disable_charging(void);
WTResult max17320_disable_discharging(void);

WTResult max17320_init(void);
#endif // MAX17320_H