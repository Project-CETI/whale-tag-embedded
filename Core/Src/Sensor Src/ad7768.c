/***************************************************************************//**
 *   @file
 *   @brief  Implementation of AD7768 Driver.
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

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include "ad7768.h"
#include "main.h"
//#include "audio.h"



const uint8_t standard_pin_ctrl_mode_sel[3][4] = {
//		MCLK/1,	MCLK/2,	MCLK/4,	MCLK/8
	{0x0,	0x1,	0x2,	0x3},	// Eco
	{0x4,	0x5,	0x6,	0x7},	// Median
	{0x8,	0x9,	0xA,	0xB},	// Fast
};

const uint8_t one_shot_pin_ctrl_mode_sel[3][4] = {
//		MCLK/1,	MCLK/2,	MCLK/4,	MCLK/8
	{0xC,	0xFF,	0xFF,	0xFF},	// Eco
	{0xD,	0xFF,	0xFF,	0xFF},	// Median
	{0xF,	0xE,	0xFF,	0xFF},	// Fast
};


#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1))
#define _LSHIFT(val, s, w) (((val) & ((1 << (w)) - 1)) << (s))

static inline const ad7768_Reg_ChStandby __reg_channelStandby_fromRaw(const uint8_t raw) {
	return (ad7768_Reg_ChStandby){
        .ch[0] = _RSHIFT(raw, 0, 1), 
        .ch[1] = _RSHIFT(raw, 1, 1), 
        .ch[2] = _RSHIFT(raw, 2, 1), 
        .ch[3] = _RSHIFT(raw, 3, 1),
	};
}

static inline const uint8_t __reg_channelStandby_intoRaw(const ad7768_Reg_ChStandby *reg) {
	return _LSHIFT(reg->ch[0], 0, 1)  
		| _LSHIFT(reg->ch[1], 1, 1)  
		| _LSHIFT(reg->ch[2], 2, 1)  
		| _LSHIFT(reg->ch[3], 3, 1);
}

static inline const ad7768_Reg_ChMode __reg_channelMode_fromRaw(const uint8_t raw) {
	return (ad7768_Reg_ChMode){
		.filter_type = _RSHIFT(raw, 3, 1),
		.dec_rate = _RSHIFT(raw, 0, 3)
	};
}

static inline const uint8_t __reg_channelMode_intoRaw(const ad7768_Reg_ChMode *reg) {
	return _LSHIFT(reg->filter_type, 3, 1) 
		| _LSHIFT(reg->filter_type, 0, 3);
}

static inline const ad7768_Reg_ChModeSelect __reg_channelModeSelect_fromRaw(const uint8_t raw) {
	return (ad7768_Reg_ChModeSelect){
		.ch = {
			[0] = _RSHIFT(raw, 0, 1), 
			[1] = _RSHIFT(raw, 1, 1), 
			[2] = _RSHIFT(raw, 2, 1), 
			[3] = _RSHIFT(raw, 3, 1)
		}
	};
}

static inline const uint8_t __reg_channelModeSelect_intoRaw(const ad7768_Reg_ChModeSelect *reg) {
	return _LSHIFT(reg->ch[0], 0, 1)  
		| _LSHIFT(reg->ch[1], 1, 1)  
		| _LSHIFT(reg->ch[2], 2, 1)  
		| _LSHIFT(reg->ch[3], 3, 1)
		| _LSHIFT(reg->ch[2], 4, 1)  
		| _LSHIFT(reg->ch[3], 5, 1);
}

static inline const ad7768_Reg_PowerMode __reg_powerMode_fromRaw(const uint8_t raw) {
	return (ad7768_Reg_PowerMode){
			.sleep_mode 	= _RSHIFT(raw, 7, 1),
			.power_mode 	= _RSHIFT(raw, 4, 2),
			.lvds_enable	= _RSHIFT(raw, 3, 1),
			.mclk_div 		= _RSHIFT(raw, 0, 2),
	};
}

static inline const uint8_t __reg_powerMode_intoRaw(const ad7768_Reg_PowerMode *reg) {
	return _LSHIFT(reg->sleep_mode, 7, 1)
		| _LSHIFT(reg->power_mode, 4, 2)
		| _LSHIFT(reg->lvds_enable, 3, 1)
		| _LSHIFT(reg->mclk_div, 0, 2);
}

static inline const ad7768_Reg_GeneralCfg __reg_generalCfg_fromRaw(const uint8_t raw){
	return (ad7768_Reg_GeneralCfg){
		.retime_en 	= _RSHIFT(raw, 5, 1),
		.vcm_pd 	= _RSHIFT(raw, 4, 1),
		.vcm_vsel 	= _RSHIFT(raw, 0, 2),
	};
}

static inline const uint8_t __reg_generalCfg_intoRaw(const ad7768_Reg_GeneralCfg *reg){
	return  _LSHIFT(reg->retime_en, 5, 1)
		| _LSHIFT(reg->vcm_pd, 4, 1)
		| _LSHIFT(reg->vcm_vsel, 0, 2);
}

static inline const ad7768_Reg_DataControl __reg_dataControl_fromRaw(const uint8_t raw){
	return (ad7768_Reg_DataControl){
		.spi_sync 		= _RSHIFT(raw, 7, 1),
		.single_shot_en = _RSHIFT(raw, 4, 1),
		.spi_reset 		= _RSHIFT(raw, 0, 2),
	};
}

static inline const uint8_t __reg_dataControl_intoRaw(const ad7768_Reg_DataControl *reg){
	return  _LSHIFT(reg->spi_sync, 7, 1)
  	| _LSHIFT(reg->single_shot_en, 4, 1)
  	| _LSHIFT(reg->spi_reset, 0, 2);
}

static inline const ad7768_Reg_InterfaceCfg __reg_interfaceCfg_fromRaw(const uint8_t raw){
	return (ad7768_Reg_InterfaceCfg){
		.crc_select = _RSHIFT(raw, 2, 2),
		.dclk_div 	= _RSHIFT(raw, 0, 2),
	};
}

static inline const uint8_t __reg_interfaceCfg_intoRaw(const ad7768_Reg_InterfaceCfg *reg){
	return  _LSHIFT(reg->crc_select, 2, 2)
  	| _LSHIFT(reg->dclk_div, 0, 2);
}

static inline const ad7768_Reg_BISTControl __reg_bistControl_fromRaw(const uint8_t raw){
	return (ad7768_Reg_BISTControl){
		.ram_bist_start = _RSHIFT(raw, 0, 1)
	};
}

static inline const uint8_t __reg_bistControl_intoRaw(const ad7768_Reg_BISTControl *reg){
	return _LSHIFT(reg->ram_bist_start, 0, 1);
}

static inline const ad7768_Reg_DeviceStatus __reg_deviceStatus_fromRaw(const uint8_t raw){
	return (ad7768_Reg_DeviceStatus){
        .chip_error       = _RSHIFT(raw, 3, 1),
        .no_clock_error   = _RSHIFT(raw, 2, 1),
        .ram_bist_pass    = _RSHIFT(raw, 1, 1),
        .ram_bist_running = _RSHIFT(raw, 0, 1),
	};
}

/**
 * SPI read from device.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param reg_data - The register data.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_spi_read(ad7768_dev *dev,
			uint8_t reg_addr,
			uint8_t *reg_data)
{
	uint16_t tx_buf[2] = {
        ((0x80 | reg_addr) << 8), 
        ((0x80 | reg_addr) << 8)
    };
	uint16_t rx_buf[2];
	HAL_StatusTypeDef ret;

	ret = HAL_SPI_TransmitReceive(dev->spi_handler, (uint8_t*)tx_buf, (uint8_t*)rx_buf, 2, ADC_TIMEOUT);

	*reg_data = (uint8_t)(rx_buf[1] & 0x00FF);

	return ret;
}

/**
 * SPI write to device.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param reg_data - The register data.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_spi_write(ad7768_dev *dev,
			 uint8_t reg_addr,
			 uint8_t reg_data)
{
	uint16_t buf;
	int32_t ret;

	buf = ((uint16_t)AD7768_SPI_WRITE(reg_addr) << 8) | reg_data;

	ret = HAL_SPI_Transmit(dev->spi_handler, (uint8_t *)&buf, 1, ADC_TIMEOUT);

	return ret;
}

/**
 * Set the device sleep mode.
 * @param dev - The device structure.
 * @param mode - The device sleep mode.
 * 				 Accepted values: AD7768_ACTIVE
 * 								  AD7768_SLEEP
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_sleep_mode(ad7768_dev *dev, ad7768_sleep_mode mode){
	HAL_StatusTypeDef ret = HAL_OK;
	uint8_t reg_data;

    ret |= ad7768_spi_read(dev, AD7768_REG_PWR_MODE, &reg_data);
    dev->power_mode = __reg_powerMode_fromRaw(reg_data);
    if(dev->power_mode.sleep_mode != mode){
	    dev->power_mode.sleep_mode = mode;
	    ret |= ad7768_spi_write(dev, AD7768_REG_PWR_MODE, __reg_powerMode_intoRaw(&dev->power_mode));
    }

	return ret;
}

/**
 * Get the device sleep mode.
 * @param dev - The device structure.
 * @param mode - The device sleep mode.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_sleep_mode(ad7768_dev *dev,
			      ad7768_sleep_mode *mode)
{
	*mode = dev->power_mode.sleep_mode;

	return 0;
}


/**
 * Set the device power mode.
 * @param dev - The device structure.
 * @param mode - The device power mode.
 * 				 Accepted values: AD7768_ECO
 *								  AD7768_MEDIAN
 *								  AD7768_FAST
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_power_mode(ad7768_dev *dev,
			      ad7768_power_mode mode)
{
	HAL_StatusTypeDef ret = HAL_ERROR;

	if (dev->pin_spi_ctrl == AD7768_SPI_CTRL) {
		dev->power_mode.power_mode = mode;
		ret = ad7768_spi_write(dev,
				      AD7768_REG_PWR_MODE,
				      __reg_powerMode_intoRaw(&dev->power_mode));
	}
	return ret;
}

/**
 * Get the device power mode.
 * @param dev - The device structure.
 * @param mode - The device power mode.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_power_mode(ad7768_dev *dev,
			      ad7768_power_mode *mode)
{
	*mode = dev->power_mode.power_mode;

	return 0;
}

/**
 * Set the MCLK divider.
 * @param dev - The device structure.
 * @param clk_div - The MCLK divider.
 * 					Accepted values: AD7768_MCLK_DIV_32
 *									 AD7768_MCLK_DIV_8
 *									 AD7768_MCLK_DIV_4
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_mclk_div(ad7768_dev *dev, ad7768_mclk_div clk_div){
	HAL_StatusTypeDef ret = HAL_OK;
    uint8_t raw;

    ret |= ad7768_spi_read(dev, AD7768_REG_PWR_MODE, &raw);
    dev->power_mode = __reg_powerMode_fromRaw(raw);
    if(dev->power_mode.mclk_div != clk_div){
        dev->power_mode.mclk_div = clk_div;
        ret |= ad7768_spi_write(dev, AD7768_REG_PWR_MODE, __reg_powerMode_intoRaw(&dev->power_mode));
    }

	return ret;
}

/**
 * Get the MCLK divider.
 * @param dev - The device structure.
 * @param clk_div - The MCLK divider.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_mclk_div(ad7768_dev *dev,
			    ad7768_mclk_div *clk_div)
{
	*clk_div = dev->power_mode.mclk_div;

	return HAL_OK;
}

/**
 * Set the DCLK divider.
 * @param dev - The device structure.
 * @param clk_div - The DCLK divider.
 * 					Accepted values: AD7768_DCLK_DIV_1
 *									 AD7768_DCLK_DIV_2
 *									 AD7768_DCLK_DIV_4
 *									 AD7768_DCLK_DIV_8
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_dclk_div(ad7768_dev *dev,
			    ad7768_dclk_div clk_div)
{
	HAL_StatusTypeDef ret = HAL_OK;
    uint8_t raw;

	if (dev->pin_spi_ctrl != AD7768_SPI_CTRL) {
        return HAL_ERROR;
    }
		
    ret |= ad7768_spi_read(dev, AD7768_REG_INTERFACE_CFG, &raw);
    dev->interface_config = __reg_interfaceCfg_fromRaw(raw);
    if(dev->interface_config.dclk_div != clk_div){
        dev->interface_config.dclk_div = clk_div;
        ret |= ad7768_spi_write(dev, AD7768_REG_INTERFACE_CFG, __reg_interfaceCfg_intoRaw(&dev->interface_config));
    }

	return ret;
}

/**
 * Get the DCLK divider.
 * @param dev - The device structure.
 * @param clk_div - The DCLK divider.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_dclk_div(ad7768_dev *dev,
			    ad7768_dclk_div *clk_div)
{
	*clk_div = dev->interface_config.dclk_div;

	return 0;
}


/**
 * Set the conversion operation mode.
 * @param dev - The device structure.
 * @param conv_op - The conversion operation mode.
 * 					Accepted values: AD7768_STANDARD_CONV
 * 									 AD7768_ONE_SHOT_CONV
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_conv_op(ad7768_dev *dev, ad7768_conv_op conv_op){
	HAL_StatusTypeDef ret = HAL_OK;
    uint8_t raw;

	if (dev->pin_spi_ctrl != AD7768_SPI_CTRL) {
        return HAL_ERROR;
    }

    ret |= ad7768_spi_read(dev, AD7768_REG_DATA_CTRL, &raw);
    dev->data_control = __reg_dataControl_fromRaw(raw);
    if(dev->data_control.single_shot_en != conv_op){
        dev->data_control.single_shot_en = conv_op;
        ret |= ad7768_spi_write(dev, AD7768_REG_DATA_CTRL, __reg_dataControl_intoRaw(&dev->data_control));
    }
    return ret;
}

/**
 * Get the conversion operation mode.
 * @param dev - The device structure.
 * @param conv_op - The conversion operation mode.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_conv_op(ad7768_dev *dev,
			   ad7768_conv_op *conv_op)
{
	*conv_op = dev->data_control.single_shot_en;

	return 0;
}

/**
 * Set the CRC selection.
 * @param dev - The device structure.
 * @param crc_sel - The CRC selection.
 * 					Accepted values: AD7768_NO_CRC
 * 									 AD7768_CRC_4
 * 									 AD7768_CRC_16
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_crc_sel(ad7768_dev *dev,
			   ad7768_crc_sel crc_sel)
{
	HAL_StatusTypeDef ret = HAL_OK;
    uint8_t raw;

    ret |= ad7768_spi_read(dev, AD7768_REG_INTERFACE_CFG, &raw);
    dev->interface_config = __reg_interfaceCfg_fromRaw(raw);
    
    if(dev->interface_config.crc_select != crc_sel){
        dev->interface_config.crc_select = crc_sel;
        ret |= ad7768_spi_write(dev, AD7768_REG_INTERFACE_CFG, __reg_interfaceCfg_intoRaw(&dev->interface_config));
    }

	return ret;
}

/**
 * Get the CRC selection.
 * @param dev - The device structure.
 * @param crc_sel - The CRC selection.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_crc_sel(ad7768_dev *dev,
			   ad7768_crc_sel *crc_sel)
{
	*crc_sel = dev->interface_config.crc_select;

	return 0;
}

/**
 * Set the channel state.
 * @param dev - The device structure.
 * @param ch - The channel number.
 * 			   Accepted values: AD7768_CH0
 * 			   					AD7768_CH1
 * 			   					AD7768_CH2
 * 			   					AD7768_CH3
 * @param state - The channel state.
 * 				  Accepted values: AD7768_ENABLED
 * 								   AD7768_STANDBY
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_ch_state(ad7768_dev *dev,
			    ad7768_ch ch,
			    ad7768_ch_state state)
{
	HAL_StatusTypeDef ret = HAL_OK;
    uint8_t raw;

    ret |= ad7768_spi_read(dev, AD7768_REG_CH_STANDBY, &raw);
    dev->channel_standby = __reg_channelStandby_fromRaw(raw);
    if(dev->channel_standby.ch[ch] != state){
        dev->channel_standby.ch[ch] = state;
        ret |= ad7768_spi_write(dev, AD7768_REG_CH_STANDBY, __reg_channelStandby_intoRaw(&dev->channel_standby));
    }

	return ret;
}

/**
 * Get the channel state.
 * @param dev - The device structure.
 * @param ch - The channel number.
 * 			   Accepted values: AD7768_CH0
 * 			   					AD7768_CH1
 * 			   					AD7768_CH2
 * 			   					AD7768_CH3
 * @param state - The channel state.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_ch_state(ad7768_dev *dev,
			    ad7768_ch ch,
			    ad7768_ch_state *state)
{
	*state = dev->channel_standby.ch[ch];

	return 0;
}

/**
 * Set the mode configuration.
 * @param dev - The device structure.
 * @param mode - The channel mode.
 * 				 Accepted values: AD7768_MODE_A
 * 								  AD7768_MODE_B
 * @param filt_type - The filter type.
 * 					  Accepted values: AD7768_FILTER_WIDEBAND
 * 					  				   AD7768_FILTER_SINC,
 * @param dec_rate - The decimation rate.
 * 					 Accepted values: AD7768_DEC_X32
 * 					 				  AD7768_DEC_X64
 * 					 				  AD7768_DEC_X128
 * 					 				  AD7768_DEC_X256
 * 					 				  AD7768_DEC_X512
 * 					 				  AD7768_DEC_X1024
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_mode_config(ad7768_dev *dev,
			       ad7768_ch_mode mode,
			       ad7768_filt_type filt_type,
			       ad7768_dec_rate dec_rate)
{
	dev->channel_mode[mode] = (ad7768_Reg_ChMode){
		.filter_type = filt_type,
		.dec_rate = dec_rate
	};

	return ad7768_spi_write(dev, (mode == AD7768_MODE_A) ? AD7768_REG_CH_MODE_A : AD7768_REG_CH_MODE_B, __reg_channelMode_intoRaw(&dev->channel_mode[mode]));
}

/**
 * Get the mode configuration.
 * @param dev - The device structure.
 * @param mode - The channel mode.
 * @param filt_type - The filter type.
 * @param dec_rate - The decimation rate.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_mode_config(ad7768_dev *dev,
			       ad7768_ch_mode mode,
			       ad7768_filt_type *filt_type,
			       ad7768_dec_rate *dec_rate)
{
	*filt_type = dev->channel_mode[mode].filter_type;
	*dec_rate = dev->channel_mode[mode].dec_rate;

	return 0;
}

/**
 * Set the channel mode.
 * @param dev - The device structure.
 * @param ch - The channel number.
 * 			   Accepted values: AD7768_CH0
 * 			   					AD7768_CH1
 * 			   					AD7768_CH2
 * 			   					AD7768_CH3
 * @param mode - The channel mode.
 * 				 Accepted values: AD7768_MODE_A
 * 								  AD7768_MODE_B
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_set_ch_mode(ad7768_dev *dev,
			   ad7768_ch ch,
			   ad7768_ch_mode mode)
{
	HAL_StatusTypeDef ret = HAL_OK;
    uint8_t raw;

    ret = ad7768_spi_read(dev, AD7768_REG_CH_MODE_SEL, &raw);
    dev->channel_mode_select = __reg_channelModeSelect_fromRaw(raw);
    if(dev->channel_mode_select.ch[ch] != mode){
        dev->channel_mode_select.ch[ch] = mode;
        ret = ad7768_spi_write(dev, AD7768_REG_CH_MODE_SEL, __reg_channelModeSelect_intoRaw(&dev->channel_mode_select));
    }

	return ret;
}

/**
 * Get the channel mode.
 * @param dev - The device structure.
 * @param ch - The channel number.
 * 			   Accepted values: AD7768_CH0
 * 			   					AD7768_CH1
 * 			   					AD7768_CH2
 * 			   					AD7768_CH3
 * @param mode - The channel mode.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_get_ch_mode(ad7768_dev *dev,
			   ad7768_ch ch,
			   ad7768_ch_mode *mode)
{
	*mode = dev->channel_mode_select.ch[ch];

	return 0;
}

/*
 * perform soft reset on the device
 * @param device - the device structure
 */
