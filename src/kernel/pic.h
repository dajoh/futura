#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

#include <stdint.h>

void PicInitialize();
void PicDisable();
void PicSendEOI(uint8_t irq);

#endif
