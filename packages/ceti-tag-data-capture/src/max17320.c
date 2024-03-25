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
    uint8_t data_buf[2] = {0};
    int ret = -1;
    int fd=i2cOpen(1,MAX17320_ADDR,0);
    if(fd < 0) {
        CETI_ERR("Failed to connect to the battery gauge");
    }
    else {
        uint16_t status_read = i2cReadWordData(fd, MAX17320_REG_STATUS);
        dev->status = __statusRegister_from_raw(status_read);
    }
    CETI_LOG("Read from MAX17320");
    CETI_LOG("Read %02x", dev->status);
    i2cClose(fd);
    return ret;
}