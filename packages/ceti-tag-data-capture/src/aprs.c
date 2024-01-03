#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "aprs.h"

/* Tries parsing a string into an APRSCallsign
 *
 * @params:     dst:   _O: destination callsign pointer. Only set if valid callsign;
 *          _String:   _I: pointer to str to be parsed
 *          _EndPtr: [_O]: set to end of callsign if valid
 * @return:  0 if valid callsign string
 *          -1 if invalid callsign string
 */
int callsign_try_from_str(APRSCallsign *dst, const char *_String, char **_EndPtr) {
    APRSCallsign tmp = {};
    if(_EndPtr != NULL){
        *_EndPtr = _String;
    }
	// skip white space
    while(isspace(*_String)){_String++;} 
    
    // get callsign
    const char *callsign_start = _String;
    const char *callsign_end;
    size_t callsign_len = 0;

    while(isalnum(*_String)){_String++;}
    callsign_end = _String;
    callsign_len = (callsign_end - callsign_start);
    if( (callsign_len == 0) || (callsign_len > 6)){
        return -1; //invalid callsign
    }

    memcpy(tmp.callsign, callsign_start, callsign_len);
    tmp.callsign[callsign_len] = '\0';

    // get optional ssid
    if(*_String != '-'){
        if(_EndPtr != NULL){
            *_EndPtr = _String;
        }
        tmp.ssid = 0;
    } else {
        uint32_t ssid = 0;
        _String++;
        ssid = strtoul(_String, _EndPtr, 0);
        if(ssid > 15){
            return -1; //invalid ssid
        }
        tmp.ssid = ssid;
    }
    *dst = tmp;
    return 0;
}

void callsign_to_str(APRSCallsign *self, char str[static 10]){
    if(self->ssid == 0) {
        strncpy(str, self->callsign, 6);
    } else {
        snprintf(str, 10, "%s-%d", self->callsign, self->ssid);
    }
}