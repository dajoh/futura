#include "pit.h"
#include "lowlevel.h"

uint32_t PitFrequency = 0;
uint64_t PitCurrentTick = 0;

void PitInitialize(uint32_t frequency)
{
    PitFrequency = frequency;
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}
