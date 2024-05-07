//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, 
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "config.h"

#include "logging.h"
#include "errno.h" //for error detection for string conversion
#include <stdio.h> // for FILE
#include <stdlib.h> // for atof, atol, etc
#include <ctype.h> //for isspace

/**************************
 *
 */
TagConfig g_config = {
    .audio = {
        .filter_type = CONFIG_DEFAULT_AUDIO_FILTER_TYPE,
        .sample_rate = CONFIG_DEFAULT_AUDIO_SAMPLE_RATE,
        .bit_depth = CONFIG_DEFAULT_AUDIO_SAMPLE_RATE,
    },
    .surface_pressure   = CONFIG_DEFAULT_SURFACE_PRESSURE_BAR,
    .dive_pressure      = CONFIG_DEFAULT_DIVE_PRESSURE_BAR,
    .release_voltage_v  = CONFIG_DEFAULT_RELEASE_VOLTAGE_V,
    .critical_voltage_v = CONFIG_DEFAULT_CRITICAL_VOLTAGE_V,
    .timeout_s          = CONFIG_DEFAULT_TIMEOUT_S,
    .burn_interval_s    = CONFIG_DEFAULT_BURN_INTERVAL_S,
    .recovery           = {
        .enabled = CONFIG_DEFAULT_RECOVERY_ENABLED,
        .freq_MHz = CONFIG_DEFAULT_RECOVERY_FREQUENCY_MHZ,
        .callsign = {
            .callsign = CONFIG_DEFAULT_RECOVERY_CALLSIGN,
            .ssid = CONFIG_DEFAULT_RECOVERY_SSID,
        },
        .recipient = {
            .callsign = CONFIG_DEFAULT_RECOVERY_RECIPIENT_CALLSIGN,
            .ssid = CONFIG_DEFAULT_RECOVERY_RECIPIENT_SSID,
        },
    },
};

typedef struct {
    const char *ptr;
    size_t len;
}str;
#define STR_FROM(string) {.ptr = string, .len = strlen(string)}

typedef struct {
    str key;
    ConfigError (*parse)(const char*_String);
}ConfigList;

static ConfigError __config_parse_audio_bitdepth(const char *_String);
static ConfigError __config_parse_audio_filter_type(const char *_String);
static ConfigError __config_parse_audio_sample_rate(const char *_String);
static ConfigError __config_parse_surface_pressure(const char *_String);
static ConfigError __config_parse_dive_pressure(const char *_String);
static ConfigError __config_parse_release_voltage(const char *_String);
static ConfigError __config_parse_critical_voltage(const char *_String);
static ConfigError __config_parse_timeout(const char *_String);
static ConfigError __config_parse_burn_interval_value(const char *_String);
static ConfigError __config_parse_recovery_enable_value(const char *_String);
static ConfigError __config_parse_recovery_callsign_value(const char *_String);
static ConfigError __config_parse_recovery_recipient_value(const char *_String);
static ConfigError __config_parse_recovery_freq_value(const char *_String);
static inline const char * strtoidentifier(const char *_String, const char **_EndPtr);
/* key is the value compared to*/
/* method is what to do with the value*/
//This would have more efficient lookup as a hash table
const ConfigList config_keys[] = {
    {.key = STR_FROM("P1"),                 .parse = __config_parse_surface_pressure},
    {.key = STR_FROM("P2"),                 .parse = __config_parse_dive_pressure},
    {.key = STR_FROM("V1"),                 .parse = __config_parse_release_voltage},
    {.key = STR_FROM("V2"),                 .parse = __config_parse_critical_voltage},
    {.key = STR_FROM("T0"),                 .parse = __config_parse_timeout},
    {.key = STR_FROM("BT"),                 .parse = __config_parse_burn_interval_value},
    {.key = STR_FROM("audio_filter"),       .parse = __config_parse_audio_filter_type},
    {.key = STR_FROM("audio_bitdepth"),     .parse = __config_parse_audio_bitdepth},
    {.key = STR_FROM("audio_sample_rate"),  .parse = __config_parse_audio_sample_rate},
    {.key = STR_FROM("rec_enabled"),        .parse = __config_parse_recovery_enable_value},
    {.key = STR_FROM("rec_callsign"),       .parse = __config_parse_recovery_callsign_value},
    {.key = STR_FROM("rec_recipient"),      .parse = __config_parse_recovery_recipient_value},
    {.key = STR_FROM("rec_freq"),           .parse = __config_parse_recovery_freq_value},
};

