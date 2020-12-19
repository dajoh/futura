#ifndef KERNEL_COMPORT_H
#define KERNEL_COMPORT_H

#include <stdint.h>

void ComInitialize();
uint8_t ComRead();
void ComWrite(uint8_t b);

#endif
