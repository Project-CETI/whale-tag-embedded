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

#define ALS_PART_ID      0b1010
#define ALS_REVISION_ID  0b0000
#define ALS_MANUFAC_ID   0b00000101

// ALS_CONTR register: SW reset set in bit 1. Set to one to start a reset
#define LIGHT_RESET		0b10
// ALS_CONTR register: ALS mode set in bit 0. Set to one to put in active mode
#define LIGHT_WAKEUP	0b01
#define LIGHT_SLEEP		0b00

//ToDo: Lux conversion formula
#define LUX(lux) lux

typedef enum {
    GAIN_DEF    = 0b000,    // Gain x1 -> 1 lux to 64k lux
    GAIN_2X     = 0b001,    // Gain x2 -> 0.5 lux to 32k lux
    GAIN_4X     = 0b010,    // Gain x4 -> 0.25 lux to 16k lux
    GAIN_8X     = 0b011,    // Gain x8 -> 0.125 lux to 8k lux
    RESERVED0   = 0b100,    // Invalid value
    RESERVED1   = 0b101,    // Invalid value
    GAIN_48X    = 0b110,    // Gain x48 -> 0.02 lux to 1.3k lux
    GAIN_96X    = 0b111,    // Gain x96 -> 0.01 lux to 600 lux
} ALS_Gain;

// ALS_MEAS_RATE register: ALS integration time set in bits 5:3
typedef enum {
    INTEG_TIME_DEF	= 0b000,	// 100ms (default)
	INTEG_TIME_2	= 0b001,	// 50ms
	INTEG_TIME_3	= 0b010,	// 200ms
	INTEG_TIME_4	= 0b011,	// 400ms
	INTEG_TIME_5	= 0b100,	// 150ms
	INTEG_TIME_6	= 0b101,	// 250ms
	INTEG_TIME_7	= 0b110,	// 300ms
	INTEG_TIME_8	= 0b111,	// 350ms
} ALS_Integ_Time;

// ALS_MEAS_RATE register: ALS measurement rate set in bits 2:0
typedef enum {
    MEAS_TIME_1		= 0b000,	// 50ms
	MEAS_TIME_2		= 0b001,	// 100ms
	MEAS_TIME_3		= 0b010,	// 200ms
	MEAS_TIME_DEF	= 0b011,	// 500ms (default)
	MEAS_TIME_5		= 0b100,	// 1000ms
	MEAS_TIME_6		= 0b101,	// 2000ms
	MEAS_TIME_7		= 0b110,	// 2000ms
	MEAS_TIME_8		= 0b111,	// 2000ms
} ALS_Meas_Rate;

/* "little-endian" storage order effects LSB vs MSB first bit ordering in gcc */
typedef struct __attribute__ ((scalar_storage_order("little-endian"))) als_control_reg_s{
    uint8_t als_mode: 1; //0
    uint8_t SW_reset: 1; //1
    uint8_t gain    : 3; //2:4
    uint8_t res_7_5 : 3; //5:7
}ALSControlRegister;

typedef struct __attribute__ ((scalar_storage_order("little-endian"))) als_meas_rate_reg_s{
    uint8_t measurement_time : 3; //0:2
    uint8_t integration_time : 3; //3:5
    uint8_t res_7_6 : 2;          //6:7
}ALSMeasureRateRegister;


typedef struct __attribute__ ((scalar_storage_order("little-endian"))) als_part_id_reg_s{
    uint8_t revision_id: 4;    //0:3
    uint8_t part_number_id: 4; //4:7
}ALSPartIDRegister;

typedef uint8_t ALSManufacIDRegister;

// Bit 7   Data Valid: 0 == valid, 1 == invalid
// Bit 6:4 Data gain range (below)
// Bit 2   Data Status: 0 == old/read data, 1 == new data
typedef struct __attribute__ ((scalar_storage_order("little-endian"))) als_status_res_s{
    uint8_t res_1_0: 2; //0:1
    uint8_t new:     1; //2
    uint8_t res_3:   1; //3
    uint8_t gain:    3; //4:6
    uint8_t invalid: 1; //7
}ALSStatusReg;

typedef struct __Light_Sensor_TypeDef
{
	I2C_HandleTypeDef *i2c_handler;

	ALS_Gain gain;

	// Data buffers for I2C data
    struct __attribute__ ((scalar_storage_order("little-endian")))
    {
        uint16_t infrared;
        uint16_t visible;
    }data;

	ALSStatusReg status;
} Light_Sensor_HandleTypedef;

HAL_StatusTypeDef Light_Sensor_Init(Light_Sensor_HandleTypedef *light_sensor, I2C_HandleTypeDef *hi2c_device);
HAL_StatusTypeDef Light_Sensor_WakeUp(Light_Sensor_HandleTypedef *light_sensor, ALS_Gain gain);
HAL_StatusTypeDef Light_Sensor_Set_DataRate(Light_Sensor_HandleTypedef *light_sensor, ALS_Integ_Time int_time, ALS_Meas_Rate meas_rate);
HAL_StatusTypeDef Light_Sensor_Get_Data(Light_Sensor_HandleTypedef *light_sensor);
HAL_StatusTypeDef Light_Sensor_Sleep(Light_Sensor_HandleTypedef *light_sensor);
HAL_StatusTypeDef LightSensor_getPartID(Light_Sensor_HandleTypedef *light_sensor, ALSPartIDRegister *dst);
HAL_StatusTypeDef LightSensor_getManufacturer(Light_Sensor_HandleTypedef *light_sensor, ALSManufacIDRegister *dst);

#endif /* LIGHTSENSOR_H */
