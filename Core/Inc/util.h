#ifndef INC_UTIL_H_
#define INC_UTIL_H_
#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1))
#define _LSHIFT(val, s, w) (((val) & ((1 << (w)) - 1)) << (s))
#endif