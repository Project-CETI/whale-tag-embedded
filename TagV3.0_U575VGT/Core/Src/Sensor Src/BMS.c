
/*
 *      @file   max17320.c
 *      @brief  Implementation of MAX17320G2 Driver
 *      @author Saksham Ahuja
 *
 */


#include "Sensor Inc/BMS.h"
#include "Sensor Inc/DataLogging.h"
#include "Lib Inc/state_machine.h"
#include "main.h"
#include "stm32u5xx_hal.h"

TX_EVENT_FLAGS_GROUP bms_event_flags_group;

extern I2C_HandleTypeDef hi2c3;
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

MAX17320_HandleTypeDef bms = {0};
uint8_t remaining_writes = 0;

void bms_thread_entry(ULONG thread_input) {

	tx_event_flags_create(&bms_event_flags_group, "BMS Event Flags Group");

	// initialize BMS
	HAL_StatusTypeDef ret = max17320_init(&bms, &hi2c3);
	if (ret != HAL_OK) {
		tx_event_flags_set(&state_machine_event_flags_group, STATE_CRIT_BATT_FLAG, TX_OR);
	}

	// configure bms settings
	max17320_clear_write_protection(&bms);
	//max17320_set_alert_thresholds(&bms);
	max17320_configure_cell_balancing(&bms);

	while (1) {

		ULONG actual_flags = 0;

		tx_event_flags_get(&state_machine_event_flags_group, STATE_BMS_NONVOLATILE_SETUP_FLAG | STATE_BMS_OPEN_MOSFET_FLAG | STATE_BMS_CLOSE_MOSFET_FLAG, TX_OR_CLEAR, &actual_flags, BMS_FLAG_TIMEOUT);
		if (actual_flags & STATE_BMS_NONVOLATILE_SETUP_FLAG) {
			remaining_writes = max17320_get_remaining_writes(&bms);
			if (remaining_writes <= BMS_WRITES_TOLERANCE) {

				// enable BMS fet control in nonvolatile memory
				//max17320_nonvolatile_write(&bms);
			}
			tx_event_flags_set(&bms_event_flags_group, BMS_OP_DONE_FLAG, TX_OR);
		}
		else if (actual_flags & STATE_BMS_CLOSE_MOSFET_FLAG) {
			max17320_close_fets(&bms);
			tx_event_flags_set(&bms_event_flags_group, BMS_OP_DONE_FLAG, TX_OR);
		}
		else if (actual_flags & STATE_BMS_OPEN_MOSFET_FLAG) {
			max17320_open_fets(&bms);
			tx_event_flags_set(&bms_event_flags_group, BMS_OP_DONE_FLAG, TX_OR);
		}

		// get battery statistics
		max17320_get_remaining_capacity(&bms);
		max17320_get_state_of_charge(&bms);
		max17320_get_voltages(&bms);
		max17320_get_temperature(&bms);
		max17320_get_battery_current(&bms);
		max17320_get_average_battery_current(&bms);
		max17320_get_time_to_empty(&bms);
		max17320_get_time_to_full(&bms);

		if (bms.status.protection_alert) {
			max17320_get_faults(&bms);
			max17320_clear_alerts(&bms);
		}

		if (bms.cell_1_voltage < MAX17320_LOW_BATT_VOLT_THR && bms.cell_2_voltage < MAX17320_LOW_BATT_VOLT_THR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_LOW_BATT_FLAG, TX_OR);
		}

		if (bms.cell_1_voltage < MAX17320_CRITICAL_BATT_VOLT_THR && bms.cell_2_voltage < MAX17320_CRITICAL_BATT_VOLT_THR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_CRIT_BATT_FLAG, TX_OR);
			max17320_close_fets(&bms);
		}

		tx_event_flags_get(&data_log_event_flags_group, DATA_LOG_COMPLETE_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
	}
}

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

