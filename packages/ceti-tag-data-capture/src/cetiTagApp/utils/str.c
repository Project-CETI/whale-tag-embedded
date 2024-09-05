#include "str.h"

#include <ctype.h> //for isspace(), isalpha(), isalnum()
#include <errno.h> //for errno

int strtobool(const char *_String, const char **_EndPtr) {
    const char *end_ptr;
    const char *value_str = strtoidentifier(_String, &end_ptr);
    if (value_str == NULL){
        return 0;
    }

    //update end ptr
    if(_EndPtr != NULL){
        *_EndPtr = end_ptr;
    }
    
    if((end_ptr - value_str) != 4){
        //can't be true based on length
        return 0;
    }
    
    char case_insensitive[5] = {0};
    for(int i = 0; i < 4; i++){
        case_insensitive[i] = tolower(value_str[i]);
    }

    return (memcmp("true", case_insensitive, 4) == 0);
}

const char * strtoidentifier(const char *_String, const char **_EndPtr) {
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

