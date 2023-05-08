
/*
 * util.c
 *
 *  Created on: May 02, 2023
 *      Author: Michael Salino-Hugg (email: msalinohugg@seas.harvard.edu)
 *
 *  Description:
 *    Useful general macros and functions
 */

#ifndef INC_UTIL_H_
#define INC_UTIL_H_

/* shift a value (val) but amount (s) and width (w)*/
#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1))
#define _LSHIFT(val, s, w) (((val) & ((1 << (w)) - 1)) << (s))

#define HZ_TO_MILLISECONDS(f) (1000/(f))

#endif
