//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Saksham Ahuja, Matt Cummings, Shanaya Barretto, Michael Salino-Hugg
//-----------------------------------------------------------------------------

#include "max17320.h"

// Global variables
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
        CETI_ERR("Failed to connect to the MAX17320 battery gauge");
        ret = -1;
    }
    else {
        ret = i2cWriteWordData(fd, memory, data);
    }
    i2cClose(fd);
    // TODO: Add error checking but note that there are some exceptions (clearing write)
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
        CETI_ERR("Failed to connect to the MAX17320 battery gauge");
        ret = -1;
    }
    else {
        *storage = i2cReadWordData(fd, memory);
    }
    i2cClose(fd);
    return ret;
}

int max17320_init(MAX17320_HandleTypeDef *dev) {
    int ret = -1;
    ret = max17320_clear_write_protection(dev);
    ret |= max17320_nonvolatile_write(dev);
    return ret;
}

int max17320_clear_write_protection(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;

    uint8_t counter = 2;
    while (counter > 0)
    {
        ret = max17320_write(dev, MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT);
        usleep(TRECALL_US);
        counter--;
    }
    ret |= max17320_read(dev, MAX17320_REG_COMM_STAT, &read);
    if (read == CLEARED_WRITE_PROT || read == CLEAR_WRITE_PROT)
    {
        return ret;
    }
    CETI_ERR("MAX17320 Clearing write protection failed, CommStat: 0x%.4x", read);
    ret = -1;
    return ret;
}

int max17320_lock_write_protection(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    uint8_t counter = 2;
    int ret = 0;
    while (counter > 0)
    {
        ret = max17320_write(dev, MAX17320_REG_COMM_STAT, LOCKED_WRITE_PROT);
        usleep(TRECALL_US);
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
    if (ret >= 0) {
        dev->status = __statusRegister_from_raw(read);
        CETI_LOG("MAX17320 Status: 0x%.4x", read);
    }
    return ret;
}

int max17320_get_remaining_capacity(MAX17320_HandleTypeDef* dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_REP_CAPACITY, &read);
    if (ret >= 0) {
        dev->remaining_capacity = read * (CAPACITY_LSB / R_SENSE_VAL);
        CETI_LOG("MAX17320 Remaining Capacity: %.2f mAh", dev->remaining_capacity);
    }
    return ret;
}

int max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_REP_SOC, &read);
    if (ret >= 0) {
        dev->state_of_charge = read * PERCENTAGE_LSB;
        CETI_LOG("MAX17320 State of Charge: %.2f %%", dev->state_of_charge);
    }
    return ret;
}

int max17320_get_voltages(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    // Cell 1 Voltage
    int ret = max17320_read(dev, MAX17320_REG_CELL1_VOLTAGE, &read);
    if (ret >= 0) {
        dev->cell_1_voltage = read * CELL_VOLTAGE_LSB;
        CETI_LOG("MAX17320 Cell 1 Voltage: %.2f V", dev->cell_1_voltage);
    }

    // Cell 2 Voltage
    ret |= max17320_read(dev, MAX17320_REG_CELL2_VOLTAGE, &read);
    if (ret >= 0) {
        dev->cell_2_voltage = read * CELL_VOLTAGE_LSB;
        CETI_LOG("MAX17320 Cell 2 Voltage: %.2f V", dev->cell_2_voltage);
    }

    // Total Battery Voltage
    ret |= max17320_read(dev, MAX17320_REG_TOTAL_BAT_VOLTAGE, &read);
    if (ret >= 0) {
        dev->total_battery_voltage = read * PACK_VOLTAGE_LSB;
        CETI_LOG("MAX17320 Total Battery Voltage: %.2f V", dev->total_battery_voltage);
    }
    
    // Pack Side Voltage
    ret |= max17320_read(dev, MAX17320_REG_PACK_SIDE_VOLTAGE, &read);
    if (ret >= 0) {
        dev->pack_side_voltage = read * PACK_VOLTAGE_LSB;
        CETI_LOG("MAX17320 Pack Side Voltage: %.2f V", dev->pack_side_voltage);
    }
    
    return ret;
}

int max17320_get_temperature(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_TEMPERATURE, &read);
    if (ret >= 0) {
        dev->temperature = read * TEMPERATURE_LSB;
        CETI_LOG("MAX17320 Thermistor Temperature: %.2f °C", dev->temperature);
    }
    ret |= max17320_read(dev, MAX17320_REG_DIETEMP, &read);
    if (ret >= 0) {
        dev->die_temperature = read * TEMPERATURE_LSB;
        CETI_LOG("MAX17320 Die Temperature: %.2f °C", dev->die_temperature);
    }
    return ret;
}

int max17320_get_battery_current(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_BATT_CURRENT, &read);
    if (ret >= 0) {
        dev->battery_current = (read * (CURRENT_LSB/R_SENSE_VAL)) / 1000;
        CETI_LOG("MAX17320 Battery Current: %.2f mA", dev->battery_current);
    }
    return ret;
}

