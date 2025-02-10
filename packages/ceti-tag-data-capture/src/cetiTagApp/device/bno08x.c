//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: Device driver for CEVA BNO086 IMU
//-----------------------------------------------------------------------------

#include "bno08x.h"
#include "gpio.h"
#include "i2c.h"

#include <pigpio.h>
#include <string.h>

#define IMU_OPEN_TIMEOUT 20000

#define IMU_SHTP_REPORT_SET_FEATURE_COMMAND 0xFD

uint8_t shtp_channel_seq_num[SHTP_CH_COUNT] = {0};

WTResult wt_bno08x_close(void) {
    PI_TRY(WT_DEV_IMU, bbI2CClose(IMU_BB_I2C_SDA));
    return WT_OK;
}

WTResult wt_bno08x_enable_report(uint16_t report_id, uint32_t report_interval_us) {
    char setFeatureCommand[17] = {0};

    // Populate the Set Feature Command (see 6.5.4 of "SH-2 Reference Manual")
    setFeatureCommand[0] = IMU_SHTP_REPORT_SET_FEATURE_COMMAND;
    setFeatureCommand[1] = report_id;
    setFeatureCommand[2] = 0;                         // feature flags
    setFeatureCommand[3] = 0;                         // change sensitivity LSB
    setFeatureCommand[4] = 0;                         // change sensitivity MSB
    setFeatureCommand[5] = report_interval_us & 0Xff; // Report Interval uS LSB
    setFeatureCommand[6] = (report_interval_us >> 8) & 0Xff;
    setFeatureCommand[7] = (report_interval_us >> 16) & 0Xff;
    setFeatureCommand[8] = (report_interval_us >> 24) & 0Xff; // Report Interval uS MSB
    setFeatureCommand[9] = 0;                                 // batch interval LSB
    setFeatureCommand[10] = 0;
    setFeatureCommand[11] = 0;
    setFeatureCommand[12] = 0; // batch interval MSB
    setFeatureCommand[13] = 0; // sensor-specific configuration word LSB
    setFeatureCommand[14] = 0;
    setFeatureCommand[15] = 0;
    setFeatureCommand[16] = 0; // sensor-specific configuration word MSB
    return wt_bno08x_write_shtp_packet(SHTP_CH_CONTROL, setFeatureCommand, sizeof(setFeatureCommand));
}

WTResult wt_bno08x_open(void) {
    PI_TRY(WT_DEV_IMU, bbI2COpen(IMU_BB_I2C_SDA, IMU_BB_I2C_SCL, IMU_OPEN_TIMEOUT));
    return WT_OK;
}

WTResult wt_bno08x_read(void *buffer, size_t buffer_len) {
    // starts at index 3 so memcpy can be more efficient
    uint8_t header[] = {
        [0] = 0x04,                                 // set addr
        [1] = IMU_I2C_DEV_ADDR,                     // imu addr
        [2] = 0x02,                                 // start
        [3] = 0x01,                                 // escape
        [4] = 0x06,                                 // read
        [5] = (((uint16_t)buffer_len) & 0xFF),      // length lsb
        [6] = (((uint16_t)buffer_len >> 8) & 0xFF), // length msb
        [7] = 0x03,                                 // stop
        [8] = 0x00,                                 // end
    };

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, (char *)header, sizeof(header), buffer, buffer_len));
    return WT_OK;
}

WTResult wt_bno08x_read_header(ShtpHeader *pHeader) {
    return wt_bno08x_read(pHeader, sizeof(ShtpHeader));
}

WTResult wt_bno08x_read_shtp_packet(void *pPacket, size_t packet_len) {
    ShtpHeader header = {};
    WT_TRY(wt_bno08x_read_header(&header));
    let num_bytes = (header->length < packet_len) ? header->length : packet_len;
    WT_TRY(wt_bno08x_read(pPacket, num_bytes));
    return WT_OK;
}

// ToDo: implement FRS read


void wt_bno08x_reset_hard(void) {
    gpioSetMode(IMU_N_RESET, PI_OUTPUT);
    usleep(10000);
    gpioWrite(IMU_N_RESET, PI_HIGH);
    usleep(100000);
    gpioWrite(IMU_N_RESET, PI_LOW); // reset the device (active low reset)
    usleep(100000);
    gpioWrite(IMU_N_RESET, PI_HIGH);
    usleep(500000); // if this is about 150000 or less, the first feature report fails to start
}

