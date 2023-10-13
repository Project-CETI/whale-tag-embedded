
/*
 *      @file   max17320.h
 *      @brief  Header file of MAX17320G2 Driver
 *      @author Saksham Ahuja
 *      Datasheet: https://www.analog.com/en/products/max17320.html
 *
 */


#ifndef MAX17320_H
#define MAX17320_H


// Include Files
#include "stm32u5xx_hal.h"
#include "tx_port.h"
#include <stdbool.h>
#include <util.h>


// MAX17320G2 Device Address
#define MAX17320_DEV_ADDR               0x6C // For internal memory range 0x000 - 0x0FF
#define MAX17320_DEV_ADDR_END           0x16

// MAX17320G2 Data Registers
#define MAX17320_REG_STATUS             0x000
#define MAX17320_REG_PROT_ALERT			0x0AF
#define MAX17320_REG_PROT_CFG			0x1F1

#define MAX17320_REG_REP_CAPACITY		0x005
#define MAX17320_REG_REP_SOC			0x006
#define MAX17320_REG_SOC				0x1C6

#define MAX17320_REG_CELL1_VOLTAGE		0x0D8
#define MAX17320_REG_CELL2_VOLTAGE		0x0D7
#define MAX17320_REG_TOTAL_BAT_VOLTAGE	0x0DA
#define MAX17320_REG_PACK_SIDE_VOLTAGE	0x0DB

#define MAX17320_REG_TEMPERATURE		0x01B

#define MAX17320_REG_PACK_CFG			0x1B5

#define MAX17320_REG_BATT_CURRENT		0x01C
#define MAX17320_REG_AVG_BATT_CURRENT	0x01D

#define MAX17320_REG_TIME_TO_EMPTY		0x011
#define MAX17320_REG_TIME_TO_FULL		0x020

#define MAX17320_REG_CURRENT_ALT_THR	0x0AC
#define MAX17320_REG_VOLTAGE_ALT_THR	0x001
#define MAX17320_REG_TEMP_ALT_THR		0x002
#define MAX17320_REG_SOC_ALT_THR		0x003

#define MAX17320_REG_CELL_BAL_THR		0x1D4

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
#define TEMP_ALT_LSB					1.0 // °C
#define SOC_ALT_LSB						1.0 // %

// Alert Thresholds
#define MAX17320_MIN_CURRENT_THR		0x00
#define MAX17320_MAX_CURRENT_THR		0x00

#define MAX17320_MIN_VOLTAGE_THR		3.0 // V
#define MAX17320_MAX_VOLTAGE_THR		4.0 // V

#define MAX17320_MIN_TEMP_THR			0x00 // Not defined yet
#define MAX17320_MAX_TEMP_THR			0x00

#define MAX17320_MIN_SOC_THR			0x00 // Not defined yet
#define MAX17320_MAX_SOC_THR			0x00

// State Machine Flag Thresholds
#define MAX17320_LOW_BATT_VOLT_THR		0x00
#define MAX17320_CRITICAL_BATT_VOLT_THR	0x00

// Cell Balancing Thresholds
#define MAX17320_CELL_BAL_THR			0b011 // Corresponds to a 10.0 mV threshold
#define MAX17320_CELL_R_BAL_THR			0b011 // Corresponds to a 11.7mΩ threshold

#define MAX17320_CELL_CHARGE_THR		0x02 // Corresponds to 5mA current charging threshold, -5mA current discharging threshold
#define MAX17320_CELL_SOC_THR			0x5005 // Corresponds to 80% full charge threshold

// Thermistor Settings
#define MAX17320_NUM_THERMISTORS		0x01
#define MAX17320_THERMISTOR_TYPE		0x00 // 10kΩ thermistor

// Other Macros
#define MAX17320_TIMEOUT                1000
#define SECOND_TO_HOUR					3600

// 8-bit to 16-bit conversion
#define TO_16_BIT(b1, b2)				((uint16_t)(b2 << 8) | (uint8_t)b1)


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
	bool ldet_alert;
	bool resd_fault_alert;
	bool overdischarge_current_alert;
	bool undervoltage_alert;
	bool too_hot_discharge_alert;
	bool die_hot_alert;
	bool permanent_fail_alert;
	bool multicell_imbalance_alert;
	bool prequal_timeout_alert;
	bool queue_overflow_alert;
	bool overcharge_current_alert;
	bool overvoltage_alert;
	bool too_cold_charge_alert;
	bool full_detect_alert;
	bool too_hot_charge_alert;
	bool charge_watchdog_timer_alert;

} max17320_Reg_Faults;



// Device Struct
typedef struct __MAX17320_HandleTypeDef {

    I2C_HandleTypeDef *i2c_handler;


    max17320_Reg_Status status;
    max17320_Reg_Faults faults;

    float remaining_capacity; // mAh
    float state_of_charge; // %

    float cell_1_voltage; // Voltage of cell connected between MID and NEG
    float cell_2_voltage; // Voltage of cell connected between POS and MID
    float total_battery_voltage; // Total battery pack voltage between POS and NEG
    float pack_side_voltage; // Pack-side voltage input into system

    float temperature; // °C

    float battery_current; // mA
    float average_current; // mA

    float time_to_empty; // h
    float time_to_full; // h

    uint16_t raw; // buffer for getting bits of any register


} MAX17320_HandleTypeDef;


HAL_StatusTypeDef max17320_init(MAX17320_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c_device);
HAL_StatusTypeDef max17320_clear_write_protection(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_set_alert_thresholds(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_configure_cell_balancing(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_configure_thermistors(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_status(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_faults(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_fet_status(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_remaining_capacity(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_voltages(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_temperature(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_battery_current(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev);
HAL_StatusTypeDef max17320_get_time_to_full(MAX17320_HandleTypeDef *dev);

//Main BMS thread to run on RTOS
void bms_thread_entry(ULONG thread_input);


#endif // MAX17320_H
