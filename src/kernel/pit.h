#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

#include <stdint.h>

extern uint32_t PitFrequency;
extern uint64_t PitCurrentTick;

void PitInitialize(uint32_t frequency);

static inline uint64_t PitTicksToMs(uint64_t ticks)
{
    return ticks * (1000 / PitFrequency);
}

static inline uint64_t PitMsToTicks(uint64_t ms)
{
    return ms / (1000 / PitFrequency);
}

#endif