static inline max17320_Reg_Faults __protAlertRegister_from_raw(uint16_t raw) {
	return (max17320_Reg_Faults) {
        .ldet_alert = _RSHIFT(raw, 0, 1),
	    .resd_fault_alert = _RSHIFT(raw, 1, 1),
		.overdischarge_current_alert = _RSHIFT(raw, 2, 1),
		.undervoltage_alert = _RSHIFT(raw, 3, 1),
		.too_hot_discharge_alert = _RSHIFT(raw, 4, 1),
	    .die_hot_alert = _RSHIFT(raw, 5, 1),
	    .permanent_fail_alert = _RSHIFT(raw, 6, 1),
	    .multicell_imbalance_alert = _RSHIFT(raw, 7, 1),
	    .prequal_timeout_alert = _RSHIFT(raw, 8, 1),
	    .queue_overflow_alert = _RSHIFT(raw, 9, 1),
		.overcharge_current_alert = _RSHIFT(raw, 10, 1),
	    .overvoltage_alert = _RSHIFT(raw, 11, 1),
	    .too_cold_charge_alert = _RSHIFT(raw, 12, 1),
	    .full_detect_alert = _RSHIFT(raw, 13, 1),
	    .too_hot_charge_alert = _RSHIFT(raw, 14, 1),
		.charge_watchdog_timer_alert = _RSHIFT(raw, 15, 1),
	};
}

static inline HAL_StatusTypeDef max17320_write(I2C_HandleTypeDef *hi2c, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout) {

	uint16_t DevAddress = 0;
	if (MemAddress > 0x0FF) {
		DevAddress = MAX17320_DEV_ADDR_EXT;
	}
	else {
		DevAddress = MAX17320_DEV_ADDR;
	}

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, Timeout);
	return ret;
}

static inline HAL_StatusTypeDef max17320_read(I2C_HandleTypeDef *hi2c, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout) {

	uint16_t DevAddress = 0;
	if (MemAddress > 0x0FF) {
		DevAddress = MAX17320_DEV_ADDR_EXT;
	}
	else {
		DevAddress = MAX17320_DEV_ADDR;
	}

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, Timeout);
	return ret;
}

// WARNING: MAXIMUM ONLY 7 NON-VOLATILE MEMORY WRITES ARE ALLOWED FOR BMS
// WaRNING: NONVOLATILE WRITES COPY VALUES FROM ADDRESSES 0x180 to 0x1EF IN SHADOW RAM TO NON-VOLATILE MEMORY
HAL_StatusTypeDef max17320_nonvolatile_write(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0b100};
	HAL_StatusTypeDef ret;

	uint32_t start_time = HAL_GetTick();
	while (data_buf[0] & 0b100) {

		// unlock write protection
		ret |= max17320_clear_write_protection(dev);

		data_buf[0] = 0;
		data_buf[1] = 0;

		// write data to shadow RAM
		ret |= max17320_configure_fets(dev);
		ret |= max17320_configure_cell_balancing(dev);

		// clear NVError bit in Comm Stat register
		ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

		// nonvolatile block copy command
		data_buf[0] = 0x04;
		data_buf[1] = 0xE9;
		ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CMD, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

		// wait for copy to complete
		HAL_Delay(BLOCK_PROG_TIME_MS);

		// check for NVError
	    ret |= HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

		if ((HAL_GetTick() - start_time) > MAX17320_CHECK_TIMEOUT) {
			return HAL_BUSY;
		}
	}

	// full reset command
	data_buf[0] = 0x0F;
	data_buf[1] = 0;
	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CMD, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	// wait for reset
	HAL_Delay(RESET_DELAY_MS);

	// unlock write protection after reset
	ret |= max17320_clear_write_protection(dev);

	// reset firmware
	data_buf[0] = 0;
	data_buf[1] = 0x80;
	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	// wait for POR_CMD bit to be cleared for POR sequence to complete
	ret |= HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	start_time = HAL_GetTick();
	while (data_buf[1] & 0b10000000) {
		ret |= HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
		if ((HAL_GetTick() - start_time) > MAX17320_CHECK_TIMEOUT) {
			return HAL_BUSY;
		}
	}

	// set write protection
	ret |= max17320_set_write_protection(dev);

	return ret;
}