WTResult wt_bno08x_reset_soft(void) {
    uint8_t reset_cmd = 1;
    return wt_bno08x_write_shtp_packet(SHTP_CH_EXE, &reset_cmd, 1);
    // ToDo: check when reset complete
}

WTResult wt_bno08x_write(void *buffer, size_t buffer_len) {
    // starts at index 3 so memcpy can be more efficient
    uint8_t i2c_packet[256 + 7] = {
        [0] = 0x04,             // set addr
        [1] = IMU_I2C_DEV_ADDR, // imu addr
        [2] = 0x02,             // start
        [3] = 0x07,             // write
        [4] = buffer_len,       // length
    };
    memcpy(&i2c_packet[5], buffer, buffer_len);
    i2c_packet[5 + buffer_len] = 0x03; // stop
    i2c_packet[6 + buffer_len] = 0x00; // end

    PI_TRY(WT_DEV_IMU, bbI2CZip(IMU_BB_I2C_SDA, (void *)i2c_packet, buffer_len + 7, NULL, 0));
    return WT_OK;
}

WTResult wt_bno08x_write_shtp_packet(ShtpChannel channel, void *pPacket, size_t packet_len) {
    // Populate the SHTP header (see 2.2.1 of "Sensor Hub Transport Protocol")
    ShtpHeader shtp_header = {
        .length = sizeof(ShtpHeader) + packet_len,
        .channel = channel,
        .seq_num = shtp_channel_seq_num[channel]++,
    };

    uint8_t shtp_packet[256] = {};
    memcpy(&shtp_packet[0], &shtp_header, sizeof(ShtpHeader));
    memcpy(&shtp_packet[4], pPacket, packet_len);

    return wt_bno08x_write(shtp_packet, packet_len + sizeof(ShtpHeader));
}

WTResult wt_bno08x_frs_write(uint16_t offset, void * data, size_t data_size) {
    const uint8_t frs_w_request[] = {
        [0] = 0xF7, // report_ID
        // [1] = reserved
        [2] = data_size & 0xFF,        // length LSB
        [3] = (data_size >> 8) & 0xFF, // length MSB
        [4] = offset & 0xFF,           // offset LSB
        [5] = (offset >> 8) & 0xFF,    // offset MSB
    };
    uint8_t frs_w_data_request[256] = {
        [0] = 0xF6, // report_ID
        // [1] = reserved
        [2] = offset & 0xFF,        // offset LSB
        [3] = (offset >> 8) & 0xFF, // offset MSB
    };
    memcpy(&frs_w_data_request[4], data, data_size);

    WT_TRY(wt_bno08x_write_shtp_packet(SHTP_CH_CONTROL, (void *)frs_w_request, sizeof(frs_w_request)));
    WT_TRY(wt_bno08x_write_shtp_packet(SHTP_CH_CONTROL, (void *)frs_w_data_request, sizeof(frs_w_data_request)));
}

WTResult wt_bno08x_set_system_orientation(double w, double x, double y, double z) {
    int32_t w_fp = (int32_t)(w * 2.0e30);
    int32_t x_fp = (int32_t)(x * 2.0e30);
    int32_t y_fp = (int32_t)(y * 2.0e30);
    int32_t z_fp = (int32_t)(z * 2.0e30);

    uint8_t unit_quaternion[] = {
        [0] = ((x_fp >> 0) & 0xFF), // x LSB
        [1] = ((x_fp >> 8) & 0xFF),
        [2] = ((x_fp >> 16) & 0xFF),
        [3] = ((x_fp >> 24) & 0xFF), // x MSB
        [4] = ((y_fp >> 0) & 0xFF),  // y LSB
        [5] = ((y_fp >> 8) & 0xFF),
        [6] = ((y_fp >> 16) & 0xFF),
        [7] = ((y_fp >> 24) & 0xFF), // y MSB
        [8] = ((z_fp >> 0) & 0xFF),  // z LSB
        [9] = ((z_fp >> 8) & 0xFF),
        [10] = ((z_fp >> 16) & 0xFF),
        [11] = ((z_fp >> 24) & 0xFF), // z MSB
        [12] = ((w_fp >> 0) & 0xFF),  // w LSB
        [13] = ((w_fp >> 8) & 0xFF),
        [14] = ((w_fp >> 16) & 0xFF),
        [15] = ((w_fp >> 24) & 0xFF), // w MSB
    };

    return wt_bno08x_frs_write(BNO08X_FRS_ID_SYSTEM_ORIENTATION, unit_quaternion, sizeof(unit_quaternion));
}
