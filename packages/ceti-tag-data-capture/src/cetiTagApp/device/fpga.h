//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef __CETI_WHALE_TAG_FPGA_H__
#define __CETI_WHALE_TAG_FPGA_H__

//-----------------------------------------------------------------------------
// Libraries
//-----------------------------------------------------------------------------
#include "../utils/error.h"

#include <stdint.h> //for uint16_t

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------
#define FPGA_BITSTREAM "../config/top.bin" // fpga bitstream

#define NUM_BYTES_MESSAGE 8

/**
 * @brief Writes value to an ADC configuration register.
 *
 * @param addr ADC register address.
 * @param value
 */
#define wt_fpga_adc_write(addr, value) wt_fpga_cam(1, (addr), (value), 0, 0, NULL)

/**
 * @brief Read the value of and ADC configuration register.
 *
 * @param addr ADC register address.
 * @param pValue Pointer to return value in. Cannot be NULL.
 */
#define wt_fpga_adc_read(addr, pValue)                           \
    {                                                            \
        char fpga_response[NUM_BYTES_MESSAGE];                   \
        wt_fpga_cam(1, (0x80 | (addr)), 0, 0, 0, NULL);          \
        wt_fpga_cam(1, (0x80 | (addr)), 0, 0, 0, fpga_response); \
        *(pValue) = (fpga_response[4] << 8) | fpga_response[5];  \
    }

/**
 * @brief Syncronize ADC hardware to configuration registers.
 *
 */
#define wt_fpga_adc_sync() wt_fpga_cam(2, 0, 0, 0, 0, NULL)

/**
 * @brief Reset FPGA audio FIFO buffer
 *
 */
#define wt_fpga_fifo_reset() wt_fpga_cam(3, 0, 0, 0, 0, NULL)

/**
 * @brief Start FPGA audio FIFO buffer
 */
#define wt_fpga_fifo_start() wt_fpga_cam(4, 0, 0, 0, 0, NULL)

/**
 * @brief Stop FPGA audio FIFO buffer
 */
#define wt_fpga_fifo_stop() wt_fpga_cam(5, 0, 0, 0, 0, NULL)

/**
 * @brief opcode 0xF will do a register write on i2c1.
 *      0x59 is the 7-bit i2c addr of BMS IC,
 *      set register 0 to 0 will turn it off
 *      set register 0 to 3 to reactivate.
 *
 * @note
 *      To complete the shutdown, the Pi must be powered down now by an
 *      external process. Currently the design uses the tagMonitor script to
 *      do the Pi shutdown.
 *
 *      After the Pi turns off, the FPGA will disable discharging and charging
 *      by sending a final i2c message to the BMS chip to pull the plug.
 *
 *       A charger connection is required to wake up the tag after this event
 *       and charging/discharging needs to subsequently be renabled.
 */
#define wt_fpga_shutdown() wt_fpga_cam(0x0E, 0x6C, 0x61, 0x03, 0x00, NULL);

/**
 * @brief Sets channel bitdepth of samples from FPGA audio FIFO buffer
 *
 * @param audio_bitdepth Audio bitdepth
 */
#define wt_fpga_fifo_set_bitdepth(audio_bitdepth) wt_fpga_cam(0x11, (audio_bitdepth), 0, 0, 0, NULL)

//-----------------------------------------------------------------------------
// Metods
//-----------------------------------------------------------------------------

/**
 * @brief Initailize FPGA and associated GPIO pins
 *
 * @param fpga_bitstream_path FPGA bitstream file path
 * @return ResultFPGA
 */
WTResult wt_fpga_init(const char *fpga_bitstream_path);

/**
 * @brief Toggles reset pin to FPGA
 *
 */
void wt_fpga_reset(void);

/**
 * @brief Returns FPGA firmware vesrion.
 *
 * @return uint16_t
 */
uint16_t wt_fpga_get_version(void);

/**
 * @brief Loads specified bitsream onto FPGA
 *
 * @param fpga_bitstream_path
 * @return ResultFPGA
 */
WTResult wt_fpga_load_bitstream(const char *fpga_bitstream_path);

/**
 * @brief Sends a CAM packet to FPGA. Most interactions can be performed
 * through associated macros.
 *
 * @param opcode
 * @param arg0
 * @param arg1
 * @param pld0
 * @param pld1
 * @param pResponse pointer char buffer for FPGA response
 */
void wt_fpga_cam(unsigned int opcode, unsigned int arg0, unsigned int arg1,
                 unsigned int pld0, unsigned int pld1, char *pResponse);

#endif