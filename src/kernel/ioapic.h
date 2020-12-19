#ifndef KERNEL_IOAPIC_H
#define KERNEL_IOAPIC_H

#include <stdint.h>
#include <stdbool.h>

bool IoApicInitialize();
void IoApicMaskIRQ(uint8_t isr);
void IoApicUnmaskIRQ(uint8_t isr);

#endif
