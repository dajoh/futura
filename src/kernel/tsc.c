#include "tsc.h"
#include "textmode.h"

uint64_t TscFrequency = 0;

extern void TscCalibrate();

void TscInitialize()
{
    TscCalibrate();
    uint64_t rawHz = TscFrequency;
    uint64_t rawMhz = rawHz / 1000000;
    uint64_t roundMhz = ((rawMhz + 50) / 100) * 100;
    TscFrequency = roundMhz * 1000000;
    TmPrintfDbg("TSC frequency: %llu Hz (%llu MHz raw=%llu, %llu Hz)\n", TscFrequency, roundMhz, rawMhz, rawHz);
}
