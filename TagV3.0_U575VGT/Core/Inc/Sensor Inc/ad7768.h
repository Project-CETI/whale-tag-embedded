/***************************************************************************//**
 *   @file   ad7768.h
 *   @brief  Header file of AD7768 Driver.
 *   @author DBogdan (dragos.bogdan@analog.com)
********************************************************************************
 * Copyright 2016(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#ifndef AD7768_H_
#define AD7768_H_

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include "stm32u5xx_hal.h"
#include <stdbool.h>

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/
#define AD7768_REG_CH_STANDBY				0x00
#define AD7768_REG_CH_MODE_A				0x01
#define AD7768_REG_CH_MODE_B				0x02
#define AD7768_REG_CH_MODE_SEL				0x03
#define AD7768_REG_PWR_MODE					0x04
#define AD7768_REG_GENERAL_CFG				0x05
#define AD7768_REG_DATA_CTRL				0x06
#define AD7768_REG_INTERFACE_CFG			0x07
#define AD7768_REG_BIST_CTRL				0x08
#define AD7768_REG_DEV_STATUS				0x09
#define AD7768_REG_REV_ID					0x0A
#define AD7768_REG_DEV_ID_MSB				0x0B
#define AD7768_REG_DEV_ID_LSB				0x0C
#define AD7768_REG_SW_REV_ID				0x0D
#define AD7768_REG_GPIO_CTRL				0x0E
#define AD7768_REG_GPIO_WR_DATA				0x0F
#define AD7768_REG_GPIO_RD_DATA				0x10
#define AD7768_REG_PRECHARGE_BUF_1			0x11
#define AD7768_REG_PRECHARGE_BUF_2			0x12
#define AD7768_REG_POS_REF_BUF				0x13
#define AD7768_REG_NEG_REF_BUF				0x14
#define AD7768_REG_CH_OFFSET_1(ch)			(0x1E + (ch) * 3)
#define AD7768_REG_CH_OFFSET_2(ch)			(0x2A + (ch) * 3)
#define AD7768_REG_CH_GAIN_1(ch)			(0x36 + (ch) * 3)
#define AD7768_REG_CH_GAIN_2(ch)			(0x42 + (ch) * 3)
#define AD7768_REG_CH_SYNC_OFFSET_1(ch)		(0x4E + (ch) * 3)
#define AD7768_REG_CH_SYNC_OFFSET_2(ch)		(0x52 + (ch) * 3)
#define AD7768_REG_DIAG_METER_RX			0x56
#define AD7768_REG_DIAG_CTRL				0x57
#define AD7768_REG_DIAG_MOD_DELAY_CTRL		0x58
#define AD7768_REG_DIAG_CHOP_CTRL			0x59
#define AD7768_REG_MAX

/* AD7768_REG_CH_STANDBY */
#define AD7768_CH_STANDBY(x)				(1 << (x))
#define AD7768_CH_SET_CH_STANDBY(reg, ch, val) (((reg) & ~(1 << (ch))) | ((val) << (ch))) 		
#define AD7768_WRITE_CMD(reg, data) ((uint16_t) ((AD7768_SPI_WRITE(reg) << 8) | (data)))
#define AD7768_READ_CMD(reg) 		((uint16_t) ((AD7768_SPI_READ(reg) << 8)))
/* AD7768_REG_CH_MODE_x */
#define AD7768_CH_MODE_FILTER_TYPE			(1 << 3)
#define AD7768_CH_MODE_DEC_RATE(x)			(((x) & 0x7) << 0)

/* AD7768_REG_CH_MODE_SEL */
#define AD7768_CH_MODE(x)					(1 << (x))

/* AD7768_REG_PWR_MODE */
#define AD7768_PWR_MODE_SLEEP_MODE			(1 << 7)
#define AD7768_PWR_MODE_POWER_MODE(x)		(((x) & 0x3) << 4)
#define AD7768_PWR_MODE_LVDS_ENABLE			(1 << 3)
#define AD7768_PWR_MODE_MCLK_DIV(x)			(((x) & 0x3) << 0)

/* AD7768_REG_DATA_CTRL */
#define AD7768_DATA_CTRL_SPI_SYNC			(1 << 7)
#define AD7768_DATA_CTRL_SINGLE_SHOT_EN		(1 << 4)
#define AD7768_DATA_CTRL_SPI_RESET(x)		(((x) & 0x3) << 0)

/* AD7768_REG_INTERFACE_CFG */
#define AD7768_INTERFACE_CFG_CRC_SEL(x)		(((x) & 0x3) << 2)
#define AD7768_INTERFACE_CFG_DCLK_DIV(x)	(((x) & 0x3) << 0)

#define AD7768_RESOLUTION					24

#define AD7768_SPI_READ(x)          		(0x80 | (x & 0x7F))
#define AD7768_SPI_WRITE(x)         		(x & 0x7F)

//SPI timeout value in ms
#define ADC_TIMEOUT							1000

