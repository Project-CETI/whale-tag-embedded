#ifndef __ACQ_DECAY_H__
#define __ACQ_DECAY_H__

#include <stdint.h>

#include "../utils/error.h" // for WTResult

typedef struct {
    uint32_t grace_count;             // how many consecutive errors can occur prior to decay kicking in
    uint32_t skip_count;              // current count of sample intervals skipped
    uint32_t decay_multiplier;        // number of samples to skip
    uint32_t consecutive_error_count; // number of consecutive errors
} AcqDecay;

/**
 * @brief create a new AcqDecay struct
 * 
 * @param grace_count 
 * @return const AcqDecay 
 */
const AcqDecay decay_new(uint32_t grace_count);

/**
 * @brief checks if the users should sample this iteration or skip 
 * 
 * @param self 
 * @return int 1 if users should sample, 0 if users should skip sample
 */
int decay_shouldSample(AcqDecay *self);

/**
 * @brief Updates decay based on a sample result 
 */
void decay_update(AcqDecay *self, WTResult result);

#endif // __ACQ_DECAY_H__
