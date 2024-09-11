//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef UTILS_STR_H
#define UTILS_STR_H

#include <unistd.h> // for size_t
#include <string.h> // for strlen()

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef struct {
    const char *ptr;
    size_t len;
}str;
#define STR_FROM(string) {.ptr = string, .len = strlen(string)}

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int strtobool(const char *_String, const char **_EndPtr);
const char * strtoidentifier(const char *_String, const char **_EndPtr);
const char * strtoquotedstring(const char *_String, const char ** _EndPtr);
#endif