/* Private Methods ***********************************************************/
static ConfigError __config_parse_audio_sample_rate(const char *_String) {
    char *end_ptr;
    unsigned long sample_rate_kHz = strtoul(_String, &end_ptr, 0);
    if (end_ptr == _String){
        return CONFIG_ERR_INVALID_VALUE;
    }

    if (sample_rate_kHz > 192){
        return CONFIG_ERR_INVALID_VALUE;
    } 
    
    if (sample_rate_kHz > 96) {
        g_config.audio.sample_rate = AUDIO_SAMPLE_RATE_192KHZ;
        CETI_DEBUG("Audio sample rate set to 192 kHz");
    } else if (sample_rate_kHz > 48) {
        g_config.audio.sample_rate = AUDIO_SAMPLE_RATE_96KHZ;
        CETI_DEBUG("Audio sample rate set to 96 kHz");
    } else if (sample_rate_kHz > 0) {
        g_config.audio.sample_rate = AUDIO_SAMPLE_RATE_48KHZ;
        CETI_DEBUG("Audio sample rate set to 48 kHz");
    } else {
        g_config.audio.sample_rate = AUDIO_SAMPLE_RATE_DEFAULT;
        CETI_DEBUG("Audio sample rate set to 750 Hz");
    }

    return CONFIG_OK;
}

static ConfigError __config_parse_audio_bitdepth(const char *_String) {
    char *end_ptr;
    unsigned long bitdepth = strtoul(_String, &end_ptr, 0);
    if (end_ptr == _String){
        return CONFIG_ERR_INVALID_VALUE;
    }

    if (bitdepth <= 16){
        g_config.audio.bit_depth = AUDIO_BIT_DEPTH_16;
        CETI_DEBUG("Audio sample bit depth set to 16 bits");
    } else {
        g_config.audio.bit_depth = AUDIO_BIT_DEPTH_24;
        CETI_DEBUG("Audio sample bit depth set to 24 bits");
    }
    
    return CONFIG_OK;
}

static ConfigError __config_parse_audio_filter_type(const char *_String) {
    const char *end_ptr = NULL;
    char case_insensitive[12] = "";
    const char *value_str = strtoidentifier(_String, &end_ptr);
    size_t value_len = 0;
    if(value_str == NULL){
        CETI_DEBUG("No value found");
        return CONFIG_ERR_INVALID_VALUE;
    }
    value_len = (end_ptr - value_str);
    
    //only 2 options
    if((value_len != 5) && (value_len != 8)){
        CETI_DEBUG("Unknown length %d", value_len);
        return CONFIG_ERR_INVALID_VALUE;
    }

    //case insensitive
    for(int i = 0; i < value_len; i++){
        case_insensitive[i] = tolower(value_str[i]);
    }

    if ((value_len == 5) && (memcmp("sinc5", case_insensitive, 5) == 0)){
        g_config.audio.filter_type = AUDIO_FILTER_SINC5;
        CETI_DEBUG("audio filter set to sinc5 filter");
        return CONFIG_OK;
    } else if((value_len == 8) && (memcmp("wideband", case_insensitive, 8) == 0)) {
        g_config.audio.filter_type = AUDIO_FILTER_WIDEBAND;
        CETI_DEBUG("audio filter set to wideband filter");
        return CONFIG_OK;
    }
    CETI_DEBUG("Unknown value %s", case_insensitive);
    return CONFIG_ERR_INVALID_VALUE;
}

static ConfigError __config_parse_dive_pressure(const char *_String){
    char *end_ptr;
    float parsed_value;

    errno = 0;
    parsed_value = strtof(_String, &end_ptr);
    if(parsed_value == 0.0f){
        if( (_String == end_ptr) || (errno == ERANGE) ){
            return CONFIG_ERR_INVALID_VALUE;
        }
    }

    //ToDo: Check acceptable range

    g_config.dive_pressure = parsed_value;
    CETI_DEBUG("dive pressure %.2f bar", parsed_value);
    return CONFIG_OK;
}

static ConfigError __config_parse_surface_pressure(const char *_String){
    char *end_ptr;
    float parsed_value;

    errno = 0;
    parsed_value = strtof(_String, &end_ptr);
    if(parsed_value == 0.0f){
        if( (_String == end_ptr) || (errno == ERANGE) ){
            return CONFIG_ERR_INVALID_VALUE;
        }
    }

    //ToDo: Check acceptable range
    g_config.surface_pressure = parsed_value;
    return CONFIG_OK;
}

