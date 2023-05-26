
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

#include <stdint.h>

/* shift a value (val) but amount (s) and width (w)*/
#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1)) 
#define _LSHIFT(val, s, w) (((val) & ((1 << (w)) - 1)) << (s))

#define HZ_TO_MILLISECONDS(f) (1000/(f))


/*************************
 * Result<T>
 */
#define HAL_RESULT_PROPAGATE(x) {\
    HAL_StatusTypeDef ret = (x);\
    if(ret != HAL_OK){\
        return ret;\
    }\
}

/*************************
 * Option<T>
 */

//type declaration
#define Option(type) Option_## type

//type definition
#define OPTION_DEFINITION(type) typedef struct {\
    uint8_t some;\
    type  val;\
}Option(type)

//macros for assigning Option(type) instances
#define OPTION_NONE(type) ((Option(type)){.some = 0})
#define OPTION_SOME(type, value) ((Option(type)){.some = 1, .val = (value)})

//returns inner value if some(), else performs err_handle
#define OPTION_UNWRAP(opt, err_handle) ({\
    if(!opt.some){\
        err_handle;\
    }\
    opt.val;\
})
#endif
