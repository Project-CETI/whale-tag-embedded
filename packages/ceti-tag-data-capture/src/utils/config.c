//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, 
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "config.h"

#include "utils/logging.h"
#include <stdio.h> // for FILE
#include <stdlib.h> // for atof, atol, etc

TagConfig g_tag_config = {
    // .dive_pressure,
    // .surface_pressure,
    .release_voltage_v = 6.4,
    .critical_voltage_v = 6.2,
    .timeout_s = 24*60*60, //
    .burn_interval_s = 5*60, //5 Minutes
};

static int64_t atotime_s(const char *_String){
    char *unit_str_ptr;
    int64_t value = strtoll(_String, &unit_str_ptr, 0);
    if(unit_str_ptr != NULL){
        switch(*unit_str_ptr){
            case 's':
            case 'S':
                return value;

            case 'm':
            case "M":
            default:
                return value * 60;

            case 'h':
            case 'H':
                return value*60*60;

            case 'd':
            case 'D':
                return value*24*60*60;
        }
    }else{
        return value * 60;
    }
}

int config_read(const char * filename){
    FILE *ceti_config_file;

    // Load the deployment configuration
    CETI_LOG("Read the deployment parameters from %s", CETI_CONFIG_FILE);
    ceti_config_file = fopen(CETI_CONFIG_FILE, "r");
    if (ceti_config_file == NULL) {
        CETI_LOG("XXXX Cannot open configuration file %s", CETI_CONFIG_FILE);
        return (-1);
    }
    
    //parse lines
    char line[256];
    char *pTemp;
    while (fgets(line, 256, ceti_config_file) != NULL) {

        if (*line == '#')  // ignore comment lines
            continue; 
        else if (!strncmp(line, "P1=", 3)) 
            g_tag_config.surface_pressure = strtof(line + 3, NULL);
        else if (!strncmp(line, "P2=", 3)) 
            g_tag_config.dive_pressure = strtof(line + 3, NULL);
        else if (!strncmp(line, "V1=", 3)) 
            g_tag_config.release_voltage_v = strtof(line + 3, NULL); //release_voltage
        else if (!strncmp(line, "V2=", 3)) 
            g_tag_config.critical_voltage_v = strtof(line + 3, NULL); //critical_voltage
        else if (!strncmp(line, "T0=", 3)) 
            g_tag_config.timeout_s = atotime_s(line + 3);
        else if (!strncmp(line, "BT=", 3)) 
            g_tag_config.burn_interval_seconds = atotime_s(line + 3);
    }

    fclose(ceti_config_file);
    return 0;
}


/* Recovery board is stored as a json
 * 
 */
int recovery_config_parse(const char * filename){

}