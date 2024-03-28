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
// TODO error checking for all writes/reads
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

static inline int max17320_write(MAX17320_HandleTypeDef *dev, uint16_t memory, uint16_t data) {
    int ret = 0;
    uint16_t addr = MAX17320_ADDR;
    if (memory > 0xFF) {
        memory = memory & 0xFF;
        addr = MAX17320_ADDR_SEC;
    }
    int fd=i2cOpen(1, addr, 0);
    if (fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        ret = i2cWriteWordData(fd, memory, data);
    }
    i2cClose(fd);
    // TODO: Add error checking based on function
    return ret;
}

static inline int max17320_read(MAX17320_HandleTypeDef *dev, uint16_t memory, uint16_t *storage) {
    int ret = 0;
    uint16_t addr = MAX17320_ADDR;
    if (memory > 0xFF) {
        memory = memory & 0xFF;
        addr = MAX17320_ADDR_SEC;
    }
    int fd=i2cOpen(1, addr, 0);
    if (fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        *storage = i2cReadWordData(fd, memory);
    }
    i2cClose(fd);
    return ret;
    // TODO before using ensure all header registers for 0x0b are in full form
}

int max17320_init(MAX17320_HandleTypeDef *dev) {
    int ret = -1;
    // check status
    ret = max17320_get_status(dev);
    ret = max17320_get_remaining_writes(dev);
    
    // Open an output file to write data.
    if(init_data_file(battery_data_file, MAX17320_DATA_FILEPATH,
                        battery_data_file_headers,  num_battery_data_file_headers,
                        battery_data_file_notes, "init_battery()") < 0)
        ret = -1;
    return ret;
}

int max17320_clear_write_protection(MAX17320_HandleTypeDef *dev) {
    int ret = 0;
    uint16_t read;
    int fd=i2cOpen(1, MAX17320_ADDR, 0);
    if (fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    // Through testing, it was determined that three writes are needed to properly clear protection
    uint8_t counter = 3;
    while (counter > 0)
    {
        ret = i2cWriteWordData(fd, MAX17320_REG_COMM_STAT, CLEARED_WRITE_PROT);
        usleep(TRECALL);
        CETI_LOG("MAX17320 comm stat %u: %u", counter, read);
        counter--;
    }
    read = i2cReadWordData(fd, MAX17320_REG_COMM_STAT);
    if (read != CLEARED_WRITE_PROT)
    {
        CETI_ERR("MAX17320 Clearing write protection failed");
        CETI_ERR("MAX17320 comm stat reg: %u", read);
        ret = -1;
    }
    i2cClose(fd);
    return ret;
}

int max17320_lock_write_protection(MAX17320_HandleTypeDef *dev) {
    int ret = 0;
    uint16_t read;
    int fd=i2cOpen(1, MAX17320_ADDR, 0);
    if (fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }

    uint8_t counter = 2;
    while (counter > 0)
    {
        ret = i2cWriteWordData(fd, MAX17320_REG_COMM_STAT, LOCKED_WRITE_PROT);
        usleep(TRECALL);
        counter--;
    }
    read = i2cReadWordData(fd, MAX17320_REG_COMM_STAT);
    if (read != LOCKED_WRITE_PROT)
    {
        ret = -1;
    }

    i2cClose(fd);
    return ret;
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

int max17320_get_remaining_writes(MAX17320_HandleTypeDef *dev) {
    int ret = 0;
    uint16_t read;
    // Clear write protection
    ret = max17320_clear_write_protection(dev);
    if (ret < 0)
    {
        return ret;
    }
    
    // Connect to first bus to write to command register
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    // Write to command register
    ret = i2cWriteWordData(fd, MAX17320_REG_COMMAND, DETERMINE_REMAINING_UPDATES);
    usleep(TRECALL);
    i2cClose(fd);

    // Connect to second bus to read remaining writes
    fd=i2cOpen(1,MAX17320_ADDR_SEC,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    read = i2cReadWordData(fd, MAX17320_REG_REMAINING_WRITES);
    i2cClose(fd);

    uint8_t first_byte = (read>>8) & 0xff;
    uint8_t last_byte = read & 0xff;
    uint8_t decoded = first_byte | last_byte;
    uint8_t count = 0;
    while (decoded > 0)
    {
        if (decoded & 1)
        {
            count++;
        }
        decoded = decoded >> 1;
    }
    dev->remaining_writes = (8-count);
    CETI_LOG("MAX17320 Remaining Writes: %u hrs", dev->remaining_writes);
    return ret;
}