#define AD7768_CLK_MAX_HZ	34000000

/******************************************************************************/
/*************************** Types Declarations *******************************/
/******************************************************************************/
typedef enum {
	AD7768_ACTIVE,
	AD7768_SLEEP,
} ad7768_sleep_mode;

typedef enum {
	AD7768_ECO = 0,    /*f_mod = [0.036 .. 1.024] MHz */
	AD7768_MEDIAN = 2, /*f_mod = [1.024 .. 4.096] MHz */
	AD7768_FAST = 3,   /*f_mod = [4.096 .. 8.192] MHz */
} ad7768_power_mode;

typedef enum {
	AD7768_MCLK_DIV_32 = 0,
	AD7768_MCLK_DIV_8 = 2,
	AD7768_MCLK_DIV_4 = 3,
} ad7768_mclk_div;

typedef enum {
	AD7768_DCLK_DIV_8 = 0b00,
	AD7768_DCLK_DIV_4 = 0b01,
	AD7768_DCLK_DIV_2 = 0b10,
	AD7768_DCLK_DIV_1 = 0b11,
} ad7768_dclk_div;

typedef enum {
	AD7768_PIN_CTRL,
	AD7768_SPI_CTRL,
} ad7768_pin_spi_ctrl;

typedef enum {
	AD7768_STANDARD_CONV,
	AD7768_ONE_SHOT_CONV,
} ad7768_conv_op;

typedef enum {
	AD7768_CRC_NONE,
	AD7768_CRC_4,
	AD7768_CRC_16
} ad7768_crc_sel;

typedef enum {
	AD7768_CH0,
	AD7768_CH1,
	AD7768_CH2,
	AD7768_CH3,
	AD7768_CH_NO
} ad7768_ch;

typedef enum {
	AD7768_ENABLED,
	AD7768_STANDBY,
} ad7768_ch_state;

typedef enum {
	AD7768_MODE_A,
	AD7768_MODE_B,
} ad7768_ch_mode;

typedef enum {
	AD7768_FILTER_WIDEBAND,
	AD7768_FILTER_SINC,
} ad7768_filt_type;

typedef enum {
	AD7768_DEC_X32,
	AD7768_DEC_X64,
	AD7768_DEC_X128,
	AD7768_DEC_X256,
	AD7768_DEC_X512,
	AD7768_DEC_X1024
} ad7768_dec_rate;

typedef enum {
	AD7768_SPI_RESET_SECOND = 0b10,
	AD7768_SPI_RESET_FIRST =  0b11,
} ad7768_spi_reset;

typedef enum {
	AD7768_VCM_ENABLED,
	AD7768_VCM_POWERED_DOWN,
} ad7768_vcm_pd;

typedef enum {
	AD7768_VCM_VSEL_HALF,
	AD7768_VCM_VSEL_1v65,
	AD7768_VCM_VSEL_2v5,
	AD7768_VCM_VSEL_2v14,
} ad7768_vcm_sel;

/* Register Structs*/
typedef struct {
	ad7768_ch_state ch[4];
} ad7768_Reg_ChStandby;

typedef struct{
	ad7768_filt_type filter_type;
	ad7768_dec_rate  dec_rate;
} ad7768_Reg_ChMode;

typedef struct{
	ad7768_ch_mode ch[4];
} ad7768_Reg_ChModeSelect;

typedef struct{
	ad7768_sleep_mode sleep_mode;
	ad7768_power_mode power_mode;
	bool 			  lvds_enable;
	ad7768_mclk_div   mclk_div;
} ad7768_Reg_PowerMode;

typedef struct{
	bool           retime_en;
	ad7768_vcm_pd  vcm_pd;
	ad7768_vcm_sel vcm_vsel;
}ad7768_Reg_GeneralCfg;

typedef struct{
	bool 			 spi_sync;
	bool 			 single_shot_en;
	ad7768_spi_reset spi_reset;
}ad7768_Reg_DataControl;

typedef struct{
	ad7768_crc_sel  crc_select;
	ad7768_dclk_div	dclk_div;
}ad7768_Reg_InterfaceCfg;

typedef struct{
	bool ram_bist_start;
}ad7768_Reg_BISTControl;

typedef struct{
	bool chip_error;
	bool no_clock_error;
	bool ram_bist_pass;
	bool ram_bist_running;
}ad7768_Reg_DeviceStatus;