int max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev) {
    int16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_AVG_BATT_CURRENT, &read);
    dev->average_current = (read * (CURRENT_LSB/R_SENSE_VAL)) / 1000;
    CETI_LOG("MAX17320 Avg Battery Current: %.2f mA", dev->average_current);
    return ret;
}

int max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_TIME_TO_EMPTY, &read);
    if (ret >= 0) {
        dev->time_to_empty = read * (TIME_LSB / SECOND_TO_HOUR);
        CETI_LOG("MAX17320 Time to Empty: %.2f hrs", dev->time_to_empty);
    }
    return ret;
}
int max17320_get_time_to_full(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(dev, MAX17320_REG_TIME_TO_FULL, &read);
    if (ret >= 0) {
        dev->time_to_full = read * (TIME_LSB / SECOND_TO_HOUR);
        CETI_LOG("MAX17320 Time to Full: %.2f hrs", dev->time_to_full);
    }
    return ret;
}

static inline int max17320_verify_nv_write(MAX17320_HandleTypeDef *dev) {
    // Verify whether each non volatile write has been completed
    uint16_t read = 0;
    int ret = 0;

    uint16_t registers[] = {MAX17320_REG_NPACKCFG, MAX17320_REG_NNVCFG0, MAX17320_REG_NNVCFG1, MAX17320_REG_NNVCFG2, MAX17320_REG_NUVPRTTH, MAX17320_REG_NTPRTTH1, MAX17320_REG_NIPRTTH1,
                            MAX17320_REG_NBALTH, MAX17320_REG_NPROTMISCTH, MAX17320_REG_NPROTCFG, MAX17320_REG_NJEITAV, MAX17320_REG_NOVPRTTH, MAX17320_REG_NDELAYCFG, MAX17320_REG_NODSCCFG,
                            MAX17320_REG_NCONFIG, MAX17320_REG_NTHERMCFG, MAX17320_REG_NVEMPTY, MAX17320_REG_NFULLSOCTHR};
    uint16_t data[] = {MAX17320_VAL_NPACKCFG, MAX17320_VAL_NNVCFG0, MAX17320_VAL_NNVCFG1, MAX17320_VAL_NNVCFG2, MAX17320_VAL_NUVPRTTH, MAX17320_VAL_NTPRTTH1, MAX17320_VAL_NIPRTTH1,
                       MAX17320_VAL_NBALTH, MAX17320_VAL_NPROTMISCTH, MAX17320_VAL_NPROTCFG, MAX17320_VAL_NJEITAV, MAX17320_VAL_NOVPRTTH, MAX17320_VAL_NDELAYCFG, MAX17320_VAL_NODSCCFG,
                       MAX17320_VAL_NCONFIG, MAX17320_VAL_NTHERMCFG, MAX17320_VAL_NVEMPTY, MAX17320_VAL_NFULLSOCTHR};
    
    for (int i = 0; i < 18; i++) {
        ret |= max17320_read(dev, registers[i], &read);
        if (read != data[i]){
            ret |= 1;
        }
    }
    return ret;
}

static inline int max17320_setup_nv_write(MAX17320_HandleTypeDef *dev) {
    int ret = 0;
    // Write memory locations to new values
    ret = max17320_write(dev, MAX17320_REG_NPACKCFG, MAX17320_VAL_NPACKCFG);
    ret |= max17320_write(dev, MAX17320_REG_NNVCFG0, MAX17320_VAL_NNVCFG0);
    ret |= max17320_write(dev, MAX17320_REG_NNVCFG1, MAX17320_VAL_NNVCFG1);
    ret |= max17320_write(dev, MAX17320_REG_NNVCFG2, MAX17320_VAL_NNVCFG2);
    ret |= max17320_write(dev, MAX17320_REG_NUVPRTTH, MAX17320_VAL_NUVPRTTH);
    ret |= max17320_write(dev, MAX17320_REG_NTPRTTH1, MAX17320_VAL_NTPRTTH1);
    ret |= max17320_write(dev, MAX17320_REG_NIPRTTH1, MAX17320_VAL_NIPRTTH1);
    ret |= max17320_write(dev, MAX17320_REG_NBALTH, MAX17320_VAL_NBALTH);
    ret |= max17320_write(dev, MAX17320_REG_NPROTMISCTH, MAX17320_VAL_NPROTMISCTH);
    ret |= max17320_write(dev, MAX17320_REG_NPROTCFG, MAX17320_VAL_NPROTCFG);
    ret |= max17320_write(dev, MAX17320_REG_NJEITAV, MAX17320_VAL_NJEITAV);
    ret |= max17320_write(dev, MAX17320_REG_NOVPRTTH, MAX17320_VAL_NOVPRTTH);
    ret |= max17320_write(dev, MAX17320_REG_NDELAYCFG, MAX17320_VAL_NDELAYCFG);
    ret |= max17320_write(dev, MAX17320_REG_NODSCCFG, MAX17320_VAL_NODSCCFG);
    ret |= max17320_write(dev, MAX17320_REG_NCONFIG, MAX17320_VAL_NCONFIG);
    ret |= max17320_write(dev, MAX17320_REG_NTHERMCFG, MAX17320_VAL_NTHERMCFG);
    ret |= max17320_write(dev, MAX17320_REG_NVEMPTY, MAX17320_VAL_NVEMPTY);
    ret |= max17320_write(dev, MAX17320_REG_NFULLSOCTHR, MAX17320_VAL_NFULLSOCTHR);
    if (ret < 0)
    {
        CETI_ERR("MAX17320 Error setting non-volatile registers");
        return ret;
    }

    // Clear CommStat.NVError bit
    ret |= max17320_write(dev, MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT);
    if (max17320_verify_nv_write(dev) == 0) {
        CETI_LOG("MAX17320: Write successful, initiating block copy");
        // Initiate a block copy
        ret |= max17320_write(dev, MAX17320_REG_COMMAND, INITIATE_BLOCK_COPY);
        // TODO: find right value
        sleep(1);
        return ret;
    }
    CETI_LOG("MAX17320: Write has errors, not initiating block copy");
    ret = -1;
    return ret;
}

