
/*
 *      @file   max17320.c
 *      @brief  Implementation of MAX17320G2 Driver
 *      @author Saksham Ahuja
 *
 */

// Include files

#include "Sensor Inc/BMS.h"
#include "Sensor Inc/DataLogging.h"
#include "Lib Inc/state_machine.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c3;
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP data_log_event_flags_group;

MAX17320_HandleTypeDef bms = {0};

void bms_thread_entry(ULONG thread_input){

	HAL_StatusTypeDef ret = max17320_init(&bms, &hi2c3);
	if (ret != HAL_OK) {
		tx_event_flags_set(&state_machine_event_flags_group, STATE_CRIT_BATT_FLAG, TX_OR);
	}
	max17320_clear_write_protection(&bms);
	max17320_set_alert_thresholds(&bms);
	max17320_configure_cell_balancing(&bms);

	ULONG actual_flags;

	while (1) {

		max17320_get_remaining_capacity(&bms);
		max17320_get_state_of_charge(&bms);
		max17320_get_voltages(&bms);

		if (bms.status.protection_alert == 1) {
			max17320_get_faults(&bms);
			tx_event_flags_set(&state_machine_event_flags_group, STATE_CRIT_BATT_FLAG, TX_OR);
		}

		if (bms.cell_1_voltage < MAX17320_CRITICAL_BATT_VOLT_THR && bms.cell_2_voltage < MAX17320_CRITICAL_BATT_VOLT_THR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_CRIT_BATT_FLAG, TX_OR);
		}

		if (bms.cell_1_voltage < MAX17320_LOW_BATT_VOLT_THR && bms.cell_2_voltage < MAX17320_LOW_BATT_VOLT_THR) {
			tx_event_flags_set(&state_machine_event_flags_group, STATE_LOW_BATT_FLAG, TX_OR);
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

HAL_StatusTypeDef max17320_init(MAX17320_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c_device) {

	HAL_StatusTypeDef ret = HAL_ERROR;
    dev->i2c_handler = hi2c_device;

    // Check status
    ret = max17320_get_status(dev);

    return ret;
}

HAL_StatusTypeDef max17320_clear_write_protection(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_COMM_STAT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}


HAL_StatusTypeDef max17320_set_alert_thresholds(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};
	HAL_StatusTypeDef ret = HAL_ERROR;

	data_buf[0] = MAX17320_MIN_VOLTAGE_THR / VOLTAGE_ALT_LSB;
	data_buf[1] = MAX17320_MAX_VOLTAGE_THR / VOLTAGE_ALT_LSB;

	ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_VOLTAGE_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
	if (ret != HAL_OK) {
		return ret;
	}

	data_buf[0] = MAX17320_MIN_CURRENT_THR / CURRENT_ALT_LSB;
	data_buf[1] = MAX17320_MAX_CURRENT_THR / CURRENT_ALT_LSB;

	ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_CURRENT_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
	if (ret != HAL_OK) {
		return ret;
	}

	data_buf[0] = MAX17320_MIN_TEMP_THR / TEMP_ALT_LSB;
	data_buf[1] = MAX17320_MAX_TEMP_THR / TEMP_ALT_LSB;

	ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_TEMP_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_VOLTAGE_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);
	if (ret != HAL_OK) {
		return ret;
	}

	data_buf[0] = MAX17320_MIN_SOC_THR / SOC_ALT_LSB;
	data_buf[1] = MAX17320_MAX_SOC_THR / SOC_ALT_LSB;

	ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_SOC_ALT_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_set_ovp_thresholds(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	data_buf[0] |= MAX17320_LOWER_HALF_DEFAULT;
	data_buf[1] |= MAX17320_ROOM_CHARGE_THR;

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR_END, MAX17320_REG_OVP_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_configure_cell_balancing(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	data_buf[0] |= (MAX17320_CELL_R_BAL_THR & 0b00111) << 5;
	data_buf[1] |= (MAX17320_CELL_R_BAL_THR & 0b11000);
	data_buf[1] |= (MAX17320_CELL_BAL_THR << 2);

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR_END, MAX17320_REG_CELL_BAL_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef max17320_configure_thermistors(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    data_buf[0] |= (MAX17320_NUM_THERMISTORS << 2);
    data_buf[1] |= (MAX17320_THERMISTOR_TYPE << 3);

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(dev->i2c_handler, MAX17320_DEV_ADDR_END, MAX17320_REG_PACK_CFG, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

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

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, MAX17320_REG_PROT_ALERT, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

    dev->faults = __protAlertRegister_from_raw(TO_16_BIT(data_buf[0], data_buf[1]));

	return ret;
}

HAL_StatusTypeDef max17320_get_fet_status(MAX17320_HandleTypeDef *dev) {

    uint8_t data_buf[2] = {0};

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR_END, MAX17320_REG_PROT_CFG, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

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

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, 0x061, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf[0], 1, MAX17320_TIMEOUT);
	ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR, 0x061, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf[1], 1, MAX17320_TIMEOUT);

	dev->raw = TO_16_BIT(data_buf[0], data_buf[1]);

	return ret;
}

HAL_StatusTypeDef max17320_test2(MAX17320_HandleTypeDef *dev) {

	uint8_t data_buf[2] = {0};

	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->i2c_handler, MAX17320_DEV_ADDR_END, MAX17320_REG_CELL_BAL_THR, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&data_buf, 2, MAX17320_TIMEOUT);

	dev->raw = TO_16_BIT(data_buf[0], data_buf[1]);

	return ret;
}
