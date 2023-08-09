/*
 * timing.h
 *
 *  Created on: Jul 5, 2023
 *      Author: Kaveet
 *
 * A library file containing helpful functions, macros and constants related to timing. This could include RTOS Software Timers, delays, etc.
 */

#ifndef INC_LIB_INC_TIMING_H_
#define INC_LIB_INC_TIMING_H_

#include "tx_user.h"

//A macro for converting seconds to threadX ticks. This can be used to feed into software timers, task sleeps, etc.
#define tx_s_to_ticks(S) ((S) * (TX_TIMER_TICKS_PER_SECOND))

//A macro for converting milliseconds to threadX ticks. This can be used to feed into software timers, task sleeps, etc.
#define tx_ms_to_ticks(MS) ((MS) * (TX_TIMER_TICKS_PER_SECOND) / 1000)

//A macro for converting microseconds to threadX ticks. This can be used to feed into software timers, task sleeps, etc.
#define tx_us_to_ticks(US) ((US) * (TX_TIMER_TICKS_PER_SECOND) / 1000000)

#endif /* INC_LIB_INC_TIMING_H_ */
