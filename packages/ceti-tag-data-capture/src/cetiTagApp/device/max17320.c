//-----------------------------------------------------------------------------
// Project:      // CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Saksham Ahuja, Matt Cummings, Shanaya Barretto, Michael Salino-Hugg
//-----------------------------------------------------------------------------

#include "max17320.h"

#include "i2c.h"

#include <pigpio.h>
#include <unistd.h> // for usleep

static inline double __current_mA_from_raw(uint16_t raw, double r_sense_mOhm) {
    double current_uv = ((double)((int16_t)raw)) * CURRENT_LSB_uV;
    return current_uv / r_sense_mOhm;
}

static inline double __raw_to_capacity_mAh(uint16_t raw, double r_sense_mOhm) {
    return ((double)raw) * 0.005 / r_sense_mOhm;
}

static inline double __raw_to_percentage(uint16_t raw) {
    return ((double)raw) / 256.0;
}

static inline double __raw_to_voltage_v(uint16_t raw) {
    return ((double)raw) * 0.000078125;
}

static inline double __raw_to_current_mA(uint16_t raw, double r_sense_mOhm) {
    return ((double)((int16_t)raw)) * 0.15625 / r_sense_mOhm;
}

static inline double __raw_to_temperature_c(uint16_t raw) {
    return ((double)((int16_t)raw)) / 256.0;
}

static inline double __raw_to_time_s(uint16_t raw) {
    return ((double)raw) * 5.625;
}

WTResult max17320_read(uint16_t memory, uint16_t *storage) {
    uint16_t addr = BMS_I2C_DEV_ADDR_LOWER;
    if (memory > 0xFF) {
        memory = memory & 0xFF;
        addr = BMS_I2C_DEV_ADDR_UPPER;
    }
    int fd = PI_TRY(WT_DEV_BMS, i2cOpen(BMS_I2C_BUS, addr, 0));
    if (storage != NULL) {
        *storage = PI_TRY(WT_DEV_BMS, i2cReadWordData(fd, memory));
    }
    i2cClose(fd);
    return WT_OK;
}

WTResult max17320_write(uint16_t memory, uint16_t data) {
    uint16_t addr = BMS_I2C_DEV_ADDR_LOWER;
    if (memory > 0xFF) {
        memory = memory & 0xFF;
        addr = BMS_I2C_DEV_ADDR_UPPER;
    }
    int fd = PI_TRY(WT_DEV_BMS, i2cOpen(BMS_I2C_BUS, addr, 0));
    PI_TRY(WT_DEV_BMS, i2cWriteWordData(fd, memory, data), i2cClose(fd));
    i2cClose(fd);
    return WT_OK;
}

WTResult max17320_clear_write_protection(void) {
    uint16_t read = 0;

    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT));
    usleep(TRECALL_US);
    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT));
    usleep(TRECALL_US);
    WT_TRY(max17320_read(MAX17320_REG_COMM_STAT, &read));

    if (read != CLEARED_WRITE_PROT && read != CLEAR_WRITE_PROT) {
        return WT_RESULT(WT_DEV_BMS, WT_ERR_BMS_WRITE_PROT_DISABLE_FAIL);
    }
    return WT_OK;
}

WTResult max17320_get_remaining_capacity_mAh(double *pCapacity_mAh) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_REP_CAPACITY, &read));
    if (pCapacity_mAh != NULL) {
        *pCapacity_mAh = __raw_to_capacity_mAh(read, R_SENSE_VAL);
    }
    return WT_OK;
}

WTResult max17320_get_state_of_charge(double *pSoc) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_REP_SOC, &read));
    if (pSoc != NULL) {
        *pSoc = __raw_to_percentage(read);
    }
    return WT_OK;
}

WTResult max17320_get_cell_voltage_v(int cell_index, double *vCells_v) {
    if (cell_index >= MAX17320_CELL_COUNT) {
        return WT_RESULT(WT_DEV_BMS, WT_ERR_BMS_BAD_CELL_INDEX);
    }

    uint16_t read = 0;
    WT_TRY(max17320_read(0xD8 - cell_index, &read));
    if (vCells_v != NULL) {
        *vCells_v = __raw_to_voltage_v(read);
    }
    return WT_OK;
}

WTResult max17320_get_cell_temperature_c(int cell_index, double *tCells_c) {
    if (cell_index >= MAX17320_CELL_COUNT) {
        return WT_RESULT(WT_DEV_BMS, WT_ERR_BMS_BAD_CELL_INDEX);
    }
    uint16_t raw = 0;
    WT_TRY(max17320_read(0x13A - cell_index, &raw));
    if (tCells_c != NULL) {
        *tCells_c = __raw_to_temperature_c(raw);
    }
    return WT_OK;
}

WTResult max17320_get_die_temperature_c(double *pDieTemp_c) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_TEMPERATURE, &read));
    if (pDieTemp_c != NULL) {
        *pDieTemp_c = __raw_to_temperature_c(read);
    }
    return WT_OK;
}

WTResult max17320_get_current_mA(double *pCurrent_mA) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_BATT_CURRENT, &read));
    if (pCurrent_mA != NULL) {
        *pCurrent_mA = __current_mA_from_raw(read, (R_SENSE_VAL * 1000.0));
    }
    return WT_OK;
}

WTResult max17320_get_average_current_mA(double *pAvgI_mA) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_AVG_BATT_CURRENT, &read));
    if (pAvgI_mA != NULL) {
        *pAvgI_mA = __current_mA_from_raw(read, (R_SENSE_VAL * 1000.0));
    }
    return WT_OK;
}

