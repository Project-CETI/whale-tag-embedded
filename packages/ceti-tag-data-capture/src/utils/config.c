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

typedef enum config_error_e {
    CONFIG_OK = 0,
    CONFIG_ERR_UNKNOWN_KEY = 1,
    CONFIG_ERR_INVALID_VALUE = 2,
    CONFIG_ERR_MISSING_ASSIGN_OP = 3,
} ConfigError;

typedef struct {
    const char *ptr;
    size_t len;
}str;
#define STR_FROM(string) {.ptr = string, .len = strlen(string)}

typedef struct {
    str key;
    ConfigError (*parse)(const char*_String);
}ConfigList;

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

/* key is the value compared to*/
/* method is what to do with the value*/
//This would have more efficient lookup as a hash table
const ConfigList config_keys[] = {
    {.key = STR_FROM("P1"),            .parse = __config_parse_surface_pressure},
    {.key = STR_FROM("P2"),            .parse = __config_parse_dive_pressure},
    {.key = STR_FROM("V1"),            .parse = __config_parse_release_voltage},
    {.key = STR_FROM("V2"),            .parse = __config_parse_critical_voltage},
    {.key = STR_FROM("T0"),            .parse = __config_parse_timeout},
    {.key = STR_FROM("BT"),            .parse = __config_parse_burn_interval_value},
    {.key = STR_FROM("rec_enabled"),   .parse = __config_parse_recovery_enable_value},
    {.key = STR_FROM("rec_callsign"),  .parse = __config_parse_recovery_callsign_value},
    {.key = STR_FROM("rec_recipient"), .parse = __config_parse_recovery_recipient_value},
    {.key = STR_FROM("rec_freq"),      .parse = __config_parse_recovery_freq_value},
};

/* Private Methods ***********************************************************/
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
    return CONFIG_OK;
}

static ConfigError __config_parse_recovery_enable_value(const char *_String){
    

    return CONFIG_OK;
}

static ConfigError __config_parse_recovery_callsign_value(const char *_String) {
    int result = callsign_try_from_str(&g_config.recovery.callsign, _String, NULL);
    return (result == 0) ? CONFIG_OK : CONFIG_ERR_INVALID_VALUE;
}

static ConfigError __config_parse_recovery_recipient_value(const char *_String) {
    int result = callsign_try_from_str(&g_config.recovery.recipient, _String, NULL);
    return (result == 0) ? CONFIG_OK : CONFIG_ERR_INVALID_VALUE;
}

static ConfigError __config_parse_recovery_freq_value(const char *_String){
    char *end_ptr;
    float f_MHz = strtof(_String, &end_ptr);
    if ( (f_MHz < 134.0000) || (f_MHz > 174.0000)){
        return CONFIG_ERR_INVALID_VALUE
    }
    
    g_config.recovery.freq_MHz = f_MHz;
    return CONFIG_OK;
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
        CETI_LOG("XXXX Cannot open configuration file %s", filename);
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
                CETI_LOG("%s: L%d: skipped unrecognized key: \"%s\"", filename, line_number, line);
                break;
            case CONFIG_ERR_MISSING_ASSIGN_OP:
                CETI_LOG("%s: L%d: skipped no '=': \"%s\"", filename, line_number, line);
                break;
            case CONFIG_ERR_INVALID_VALUE:
                CETI_LOG("%s: L%d: invalid value: \"%s\"", filename, line_number, line);
                break;
            default:
                break;
        }
        line_number++;
    }

    fclose(ceti_config_file);
    return 0;
}