int max17320_reset(MAX17320_HandleTypeDef *dev) {
    // Performs full reset
    int ret = max17320_write(dev, MAX17320_REG_COMMAND, MAX17320_RESET);
    usleep(10000);
    return ret;
}

int max17320_nonvolatile_write(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_verify_nv_write(dev);
    if (ret == 0) {
        CETI_LOG("MAX17320 Nonvolatile settings already written");
        return ret;
    }
    CETI_LOG("MAX17320 Nonvolatile settings will be re-written");
    
    ret |= max17320_get_remaining_writes(dev);
    if (ret < 0)
    {
        return ret;
    }
    if (dev->remaining_writes < 3) {
        ret = -1;
        CETI_LOG("MAX17320 Remaining nonvolatile writes less than 3, not rewriting");
        return ret;
    }
    
    // Clear write protection
    uint16_t read = 0;
    ret |= max17320_clear_write_protection(dev);
    if (ret < 0)
    {
        return ret;
    }

    // Write all registers, clear error bit, and send block copy command
    ret |= max17320_setup_nv_write(dev);
    if (ret < 0)
    {
        CETI_LOG("MAX17320 Something went wrong with register write");
        return ret;
    }
    ret |= max17320_read(dev, MAX17320_REG_COMM_STAT, &read);
    while((read & 0x0004) == 0x0004) {
        ret |= max17320_read(dev, MAX17320_REG_COMM_STAT, &read);
        CETI_LOG("MAX17320 waiting for bit to clear");
        // TODO: Determine proper way to exit this loop
    }

    if (ret == 0) {
        CETI_LOG("MAX17320 Block copy complete, resetting system...");
        // Send full reset command to the IC
        ret |= max17320_reset(dev);
        // Reset firmware
        ret |= max17320_clear_write_protection(dev);
        ret |= max17320_write(dev, MAX17320_REG_CONFIG2, MAX17320_RESET_FW);
        ret |= max17320_read(dev, MAX17320_REG_CONFIG2, &read);
        // Wait for POR_CMD bit to clear
        while ((read & 0x4000) == 0x4000) {
            ret |= max17320_read(dev, MAX17320_REG_CONFIG2, &read);
        }
        // Lock write protection
        CETI_LOG("MAX17320 Non-volatile settings written");
        // Commented out for now, to not affect previous settings for FETS when unlocking
        // ret |= max17320_lock_write_protection(dev);
    }

    return ret;
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
    usleep(TRECALL_US);

    // Read from register that holds remaining writes
    ret |= max17320_read(dev, MAX17320_REG_REMAINING_WRITES, &read);
    CETI_LOG("MAX17320 Remaining Writes Register Read: 0x%.4x", read);

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
    // Commented out for now, to not affect previous settings for FETS when unlocking
    // ret |= max17320_lock_write_protection(dev);
    return ret;
}

int max17320_enable_charging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_write(dev, MAX17320_REG_COMM_STAT, CHARGE_ON);
    if (ret >= 0) {
        CETI_LOG("MAX17320 Charge FET Enabled");
    }
    else {
        CETI_LOG("MAX17320 Charge FET could not be enabled");
    }
    return ret;
}

int max17320_enable_discharging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_write(dev, MAX17320_REG_COMM_STAT, DISCHARGE_ON);
    if (ret >= 0) {
        CETI_LOG("MAX17320 Discharge FET Enabled");
    }
    else {
        CETI_LOG("MAX17320 Discharge FET could not be enabled");
    }
    return ret;
}

int max17320_disable_charging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_write(dev, MAX17320_REG_COMM_STAT, CHARGE_OFF);
    if (ret >= 0) {
        CETI_LOG("MAX17320 Charge FET Disabled");
    }
    else {
        CETI_LOG("MAX17320 Charge FET could not be disabled");
    }
    return ret;
}

int max17320_disable_discharging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_write(dev, MAX17320_REG_COMM_STAT, DISCHARGE_OFF);
    if (ret >= 0) {
        CETI_LOG("MAX17320 Discharge FET Disabled");
    }
    else {
        CETI_LOG("MAX17320 Discharge FET could not be disabled");
    }
    return ret;
}