static ConfigError __config_parse_release_voltage(const char *_String){
    char *end_ptr;
    float parsed_value;

    //try reading a float
    errno = 0;
    parsed_value = strtof(_String, &end_ptr);
    if(parsed_value == 0.0f){
        if( (_String == end_ptr) || (errno == ERANGE) ){
            return CONFIG_ERR_INVALID_VALUE;
        }
    }

    //Check acceptable range
    if(parsed_value > 8.4) return CONFIG_ERR_INVALID_VALUE;
    if(parsed_value < 6.2) return CONFIG_ERR_INVALID_VALUE;
    
    //assign value
    g_config.release_voltage_v = parsed_value;
    CETI_DEBUG("release voltage set to %.2fV", parsed_value);
    return CONFIG_OK;
}

static ConfigError __config_parse_critical_voltage(const char *_String){
    char *end_ptr;
    float parsed_value;

    //try reading a float
    errno = 0;
    parsed_value = strtof(_String, &end_ptr);
    if(parsed_value == 0.0f){
        if( (_String == end_ptr) || (errno == ERANGE) ){
            return CONFIG_ERR_INVALID_VALUE;
        }
    }

    //Check acceptable range
    if(parsed_value > 8.4) return CONFIG_ERR_INVALID_VALUE;
    if(parsed_value < 6.2) return CONFIG_ERR_INVALID_VALUE;

    //assign value
    g_config.critical_voltage_v = parsed_value;
    CETI_DEBUG("critical voltage set to %.2fV", parsed_value);
    return CONFIG_OK;
}

static ConfigError __config_parse_timeout(const char *_String){
    char *end_ptr;
    time_t parsed_value;

    errno = 0;
    parsed_value = strtotime_s(_String, &end_ptr);
    if(parsed_value == 0){
        if( (_String == end_ptr) || (errno == ERANGE) ){
            return CONFIG_ERR_INVALID_VALUE;
        }
    }

    g_config.timeout_s = parsed_value;
    CETI_DEBUG("time release set to %ld seconds", parsed_value);
    return CONFIG_OK;
}

static ConfigError __config_parse_burn_interval_value(const char *_String){
    char *end_ptr;
    time_t parsed_value;

    errno = 0;
    parsed_value = strtotime_s(_String, &end_ptr);
    if(parsed_value == 0){
        if( (_String == end_ptr) || (errno == ERANGE) ){
            return CONFIG_ERR_INVALID_VALUE;
        }
    }

    //ToDo: Error Checking
    g_config.burn_interval_s = parsed_value;
    CETI_DEBUG("burn interval release set to %ld seconds", parsed_value);
    return CONFIG_OK;
}

static ConfigError __config_parse_recovery_enable_value(const char *_String){
    g_config.recovery.enabled = strtobool_s(_String, NULL);
    #ifdef DEBUG
    if(g_config.recovery.enabled) {
        CETI_DEBUG("recovery board enabled");
    } else {
        CETI_DEBUG("recovery board disabled");
    }
    #endif
    return CONFIG_OK;
}

static ConfigError __config_parse_recovery_callsign_value(const char *_String) {
    int result = callsign_try_from_str(&g_config.recovery.callsign, _String, NULL);
    if(result != 0) {
        return CONFIG_ERR_INVALID_VALUE;
    }
    CETI_DEBUG("tag callsign set to \"%s-%d\"", g_config.recovery.callsign.callsign, g_config.recovery.callsign.ssid);
    return CONFIG_OK;
}

static ConfigError __config_parse_recovery_recipient_value(const char *_String) {
    int result = callsign_try_from_str(&g_config.recovery.recipient, _String, NULL);
    if(result != 0) {
        return CONFIG_ERR_INVALID_VALUE;
    }
    CETI_DEBUG("message recipient callsign set to \"%s-%d\"", g_config.recovery.recipient.callsign, g_config.recovery.recipient.ssid);
    return CONFIG_OK;
}

static ConfigError __config_parse_recovery_freq_value(const char *_String){
    char *end_ptr;
    float f_MHz = strtof(_String, &end_ptr);
    if ( (f_MHz < 134.0000) || (f_MHz > 174.0000)){
        return CONFIG_ERR_INVALID_VALUE;
    }
    
    g_config.recovery.freq_MHz = f_MHz;
    CETI_DEBUG("aprs frequency set to %.3f MHz", f_MHz);
    return CONFIG_OK;
}

int strtobool_s(const char *_String, const char **_EndPtr){
    const char *value_str = strtoidentifier(_String, _EndPtr);
    if (value_str == NULL){
        return 0;
    }
    
    return (memcmp("true", value_str, 4) == 0) 
           || (memcmp("TRUE", value_str, 4) == 0)
           || (memcmp("True", value_str, 4) == 0);
}