HAL_StatusTypeDef ad7768_softReset(ad7768_dev *dev)
{
    HAL_StatusTypeDef ret = HAL_ERROR;
    uint16_t reset_commands[4] = {
        AD7768_WRITE_CMD(AD7768_REG_DATA_CTRL, __reg_dataControl_intoRaw(&(ad7768_Reg_DataControl){.spi_reset = AD7768_SPI_RESET_FIRST})),
        AD7768_WRITE_CMD(AD7768_REG_DATA_CTRL, __reg_dataControl_intoRaw(&(ad7768_Reg_DataControl){.spi_reset = AD7768_SPI_RESET_SECOND})),
    };
    ret = HAL_SPI_Transmit(dev->spi_handler, (uint8_t *)reset_commands, 2, ADC_TIMEOUT);
    HAL_Delay(5);
    return ret;
}

/**
 * Initialize the device.
 * @param device - The device structure.
 * @param init_param - The structure that contains the device initial
 * 					   parameters.
 * @return 0 in case of success, negative error code otherwise.
 */
HAL_StatusTypeDef ad7768_setup(ad7768_dev *dev, SPI_HandleTypeDef *hspi, SAI_HandleTypeDef *hsai)
{
	HAL_StatusTypeDef ret = HAL_ERROR;
	uint8_t data = 0;

    *dev = (ad7768_dev){
        .spi_handler = hspi,
        .sai_handler = hsai,
        .channel_standby = {
            .ch[0] = AD7768_ENABLED,
            .ch[1] = AD7768_ENABLED,
            .ch[2] = AD7768_ENABLED,
            .ch[3] = AD7768_STANDBY
        },
        .channel_mode[AD7768_MODE_A] = {.filter_type = AD7768_FILTER_SINC, .dec_rate = AD7768_DEC_X32},
        .channel_mode[AD7768_MODE_B] = {.filter_type = AD7768_FILTER_SINC, .dec_rate = AD7768_DEC_X32},
        .channel_mode_select = {
            .ch[0] = AD7768_MODE_B,
            .ch[1] = AD7768_MODE_B,
            .ch[2] = AD7768_MODE_B,
            .ch[3] = AD7768_MODE_A,
	    },
        .power_mode = {
            .sleep_mode = AD7768_ACTIVE,
            .power_mode = AD7768_MEDIAN,
            .lvds_enable = false,
            .mclk_div = AD7768_MCLK_DIV_4,
        },
        .interface_config = {
            .crc_select = AD7768_CRC_NONE,
            .dclk_div = AD7768_DCLK_DIV_1,
        },
        .pin_spi_ctrl = AD7768_SPI_CTRL,
    };

    ad7768_softReset(dev);
    ret = ad7768_spi_read(dev, AD7768_REG_DEV_STATUS, &data); // Ensure the status register reads no error

	// TODO: Change to output an error
	// Bit 3 is CHIP_ERROR, Bit 2 is NO_CLOCK_ERROR, check both to see if there is an error
	if(data != 0x00)
		return HAL_ERROR;

    ret |= ad7768_getRevisionID(dev, &data);
    ret |= ad7768_getRevisionID(dev, &data);
    ret |= ad7768_getRevisionID(dev, &data);
    if(data != 0x06){
        return HAL_ERROR;
    }

	uint16_t tx_buffer[] = {
		AD7768_WRITE_CMD(AD7768_REG_CH_STANDBY,    __reg_channelStandby_intoRaw(&dev->channel_standby)),
		AD7768_WRITE_CMD(AD7768_REG_CH_MODE_A,     __reg_channelMode_intoRaw(&dev->channel_mode[AD7768_MODE_A])),
		AD7768_WRITE_CMD(AD7768_REG_CH_MODE_B,     __reg_channelMode_intoRaw(&dev->channel_mode[AD7768_MODE_B])),
		AD7768_WRITE_CMD(AD7768_REG_CH_MODE_SEL,   __reg_channelModeSelect_intoRaw(&dev->channel_mode_select)),
		AD7768_WRITE_CMD(AD7768_REG_PWR_MODE,      __reg_powerMode_intoRaw(&dev->power_mode)),
		AD7768_WRITE_CMD(AD7768_REG_INTERFACE_CFG, __reg_interfaceCfg_intoRaw(&dev->interface_config)),
		AD7768_WRITE_CMD(AD7768_REG_GPIO_CTRL, 	   0x00),
	};

	ret |= HAL_SPI_Transmit(dev->spi_handler, (uint8_t *)&tx_buffer, sizeof(tx_buffer)/sizeof(tx_buffer[0]), ADC_TIMEOUT);
	ret |= ad7768_sync(dev);

	// TODO: Add error detection and correction
//	if (!ret)
//		printf("AD7768 successfully initialized\n");

	return ret;
}

HAL_StatusTypeDef ad7768_sync(ad7768_dev *dev){
	HAL_StatusTypeDef ret = HAL_ERROR;
	const uint16_t tx_buffer[2] = {
        AD7768_WRITE_CMD(AD7768_REG_DATA_CTRL, __reg_dataControl_intoRaw(&(ad7768_Reg_DataControl){.spi_sync = 0})),
        AD7768_WRITE_CMD(AD7768_REG_DATA_CTRL, __reg_dataControl_intoRaw(&(ad7768_Reg_DataControl){.spi_sync = 1}))
    }; 

	ret = HAL_SPI_Transmit(dev->spi_handler, (uint8_t*)tx_buffer, 2, ADC_TIMEOUT);

	return ret;
}

HAL_StatusTypeDef ad7768_getRevisionID(ad7768_dev *dev, uint8_t *reg_data){
	HAL_StatusTypeDef ret = HAL_ERROR;
    ret = ad7768_spi_read(dev, AD7768_REG_REV_ID, reg_data);
	return ret;
}
