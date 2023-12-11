/*
 * LightSensor.h
 *
 *  Created on: Feb. 9, 2023
 *      Author: Amjad Halis
 *      Sensor: LTR-329ALS-01_DS_V1
 *      Datasheet: optoelectronics.liteon.com/upload/download/DS86-2014-0006/LTR-329ALS-01_DS_V1.pdf
 */

#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

#include "stm32u5xx_hal.h"
#include "tx_port.h"

// ALS == Ambient Light Sensor
// Light sensor I2C address
#define ALS_ADDR	 	0x29

typedef enum {
    ALS_CONTR           = 0x80, //RW
    ALS_MEAS_RATE       = 0x85, //RW
    ALS_PART_ID_ADDR    = 0x86, //R
    ALS_MANUFAC_ID_ADDR = 0x87, //R
    ALS_DATA            = 0x88, //R
    // Channel 1 is the IR diode
    // 0x88 Channel 1 Lower byte
    // 0x89 Channel 1 Upper byte
    // Channel 0 is the visible + IR diode
    // 0x8A Channel 0 Lower byte
    // 0x8B Channel 0 Upper byte
    ALS_STATUS          = 0x8C, //R
} ALSRegAddress;

#define ALS_DLEN		4


// ALS_CONTR register: SW reset set in bit 1. Set to one to start a reset
#define LIGHT_RESET		0b10
// ALS_CONTR register: ALS mode set in bit 0. Set to one to put in active mode
#define LIGHT_WAKEUP	0b01
#define LIGHT_SLEEP		0b00

//ToDo: Lux conversion formula
#define LUX(lux) lux

//Expected part values
#define ALS_MANUFAC_ID   0b00000101
#define ALS_PART_ID      0b1010
#define ALS_REVISION_ID  0b0000

//Delay values
#define LIGHT_DELAY_MS	1000

//Light sensor flags
#define LIGHT_UNIT_TEST_FLAG		0x1
#define LIGHT_UNIT_TEST_DONE_FLAG	0x2
#define LIGHT_READ_FLAG				0x4
#define LIGHT_WRITE_FLAG			0x8
#define LIGHT_CMD_FLAG				0x10

//Commands for light sensor
#define LIGHT_GET_SAMPLES_CMD		0x1

typedef enum {
    GAIN_DEF    = 0b000,    // Gain x1 -> 1 lux to 64k lux
    GAIN_2X     = 0b001,    // Gain x2 -> 0.5 lux to 32k lux
    GAIN_4X     = 0b010,    // Gain x4 -> 0.25 lux to 16k lux
    GAIN_8X     = 0b011,    // Gain x8 -> 0.125 lux to 8k lux
    RESERVED0   = 0b100,    // Invalid value
    RESERVED1   = 0b101,    // Invalid value
    GAIN_48X    = 0b110,    // Gain x48 -> 0.02 lux to 1.3k lux
    GAIN_96X    = 0b111,    // Gain x96 -> 0.01 lux to 600 lux
} ALSGain;

// ALSMeasureTime register: ALS integration time set in bits 5:3
typedef enum {
    ALS_INTEG_TIME_100_MS	= 0b000,	// 100ms (default)
	ALS_INTEG_TIME_50_MS	= 0b001,	// 50ms
	ALS_INTEG_TIME_200_MS	= 0b010,	// 200ms
	ALS_INTEG_TIME_400_MS	= 0b011,	// 400ms
	ALS_INTEG_TIME_150_MS	= 0b100,	// 150ms
	ALS_INTEG_TIME_250_MS	= 0b101,	// 250ms
	ALS_INTEG_TIME_300_MS	= 0b110,	// 300ms
	ALS_INTEG_TIME_350_MS	= 0b111,	// 350ms
} ALSIntegrationTime;

#define ALS_INTEG_TIME_DEF ALS_INTEG_TIME_100_MS
// ALSMeasureTime register: ALS measurement rate set in bits 2:0
typedef enum {
    ALS_MEAS_TIME_50_MS	  = 0b000,	// 50ms
	ALS_MEAS_TIME_100_MS  = 0b001,	// 100ms
	ALS_MEAS_TIME_200_MS  = 0b010,	// 200ms
	ALS_MEAS_TIME_500_MS  = 0b011,	// 500ms (default)
	ALS_MEAS_TIME_1000_MS = 0b100,	// 1000ms
	ALS_MEAS_TIME_2000_MS = 0b101,	// 2000ms
	ALS_MEAS_TIME_7		  = 0b110,	// 2000ms
	ALS_MEAS_TIME_8		  = 0b111,	// 2000ms
} ALSMeasureTime;

/* "little-endian" storage order effects LSB vs MSB first bit ordering in gcc */
typedef struct als_control_reg_s{
    uint8_t als_mode; //0
    uint8_t sw_reset; //1
    ALSGain gain; //2:4
}ALSControlRegister;

typedef struct als_meas_rate_reg_s{
    ALSMeasureTime measurement_time; //0:2
    ALSIntegrationTime integration_time; //3:5
}ALSMeasureRateRegister;


typedef struct als_part_id_reg_s{
    uint8_t revision_id;    //0:3
    uint8_t part_number_id; //4:7
}ALSPartIDRegister;


typedef uint8_t ALSManufacIDRegister;

// Bit 7   Data Valid: 0 == valid, 1 == invalid
// Bit 6:4 Data gain range (below)
// Bit 2   Data Status: 0 == old/read data, 1 == new data
typedef struct als_status_reg_s{
    uint8_t invalid;
    ALSGain gain;
    uint8_t new;
}ALSStatusRegister;

__attribute__ ((scalar_storage_order("little-endian")))
typedef struct als_data_reg_s{
    uint16_t infrared;
    uint16_t visible;
}ALSDataRegister;

typedef struct als_data{
    ALSStatusRegister status;
    ALSDataRegister data;
}ALSData;

typedef struct __LightSensor_TypeDef
{
	I2C_HandleTypeDef *i2c_handler;

	ALSGain gain;

	// Data buffers for I2C data
	ALSStatusRegister status;
    ALSDataRegister data;
} LightSensorHandleTypedef;

void light_thread_entry(ULONG thread_input);
HAL_StatusTypeDef LightSensor_init(I2C_HandleTypeDef *hi2c_device, LightSensorHandleTypedef *light_sensor);
HAL_StatusTypeDef LightSensor_wake_up(LightSensorHandleTypedef *light_sensor, ALSGain gain);
HAL_StatusTypeDef LightSensor_sleep(LightSensorHandleTypedef *light_sensor);
HAL_StatusTypeDef LightSensor_get_data(LightSensorHandleTypedef *light_sensor);
HAL_StatusTypeDef LightSensor_set_data_rate(LightSensorHandleTypedef *light_sensor, ALSIntegrationTime int_time, ALSMeasureTime meas_rate);
HAL_StatusTypeDef LightSensor_get_part_id(LightSensorHandleTypedef *light_sensor, ALSPartIDRegister *dst);
HAL_StatusTypeDef LightSensor_get_manufacturer(LightSensorHandleTypedef *light_sensor, ALSManufacIDRegister *dst);

#endif /* LIGHTSENSOR_H */
