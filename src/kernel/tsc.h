#ifndef KERNEL_TSC_H
#define KERNEL_TSC_H

#include "lowlevel.h"

extern uint64_t TscFrequency;

void TscInitialize();

#endif
