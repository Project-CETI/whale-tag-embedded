#ifndef RECOVERY_APRS_H
#define RECOVERY_APRS_H

#include <stdint.h>
#include <string.h> // for memset() and other string functions
#include <unistd.h>

typedef struct aprs_callsign_t {
	char callsign[7];
	uint8_t ssid;
} APRSCallsign;

/**
 *  Tries parsing a string into an APRSCallsign
 *
 * @params:     dst:   _O: destination callsign pointer. Only set if valid callsign;
 *          _String:   _I: pointer to str to be parsed
 *          _EndPtr: [_O]: set to end of callsign if valid
 * @return:  0 if valid callsign string
 *          -1 if invalid callsign string
 */
int callsign_try_from_str(APRSCallsign *dst, const char *_String, char **_EndPtr);
void callsign_to_str(APRSCallsign *self, char str[static 10]);

#endif