/* Device Struct*/
typedef struct {
	SPI_HandleTypeDef	*spi_handler; //config

	// The GPIOs are unused
	GPIO_TypeDef		*NRST_GPIOx;
	uint16_t			NRST_GPIO_Pin;

	GPIO_TypeDef		*M0_GPIOx;
	uint16_t			M0_GPIO_Pin;

	GPIO_TypeDef		*M1_GPIOx;
	uint16_t			M1_GPIO_Pin;

	GPIO_TypeDef		*M2_GPIOx;
	uint16_t			M2_GPIO_Pin;

	GPIO_TypeDef		*M3_GPIOx;
	uint16_t			M3_GPIO_Pin;

	GPIO_TypeDef        *spi_cs_port;
	uint16_t			spi_cs_pin;

	// To determine logic in sw based on pin/spi mode
	ad7768_pin_spi_ctrl	pin_spi_ctrl;

	ad7768_Reg_ChStandby 	channel_standby;
	ad7768_Reg_ChMode	 	channel_mode[2];
	ad7768_Reg_ChModeSelect channel_mode_select;
	ad7768_Reg_PowerMode	power_mode;
	ad7768_Reg_GeneralCfg   general_config;
	ad7768_Reg_DataControl  data_control;
	ad7768_Reg_InterfaceCfg interface_config;


} ad7768_dev;

/******************************************************************************/
/************************ Functions Declarations ******************************/
/******************************************************************************/
/* SPI read from device. */
HAL_StatusTypeDef ad7768_spi_read(ad7768_dev *dev, 
			uint8_t reg_addr,
			uint8_t *reg_data);
/* SPI write to device. */
HAL_StatusTypeDef ad7768_spi_write(ad7768_dev *dev,
			 uint8_t reg_addr,
			 uint8_t reg_data);
/* Set the device sleep mode. */
HAL_StatusTypeDef ad7768_set_sleep_mode(ad7768_dev *dev,
			      ad7768_sleep_mode mode);
/* Get the device sleep mode. */
HAL_StatusTypeDef ad7768_get_sleep_mode(ad7768_dev *dev,
			      ad7768_sleep_mode *mode);
/* Set the device power mode. */
HAL_StatusTypeDef ad7768_set_power_mode(ad7768_dev *dev,
			      ad7768_power_mode mode);
/* Get the device power mode. */
HAL_StatusTypeDef ad7768_get_power_mode(ad7768_dev *dev,
			      ad7768_power_mode *mode);
/* Set the MCLK divider. */
HAL_StatusTypeDef ad7768_set_mclk_div(ad7768_dev *dev,
			    ad7768_mclk_div clk_div);
/* Get the MCLK divider. */
HAL_StatusTypeDef ad7768_get_mclk_div(ad7768_dev *dev,
			    ad7768_mclk_div *clk_div);
/* Set the DCLK divider. */
HAL_StatusTypeDef ad7768_set_dclk_div(ad7768_dev *dev,
			    ad7768_dclk_div clk_div);
/* Get the DCLK divider. */
HAL_StatusTypeDef ad7768_get_dclk_div(ad7768_dev *dev,
			    ad7768_dclk_div *clk_div);
/* Set the conversion operation mode. */
HAL_StatusTypeDef ad7768_set_conv_op(ad7768_dev *dev,
			   ad7768_conv_op conv_op);
/* Get the conversion operation mode. */
HAL_StatusTypeDef ad7768_get_conv_op(ad7768_dev *dev,
			   ad7768_conv_op *conv_op);
/* Set the CRC selection. */
HAL_StatusTypeDef ad7768_set_crc_sel(ad7768_dev *dev,
			   ad7768_crc_sel crc_sel);
/* Get the CRC selection. */
HAL_StatusTypeDef ad7768_get_crc_sel(ad7768_dev *dev,
			   ad7768_crc_sel *crc_sel);
/* Set the channel state. */
HAL_StatusTypeDef ad7768_set_ch_state(ad7768_dev *dev,
			    ad7768_ch ch,
			    ad7768_ch_state state);
/* Get the channel state. */
HAL_StatusTypeDef ad7768_get_ch_state(ad7768_dev *dev,
			    ad7768_ch ch,
			    ad7768_ch_state *state);
/* Set the mode configuration. */
HAL_StatusTypeDef ad7768_set_mode_config(ad7768_dev *dev,
			       ad7768_ch_mode mode,
			       ad7768_filt_type filt_type,
			       ad7768_dec_rate dec_rate);
/* Get the mode configuration. */
HAL_StatusTypeDef ad7768_get_mode_config(ad7768_dev *dev,
			       ad7768_ch_mode mode,
			       ad7768_filt_type *filt_type,
			       ad7768_dec_rate *dec_rate);
/* Set the channel mode. */
HAL_StatusTypeDef ad7768_set_ch_mode(ad7768_dev *dev,
			   ad7768_ch ch,
			   ad7768_ch_mode mode);
/* Get the channel mode. */
HAL_StatusTypeDef ad7768_get_ch_mode(ad7768_dev *dev,
			   ad7768_ch ch,
			   ad7768_ch_mode *mode);
/* Initialize the device. */
HAL_StatusTypeDef ad7768_setup(
	ad7768_dev 			*dev);

HAL_StatusTypeDef ad7768_sync(ad7768_dev *dev);

HAL_StatusTypeDef ad7768_get_revision_id(ad7768_dev *dev, uint8_t *reg_data); 

#endif // AD7768_H_
