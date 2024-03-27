//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Saksham Ahuja, Matt Cummings, Shanaya Barretto, Michael Salino-Hugg
//-----------------------------------------------------------------------------

#include "max17320.h"

// Global variables
// TODO add battery thread
// TODO we've created a struct, they just have global vars, evaluate what is needed
static FILE* battery_data_file = NULL;
static char battery_data_file_notes[256] = "";
static const char* battery_data_file_headers[] = {
  "Battery V1 [V]",
  "Battery V2 [V]",
  "Battery I [mA]",
  };
static const int num_battery_data_file_headers = 3;

static inline max17320_Reg_Status __statusRegister_from_raw(uint16_t raw) {
    return (max17320_Reg_Status) {
        .power_on_reset = _RSHIFT(raw, 1, 1),
        .min_current_alert = _RSHIFT(raw, 2, 1),
	    .max_current_alert = _RSHIFT(raw, 6, 1),
	    .state_of_charge_change_alert = _RSHIFT(raw, 7, 1),
	    .min_voltage_alert = _RSHIFT(raw, 8, 1),
	    .min_temp_alert = _RSHIFT(raw, 9, 1),
	    .min_state_of_charge_alert = _RSHIFT(raw, 10, 1),
	    .max_voltage_alert = _RSHIFT(raw, 12, 1),
	    .max_temp_alert = _RSHIFT(raw, 13, 1),
	    .max_state_of_charge_alert = _RSHIFT(raw, 14, 1),
	    .protection_alert = _RSHIFT(raw, 15, 1),
    };
}

int max17320_init(MAX17320_HandleTypeDef *dev) {
    int ret = -1;
    // check status
    ret = max17320_get_status(dev);
    ret = max17320_get_remaining_capacity(dev);
    ret = max17320_get_state_of_charge(dev);
    ret = max17320_get_voltages(dev);
    ret = max17320_get_temperature(dev);
    ret = max17320_get_battery_current(dev);
    ret = max17320_get_average_battery_current(dev);
    ret = max17320_get_time_to_empty(dev);
    ret = max17320_get_time_to_full(dev);
    // Open an output file to write data.
    if(init_data_file(battery_data_file, MAX17320_DATA_FILEPATH,
                        battery_data_file_headers,  num_battery_data_file_headers,
                        battery_data_file_notes, "init_battery()") < 0)
        ret = -1;
    return ret;
}

int max17320_clear_write_protection(MAX17320_HandleTypeDef *dev) {
    // TODO
}

int max17320_get_status(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_STATUS);
        dev->status = __statusRegister_from_raw(read);
    }
    CETI_LOG("MAX17320 Status: 0x%.4x", read);
    i2cClose(fd);
    return ret;
}

int max17320_get_remaining_capacity(MAX17320_HandleTypeDef* dev) {
    uint16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_REP_CAPACITY) * (CAPACITY_LSB / R_SENSE_VAL);
        dev->remaining_capacity = read;
    }
    CETI_LOG("MAX17320 Remaining Capacity: %u mAh", read);
    i2cClose(fd);
    return ret;
}

int max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_REP_SOC)  * PERCENTAGE_LSB;
        dev->state_of_charge = read;
    }
    CETI_LOG("MAX17320 State of Charge: %u %%", read);
    i2cClose(fd);
    return ret;
}

int max17320_get_voltages(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        // Cell 1 Voltage
        read = i2cReadWordData(fd, MAX17320_REG_CELL1_VOLTAGE) * CELL_VOLTAGE_LSB;
        dev->cell_1_voltage = read;
        CETI_LOG("MAX17320 Cell 1 Voltage: %u V", read);
        // Cell 2 Voltage
        read = i2cReadWordData(fd, MAX17320_REG_CELL2_VOLTAGE) * CELL_VOLTAGE_LSB;
        dev->cell_2_voltage = read;
        CETI_LOG("MAX17320 Cell 2 Voltage: %u V", read);
        // Total Battery Voltage
        read = i2cReadWordData(fd, MAX17320_REG_TOTAL_BAT_VOLTAGE) * PACK_VOLTAGE_LSB;
        dev->total_battery_voltage = read;
        CETI_LOG("MAX17320 Total Battery Voltage: %u V", read);
        // Pack Side Voltage
        read = i2cReadWordData(fd, MAX17320_REG_PACK_SIDE_VOLTAGE) * PACK_VOLTAGE_LSB;
        dev->pack_side_voltage = read;
        CETI_LOG("MAX17320 Pack Side Voltage: %u V", read);
    }
    i2cClose(fd);
    return ret;
}

int max17320_get_temperature(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_TEMPERATURE) * TEMPERATURE_LSB;
        dev->temperature = read;
    }
    CETI_LOG("MAX17320 Temperature: %u °C", read);
    i2cClose(fd);
    return ret;
}
int max17320_get_battery_current(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_BATT_CURRENT) * (CURRENT_LSB/R_SENSE_VAL) * 100;
        dev->battery_current = read;
    }
    CETI_LOG("MAX17320 Battery Current: %u mA", read);
    i2cClose(fd);
    return ret;
}
int max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_AVG_BATT_CURRENT) * (CURRENT_LSB/R_SENSE_VAL) * 100;
        dev->average_current = read;
    }
    CETI_LOG("MAX17320 Avg Battery Current: %u mA", read);
    i2cClose(fd);
    return ret;
}

int max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_TIME_TO_EMPTY) * (TIME_LSB / SECOND_TO_HOUR);
        dev->time_to_empty = read;
    }
    CETI_LOG("MAX17320 Time to Empty: %u hrs", read);
    i2cClose(fd);
    return ret;
}
int max17320_get_time_to_full(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        read = i2cReadWordData(fd, MAX17320_REG_TIME_TO_FULL) * (TIME_LSB / SECOND_TO_HOUR);
        dev->time_to_full = read;
    }
    CETI_LOG("MAX17320 Time to Full: %u hrs", read);
    i2cClose(fd);
    return ret;
}
int max17320_set_alert_thresholds(MAX17320_HandleTypeDef *dev) {
    // TODO
}
int max17320_configure_cell_balancing(MAX17320_HandleTypeDef *dev) {
    // TODO
}