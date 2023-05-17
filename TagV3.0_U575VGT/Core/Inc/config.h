/*
 * config.h
 *
 *  Created on: Apr 27, 2023
 *      Author: Michael Salino-Hugg (email: msalinohugg@seas.harvard.edu)
 * 
 * Description:
 *   this header is used to read/write runtime configuration parameters from a 
 *  "config.txt" file saved to the tag's SD card.
 * 
 *  - comment lines start with '#'
 *  - there can only be 1 key/value pair per line
 *  - ':' is used to seperate the key and value
 *  - whitespace is ignored
 *  - keys and values consist of only alphanumberical digits, '_', '-', '+', and '.'
 *  - anything on a lin following a key/value pair is ignored
 *  - any parameters not specified will be set to their default values
 *  - the last instance of a parameter is the one that will be used
 *  - invalid lines are ignored
 * 
 ***** example config.txt *****************************
 *
 *  # this is a comment
 *  audio_channel_0: enabled
 *  audio_channel_1: disabled
 * 
 *  audio_depth: 16_bit
 *  aduio_channel_1: enabled  #This overrides the previous definition
 *  
 ***** valid/key value pairs **************************
 *
 * key: audio_channel_0
 * values: enabled, disabled
 * default: enabled
 * desc: enables/disables hydrophone on channel 0.
 * 
 * key: audio_channel_1
 * values: enabled, disabled
 * default: enabled
 * desc: enables/disables hydrophone on channel 1.
 * 
 * key: audio_channel_2
 * values: enabled, disabled
 * default: enabled
 * desc: enables/disables hydrophone on channel 2.
 * 
 * key: audio_channel_3
 * values: enabled, disabled
 * default: disabled
 * desc: enables/disables hydrophone on channel 3.
 * 
 * key: audio_depth
 * values: 16_bit, 24_bit
 * default: 24_bit
 * desc: sets audio bit depth as 16 or 24 bits.
 * 
 * key: audio_ch_headers
 * values: enabled, disabled
 * default: enabled
 * desc: stores the audio channel headers at the start of each sample.
 * 
 * key: audio_sample_rate
 * values: 96_khz, 192_khz
 * default: 96_khz
 * desc: sets the audio sample rate.
 * 
 * key: sensor_light_rate
 * values: {int: [0..30]} ??? is this range good
 * default: 1
 * desc: sampling rate of light sensor in Hz, 0 = disabled
 * 
 * key: sensor_depth_rate
 * values: {int: [0..30]} ??? is this range good
 * default: 1
 * desc: sampling rate of depth sensor in Hz, 0 = disabled
 * 
 * key: sensor_imu_rate
 * values: {int: [0..30]} ??? is this range good
 * default: 1
 * desc: sampling rate of IMU in Hz, 0 = disabled
 * 
 * key: burn_wire_timer
 * values: {int: [0..72]} ??? is this range good
 * default: 0
 * desc: hours before burnwire triggers, 0 = disabled
 * 
 **************************************************************/

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#include "app_filex.h"
#include "stdbool.h"
#include <stdint.h>

typedef enum {
    CFG_AUDIO_RATE_96_KHZ,
    CFG_AUDIO_RATE_192_KHZ,
}TagConfigAudioSampleRate;

typedef enum {
    CFG_AUDIO_DEPTH_16_BIT,
    CFG_AUDIO_DEPTH_24_BIT,
}TagConfigAudioSampleDepth;

typedef struct tag_config_s{
    uint8_t                     audio_ch_enabled[4];
    uint8_t                     audio_ch_headers;
    TagConfigAudioSampleRate    audio_rate;
    TagConfigAudioSampleDepth   audio_depth;
} TagConfig;

/* Set tag configuration to default settings */
void TagConfig_default(TagConfig *cfg);

/* Read tag configuration from file */
void TagConfig_read(TagConfig *cfg, FX_FILE *cfg_file);

/* Write tag configuration to file */
//uint32_t TagConfig_write(TagConfig *cfg, FX_FILE *cfg_file);

#endif /* INC_CONFIG_H_ */