time_t strtotime_s(const char *_String, char **_EndPtr){
    char *unit_str_ptr;

    errno = 0; //reset errno;
    time_t value = strtoll(_String, &unit_str_ptr, 0);
    
    if(errno){ //error occured
        if(_EndPtr != NULL){
            *_EndPtr = unit_str_ptr;
        }
        return 0;
    }

    if(unit_str_ptr == NULL){
        if(_EndPtr != NULL){
            *_EndPtr = unit_str_ptr;
        }
        return value*60;
    }

    //remove whitespace
    while(isspace(*unit_str_ptr)){unit_str_ptr++;} 

    //convert time to seconds based on units
    switch(*unit_str_ptr){
        default:
            if(_EndPtr != NULL){ 
                *_EndPtr = unit_str_ptr;
            }
            return value * 60;

        case 's': //second
        case 'S':
            if(_EndPtr != NULL){ 
                *_EndPtr = unit_str_ptr + 1;
            }
            return value;

        case 'm': //minute
        case 'M':
            if(_EndPtr != NULL){ 
                *_EndPtr = unit_str_ptr + 1;
            }
            return value * 60;


        case 'h': //hour
        case 'H':
            if(_EndPtr != NULL){ 
                *_EndPtr = unit_str_ptr + 1;
            }
            return value*60*60;

        case 'd': //day
        case 'D':
            if(_EndPtr != NULL){ 
                *_EndPtr = unit_str_ptr + 1;
            }
            return value*24*60*60;
    }
}

static inline 
const char * strtoidentifier(const char *_String, const char **_EndPtr){
    errno = 0; //reset errno;

    if(_String == NULL){
        return 0; //invalid _String
    }
    
    //skip whitespace
    while(isspace(*_String)){_String++;} 

    const char *identifier_start = _String;
    //look for start character
    if(!isalpha(*_String) && (*_String != '_')){
        if (_EndPtr != NULL){
            *_EndPtr = _String;
        }
        return 0; //not an identifier
    }
    _String++;
    
    //find end of String
    while(isalnum(*_String) || (*_String == '_')){
        _String++;
    }
    if (_EndPtr != NULL){
        *_EndPtr = _String;
    }
    return identifier_start;
}

int config_parse_line(const char *_String){
    //try to parse <config_assignment>

    //store <identifier>
    const char *end_ptr;
    const char *start_ptr = strtoidentifier(_String, &end_ptr);
    if(start_ptr == NULL) //comments caught here
        return CONFIG_OK; //line did not start with an identifier

    str identifier = {
        .ptr = start_ptr,
        .len = end_ptr - start_ptr,
    };
    _String = end_ptr;

    //skip whitespace
    while(isspace(*_String)){_String++;} 
    
    // '='
    if(*_String != '='){
        return CONFIG_ERR_MISSING_ASSIGN_OP; //no assignment operator
    }
    _String++;
    
    // try to parse rhv based on lhv
    for(int i = 0; i < sizeof(config_keys)/sizeof(config_keys[0]); i++){
        const str *i_key = &config_keys[i].key;
        //look for match in possible keys
        if( (i_key->len == identifier.len) 
            && (memcmp(i_key->ptr, identifier.ptr, i_key->len) == 0)
        ){
            return config_keys[i].parse(_String);
        }
    }
    return CONFIG_ERR_UNKNOWN_KEY; //unknown key
}

/*
 * Read a config file and 
 */
int config_read(const char * filename){
    FILE *ceti_config_file;

    // Load the deployment configuration
    CETI_LOG("Read the deployment parameters from %s", filename);
    ceti_config_file = fopen(filename, "r");
    if (ceti_config_file == NULL) {
        CETI_ERR("Cannot open configuration file %s", filename);
        return (-1);
    }
    
    //parse lines
    char line[256];
    uint32_t line_number = 1;
    while (fgets(line, 256, ceti_config_file) != NULL) {
        int config_result = config_parse_line(line);
        switch(config_result){
            case CONFIG_OK:
                break;
            case CONFIG_ERR_UNKNOWN_KEY:
                CETI_WARN("%s: L%d: skipped unrecognized key: \"%s\"", filename, line_number, line);
                break;
            case CONFIG_ERR_MISSING_ASSIGN_OP:
                CETI_WARN("%s: L%d: skipped no '=': \"%s\"", filename, line_number, line);
                break;
            case CONFIG_ERR_INVALID_VALUE:
                CETI_WARN("%s: L%d: invalid value: \"%s\"", filename, line_number, line);
                break;
            default:
                break;
        }
        line_number++;
    }

    fclose(ceti_config_file);
    return 0;
}