uint8_t max17320_get_remaining_writes(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	// unlock write protection
	HAL_StatusTypeDef ret = max17320_clear_write_protection(dev);

	// send command to update remaining writes
	data_buf[0] = 0x9B;
	data_buf[1] = 0xE2;
	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CMD, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	HAL_Delay(RECALL_TIME_MS);

	// read number of remaining writes
	ret |= HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, MAX17320_REG_REMAIN_WRITES, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 1, MAX17320_TIMEOUT);

	// set write protection
	ret |= max17320_set_write_protection(dev);

	return data_buf[0];
}

HAL_StatusTypeDef max17320_full_reset(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	// full reset command
	data_buf[0] = 0x0F;
	data_buf[1] = 0;
	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CMD, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	// wait for reset
	HAL_Delay(RESET_DELAY_MS);

	// unlock write protection after reset
	ret |= max17320_clear_write_protection(dev);

	// reset firmware
	data_buf[0] = 0;
	data_buf[1] = 0x80;
	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	// wait for POR_CMD bit to be cleared for POR sequence to complete
	ret |= HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	uint32_t start_time = HAL_GetTick();
	while (data_buf[1] & 0b10000000) {
		ret |= HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
		if ((HAL_GetTick() - start_time) > MAX17320_CHECK_TIMEOUT) {
			return HAL_BUSY;
		}
	}

	// set write protection
	ret |= max17320_set_write_protection(dev);

	return ret;
}

HAL_StatusTypeDef max17320_init(MAX17320_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c_device) {

    dev->i2c_handler = hi2c_device;

    // Check status
    HAL_StatusTypeDef ret = max17320_get_status(dev);

    return ret;
}