WTResult max17320_get_time_to_empty_s(double *pTimeToEmpty_s) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_TIME_TO_EMPTY, &read));
    if (pTimeToEmpty_s != NULL) {
        *pTimeToEmpty_s = __raw_to_time_s(read);
    }
    return WT_OK;
}
WTResult max17320_get_time_to_full_s(double *pTimeToFull_s) {
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_TIME_TO_FULL, &read));
    if (pTimeToFull_s != NULL) {
        *pTimeToFull_s = __raw_to_time_s(read);
    }
    return WT_OK;
}

WTResult max17320_swap_shadow_ram(void) {
    // Clear CommStat.NVError bit
    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT));

    // Initiate a block copy
    WT_TRY(max17320_write(MAX17320_REG_COMMAND, INITIATE_BLOCK_COPY));
    // TODO: find right value
    usleep(TRECALL_US);
    // wait for block copy to complete
    uint16_t read = 0;
    do {
        WT_TRY(max17320_read(MAX17320_REG_COMM_STAT, &read));
        usleep(TRECALL_US);
        // ToDo: add timeout
    } while (read & 0x0004);

    WT_TRY(max17320_reset());
    return WT_OK;
}

WTResult max17320_gauge_reset(void) {
    WT_TRY(max17320_clear_write_protection());
    // Reset firmware
    WT_TRY(max17320_write(MAX17320_REG_CONFIG2, MAX17320_RESET_FW));
    uint16_t read = 0;
    do {
        WT_TRY(max17320_read(MAX17320_REG_CONFIG2, &read));
        usleep(TRECALL_US);
    } while ((read & 0x8000)); // Wait for POR_CMD bit to clear
    return WT_OK;
}

WTResult max17320_reset(void) {
    // Performs full reset
    WT_TRY(max17320_clear_write_protection());
    WT_TRY(max17320_write(MAX17320_REG_COMMAND, MAX17320_RESET));
    usleep(10000);
    WT_TRY(max17320_gauge_reset());
    return WT_OK;
}

WTResult max17320_get_remaining_writes(uint8_t *pRemainingWrites) {
    // Clear write protection
    WT_TRY(max17320_clear_write_protection());

    // Write to command register
    WT_TRY(max17320_write(MAX17320_REG_COMMAND, DETERMINE_REMAINING_UPDATES));
    usleep(TRECALL_US);

    // Read from register that holds remaining writes
    uint16_t read = 0;
    WT_TRY(max17320_read(MAX17320_REG_REMAINING_WRITES, &read));

    // Decode remaining writes
    uint8_t first_byte = (read >> 8) & 0xff;
    uint8_t last_byte = read & 0xff;
    uint8_t decoded = first_byte | last_byte;
    uint8_t count = 0;
    while (decoded > 0) {
        if (decoded & 1) {
            count++;
        }
        decoded = decoded >> 1;
    }
    if (pRemainingWrites != NULL) {
        *pRemainingWrites = (8 - count);
    }
    return WT_OK;
}

WTResult max17320_enable_charging(void) {
    uint16_t value = 0;
    WT_TRY(max17320_read(MAX17320_REG_COMM_STAT, &value));
    value &= ~CHARGE_OFF;
    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, value));
    return WT_OK;
}

WTResult max17320_enable_discharging(void) {
    uint16_t value = 0;
    WT_TRY(max17320_read(MAX17320_REG_COMM_STAT, &value));
    value &= ~DISCHARGE_OFF;
    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, value));
    return WT_OK;
}

WTResult max17320_disable_charging(void) {
    uint16_t value = 0;
    WT_TRY(max17320_read(MAX17320_REG_COMM_STAT, &value));
    value |= CHARGE_OFF;
    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, value));
    return WT_OK;
}

WTResult max17320_disable_discharging(void) {
    uint16_t value = 0;
    WT_TRY(max17320_read(MAX17320_REG_COMM_STAT, &value));
    value |= DISCHARGE_OFF;
    WT_TRY(max17320_write(MAX17320_REG_COMM_STAT, DISCHARGE_OFF));
    return WT_OK;
}

//-----------------------------------------------------------------------------
WTResult max17320_init(void) {

    int ret = 0;

    // TODO MRC
    // Don't change nonvolatile BMS chip entries each time the device is booted
    // These should only be set once at the factory only 7 writes are available
    // Instead, load the values needed directly at run time while we get this running.
    // Once design is stable this can be put back or a special BMS
    // configuration procedure can be created.  During development we
    // will probably need to change a lot of these values.
    // So, commenting the instruction to write NV
    //
    // The chip is designed to work more or less independently of a host, but
    // in our case this really isn't necessary. We can save settings in our
    // code.
    // Having said that, having built-in NV values is a great feature because
    // the batteries will be protected even if the softwware fails to run OK
    // TODO MRC work out best way to manage settings stored on the BMS chip
    // * Initialize at run time anyhow using desired values and,
    // * Configure once when Tag is manufactured
    // Belts AND suspenders

    //-----------------------------------------------------------------------------
    //  ret |= max17320_nonvolatile_write(dev);
    //-----------------------------------------------------------------------------

    //  Special for development, write shadow RAM for now at run time with
    //  key registers to get the chip running for hardware integration
    //

    WT_TRY(max17320_clear_write_protection());                            // Need to allow writes
    WT_TRY(max17320_write(MAX17320_REG_DESIGN_CAP, DESIGN_CAP));          // establish design capacity (5000 mAH)
    WT_TRY(max17320_write(MAX17320_REG_NIPRTTH1, MAX17320_VAL_NIPRTTH1)); // set new slow current limits for development (2A)
    WT_TRY(max17320_write(MAX17320_REG_NBALTH, MAX17320_VAL_NBALTH));     // disable imbalance charge term for development

    // TODO relock the protection for final design

    return ret;
}