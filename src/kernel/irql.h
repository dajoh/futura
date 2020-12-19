#ifndef KERNEL_IRQL_H
#define KERNEL_IRQL_H

typedef int irql_t;

extern irql_t IrqlCurrent;

#define IRQL_STANDARD  0x00 // Masks nothing
#define IRQL_SCHEDULER 0x03 // Masks IRQ0 timer
#define IRQL_DEVICE_LO 0x04 // Masks IRQ0-IRQ15 except keyboard and mouse
#define IRQL_DEVICE_HI 0x05 // Masks IRQ0-IRQ15
#define IRQL_EXCLUSIVE 0x0F // Masks IRQ0-IRQ15, disables IF flag

static inline irql_t IrqlGetCurrent()
{
    return IrqlCurrent;
}

irql_t IrqlSetCurrent(irql_t level);

#endif