HAL_StatusTypeDef max17320_clear_write_protection(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_set_write_protection(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};
	data_buf[0] = 0xF9;

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_set_alert_thresholds(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	data_buf[0] = MAX17320_MIN_VOLTAGE_THR / VOLTAGE_ALT_LSB;
	data_buf[1] = MAX17320_MAX_VOLTAGE_THR / VOLTAGE_ALT_LSB;

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_VOLTAGE_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	data_buf[0] = MAX17320_MIN_CURRENT_THR / CURRENT_ALT_LSB;
	data_buf[1] = MAX17320_MAX_CURRENT_THR / CURRENT_ALT_LSB;

	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CURRENT_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	data_buf[0] = MAX17320_MIN_TEMP_THR / TEMP_ALT_LSB;
	data_buf[1] = MAX17320_MAX_TEMP_THR / TEMP_ALT_LSB;

	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_TEMP_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	data_buf[0] = MAX17320_MIN_SOC_THR / SOC_ALT_LSB;
	data_buf[1] = MAX17320_MAX_SOC_THR / SOC_ALT_LSB;

	ret |= HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_SOC_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_configure_cell_balancing(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	data_buf[0] |= (MAX17320_CELL_R_BAL_THR & 0b00111) << 5;
	data_buf[1] |= (MAX17320_CELL_R_BAL_THR & 0b11000);
	data_buf[1] |= (MAX17320_CELL_BAL_THR << 2);

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, MAX17320_REG_CELL_BAL_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_configure_thermistors(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    data_buf[0] |= (MAX17320_NUM_THERMISTORS << 2);
    data_buf[1] |= (MAX17320_THERMISTOR_TYPE << 3);

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, MAX17320_REG_PACK_CFG, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_configure_fets(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};
	data_buf[1] = (MAX17320_PROT_CFG_DEFAULT | MAX17320_ENABLE_FET_OVERRIDE);

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, MAX17320_REG_PROT_CFG, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

    return ret;
}

HAL_StatusTypeDef max17320_close_fets(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};
    data_buf[1] = (MAX17320_FET_CHGOFF | MAX17320_FET_DISOFF);

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_open_fets(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_start_charge(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};
    data_buf[1] = MAX17320_FET_DISOFF;

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_stop_charge(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};
    data_buf[1] = MAX17320_FET_CHGOFF;

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_clear_alerts(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    // clear ProtAlrt register
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_PROT_ALRT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

    // clear status register
    ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_STATUS, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_get_status(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_STATUS, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

    dev->status = __statusRegister_from_raw(TO_16_BIT(data_buf[0], data_buf[1]));

	return ret;
}

HAL_StatusTypeDef max17320_get_faults(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_PROT_ALRT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

    dev->faults = __protAlertRegister_from_raw(TO_16_BIT(data_buf[0], data_buf[1]));

	return ret;
}

//double check
HAL_StatusTypeDef max17320_get_fet_status(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, MAX17320_REG_PROT_CFG2, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

    dev->raw = TO_16_BIT(data_buf[0], data_buf[1]);

	return ret;
}

HAL_StatusTypeDef max17320_get_remaining_capacity(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_REP_CAPACITY, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->remaining_capacity = TO_16_BIT(data_buf[0], data_buf[1]) * CAPACITY_LSB/R_SENSE_VAL;

	return ret;
}

HAL_StatusTypeDef max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_REP_SOC, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->state_of_charge = TO_16_BIT(data_buf[0], data_buf[1]) * PERCENTAGE_LSB;

	return ret;
}

HAL_StatusTypeDef max17320_get_voltages(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CELL1_VOLTAGE, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->cell_1_voltage = TO_16_BIT(data_buf[0], data_buf[1]) * CELL_VOLTAGE_LSB;

	ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CELL2_VOLTAGE, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->cell_2_voltage = TO_16_BIT(data_buf[0], data_buf[1]) * CELL_VOLTAGE_LSB;

	ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_TOTAL_BAT_VOLTAGE, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->total_battery_voltage = TO_16_BIT(data_buf[0], data_buf[1]) * PACK_VOLTAGE_LSB;

	ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_PACK_SIDE_VOLTAGE, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->pack_side_voltage = TO_16_BIT(data_buf[0], data_buf[1]) * PACK_VOLTAGE_LSB;

	ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_OC_VOLTAGE, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->total_battery_voltage = TO_16_BIT(data_buf[0], data_buf[1]) * CELL_VOLTAGE_LSB;

 	return ret;
}

HAL_StatusTypeDef max17320_get_temperature(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_TEMPERATURE, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->temperature = TO_16_BIT(data_buf[0], data_buf[1]) * TEMPERATURE_LSB;

	return ret;
}

// Reads the instantaneous current of the battery
HAL_StatusTypeDef max17320_get_battery_current(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_BATT_CURRENT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	int16_t x = TO_16_BIT(data_buf[0], data_buf[1]);

	dev->battery_current = x * CURRENT_LSB/R_SENSE_VAL;

	return ret;
}

HAL_StatusTypeDef max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_AVG_BATT_CURRENT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	int16_t x = TO_16_BIT(data_buf[0], data_buf[1]);

	dev->average_current = x * CURRENT_LSB/R_SENSE_VAL;

	return ret;
}

HAL_StatusTypeDef max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_TIME_TO_EMPTY, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->time_to_empty = TO_16_BIT(data_buf[0], data_buf[1]) * TIME_LSB/SECOND_TO_HOUR;

	return ret;
}

HAL_StatusTypeDef max17320_get_time_to_full(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_TIME_TO_FULL, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->time_to_full = TO_16_BIT(data_buf[0], data_buf[1]) * TIME_LSB/SECOND_TO_HOUR;

	return ret;
}

HAL_StatusTypeDef max17320_test(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, MAX17320_REG_PROT_CFG, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->raw = TO_16_BIT(data_buf[0], data_buf[1]);

	return ret;
}

HAL_StatusTypeDef max17320_test2(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->raw = TO_16_BIT(data_buf[0], data_buf[1]);

	return ret;
}

HAL_StatusTypeDef max17320_test3(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR_EXT, 0x1f1, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->raw = TO_16_BIT(data_buf[0], data_buf[1]);

	return ret;
}
