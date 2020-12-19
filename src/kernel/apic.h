#ifndef KERNEL_APIC_H
#define KERNEL_APIC_H

#include <stdint.h>
#include <stdbool.h>

extern uint32_t ApicFrequency;

bool ApicInitialize();
void ApicSetTPR(uint8_t tpr);
uint8_t ApicGetTPR();
void ApicSendEOI(uint8_t interrupt);

#endif
