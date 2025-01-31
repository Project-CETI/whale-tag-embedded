#include "decay.h"

const AcqDecay decay_new(uint32_t grace_count) {
    return (AcqDecay){
        .grace_count = grace_count,
        .decay_multiplier = 1,
    };
}

int decay_shouldSample(AcqDecay *self) {
    self->skip_count++;
    if (self->skip_count < self->decay_multiplier) {
        return 0;
    } else {
        self->skip_count = 0;
        return 1;
    }
}

void decay_update(AcqDecay *self, WTResult result) {
    if (result == WT_OK) {
        self->decay_multiplier = 1;
        self->consecutive_error_count = 0;
    } else {
        self->consecutive_error_count++;
        if (!(self->consecutive_error_count < self->grace_count)) {
            self->decay_multiplier <<= 1; // double decay time
        }
    }
}
