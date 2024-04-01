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
    // Can add verification but there are some exceptions
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
    uint16_t read = 0;
    int ret = 0;
    // Through testing, it was determined that three writes are needed to properly clear protection
    uint8_t counter = 3;
    while (counter > 0)
    {
        ret = max17320_write(dev, MAX17320_REG_COMM_STAT, CLEARED_WRITE_PROT);
        usleep(TRECALL);
        counter--;
    }
    ret |= max17320_read(dev, MAX17320_REG_COMM_STAT, &read);
    if (read != CLEARED_WRITE_PROT)
    {
        CETI_ERR("MAX17320 Clearing write protection failed, CommStat: 0x%.4x", read);
        ret = -1;
    }
    return ret;
}

int max17320_lock_write_protection(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    uint8_t counter = 2;
    int ret = 0;
    while (counter > 0)
    {
        ret = max17320_write(dev, MAX17320_REG_COMM_STAT, LOCKED_WRITE_PROT);
        usleep(TRECALL);
        counter--;
    }
    ret |= max17320_read(dev, MAX17320_REG_COMM_STAT, &read);
    if (read != LOCKED_WRITE_PROT)
    {
        CETI_ERR("MAX17320 Locking write protection failed, CommStat: 0x%.4x", read);
        ret = -1;
    }
    return ret;
}

int max17320_get_status(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_STATUS, &read);
    dev->status = __statusRegister_from_raw(read);
    CETI_LOG("MAX17320 Status: 0x%.4x", read);
    return ret;
}

int max17320_get_remaining_capacity(MAX17320_HandleTypeDef* dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_REP_CAPACITY, &read);
    CETI_LOG("MAX17320 Remaining Capacity Pre struct: %u mAh", dev->remaining_capacity);
    dev->remaining_capacity = read * (CAPACITY_LSB / R_SENSE_VAL);
    CETI_LOG("MAX17320 Remaining Capacity Saved: %u mAh", dev->remaining_capacity);
    CETI_LOG("MAX17320 Remaining Capacity Read after save: %u mAh", read);

    // Try old way
    int fd=i2cOpen(1, MAX17320_ADDR, 0);
    if (fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
        ret = -1;
    }
    else {
        uint16_t oldread = i2cReadWordData(fd, MAX17320_REG_REP_CAPACITY) * (CAPACITY_LSB / R_SENSE_VAL);
        CETI_LOG("MAX17320 Remaining Capacity Old: %u mAh", oldread);
        CETI_LOG("MAX17320 Remaining Capacity Just Factor Early: %u mAh", (CAPACITY_LSB / R_SENSE_VAL));
        CETI_LOG("MAX17320 Remaining Capacity Raw: %f mAh", read * (CAPACITY_LSB / R_SENSE_VAL));
        CETI_LOG("MAX17320 Remaining Capacity Just Read: %u mAh", read);
        CETI_LOG("MAX17320 Remaining Capacity Just Factor: %f mAh", (CAPACITY_LSB / R_SENSE_VAL));
    }
    i2cClose(fd);
    return ret;
}

int max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_REP_SOC, &read);
    dev->state_of_charge = read * PERCENTAGE_LSB;
    CETI_LOG("MAX17320 State of Charge: %u %%", dev->state_of_charge);
    return ret;
}

int max17320_get_voltages(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    // Cell 1 Voltage
    int ret = max17320_read(dev, MAX17320_REG_CELL1_VOLTAGE, &read);
    dev->cell_1_voltage = read * CELL_VOLTAGE_LSB;
    CETI_LOG("MAX17320 Cell 1 Voltage: %u V", dev->cell_1_voltage);

    // Cell 2 Voltage
    ret |= max17320_read(dev, MAX17320_REG_CELL2_VOLTAGE, &read);
    dev->cell_2_voltage = read * CELL_VOLTAGE_LSB;
    CETI_LOG("MAX17320 Cell 2 Voltage: %u V", dev->cell_2_voltage);

    // Total Battery Voltage
    ret |= max17320_read(dev, MAX17320_REG_TOTAL_BAT_VOLTAGE, &read);
    dev->total_battery_voltage = read * PACK_VOLTAGE_LSB;
    CETI_LOG("MAX17320 Total Battery Voltage: %u V", dev->total_battery_voltage);
    
    // Pack Side Voltage
    ret |= max17320_read(dev, MAX17320_REG_PACK_SIDE_VOLTAGE, &read);
    dev->pack_side_voltage = read * PACK_VOLTAGE_LSB;
    CETI_LOG("MAX17320 Pack Side Voltage: %u V", dev->pack_side_voltage);
    
    return ret;
}

int max17320_get_temperature(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_TEMPERATURE, &read);
    dev->temperature = read * TEMPERATURE_LSB;
    CETI_LOG("MAX17320 Temperature: %u °C", dev->temperature);
    return ret;
}

int max17320_get_battery_current(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_BATT_CURRENT, &read);
    dev->battery_current = read * (CURRENT_LSB/R_SENSE_VAL) * 100;
    CETI_LOG("MAX17320 Battery Current: %u mA", dev->battery_current);
    return ret;
}

int max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_AVG_BATT_CURRENT, &read);
    dev->average_current = read * (CURRENT_LSB/R_SENSE_VAL) * 100;
    CETI_LOG("MAX17320 Avg Battery Current: %u mA", dev->average_current);
    return ret;
}

int max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_TIME_TO_EMPTY, &read);
    dev->time_to_empty = read * (TIME_LSB / SECOND_TO_HOUR);
    CETI_LOG("MAX17320 Time to Empty: %u hrs", dev->time_to_empty);
    return ret;
}
int max17320_get_time_to_full(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_TIME_TO_FULL, &read);
    dev->time_to_full = read * (TIME_LSB / SECOND_TO_HOUR);
    CETI_LOG("MAX17320 Time to Full: %u hrs", dev->time_to_full);
    return ret;
}

int max17320_set_alert_thresholds(MAX17320_HandleTypeDef *dev) {
    // TODO
}

int max17320_configure_cell_balancing(MAX17320_HandleTypeDef *dev) {
    // TODO
}

int max17320_get_remaining_writes(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    // Clear write protection
    int ret = max17320_clear_write_protection(dev);
    if (ret < 0)
    {
        return ret;
    }
    
    // Write to command register
    ret = max17320_write(dev, MAX17320_REG_COMMAND, DETERMINE_REMAINING_UPDATES);
    usleep(TRECALL);

    // Read from register that holds remaining writes
    ret |= max17320_read(dev, MAX17320_REG_REMAINING_WRITES, &read);

    // Decode remaining writes
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
    CETI_LOG("MAX17320 Remaining Writes: %u", dev->remaining_writes);
    return ret;
}
