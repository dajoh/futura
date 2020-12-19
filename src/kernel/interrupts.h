#ifndef KERNEL_INTERRUPTS_H
#define KERNEL_INTERRUPTS_H

#include <stdint.h>
#include <stdbool.h>
#include "debug.h"

#define INT00_CPU_DIVIDE_BY_ZERO             0x00
#define INT01_CPU_DEBUG                      0x01
#define INT02_CPU_NMI                        0x02
#define INT03_CPU_BREAKPOINT                 0x03
#define INT04_CPU_OVERFLOW                   0x04
#define INT05_CPU_BOUND_RANGE_EXCEEDED       0x05
#define INT06_CPU_INVALID_OPCODE             0x06
#define INT07_CPU_DEV_NOT_AVAILABLE          0x07
#define INT08_CPU_DOUBLE_FAULT               0x08
#define INT09_CPU_COPROC_SEG_OVERRUN         0x09
#define INT0A_CPU_INVALID_TSS                0x0A
#define INT0B_CPU_SEG_NOT_PRESENT            0x0B
#define INT0C_CPU_STACK_SEG_FAULT            0x0C
#define INT0D_CPU_GP_FAULT                   0x0D
#define INT0E_CPU_PAGE_FAULT                 0x0E
#define INT0F_CPU_RESERVED                   0x0F
#define INT10_CPU_FPU_EXCEPTION              0x10
#define INT11_CPU_ALIGN_CHECK                0x11
#define INT12_CPU_MACHINE_CHECK              0x12
#define INT13_CPU_SIMD_EXCEPTION             0x13
#define INT14_CPU_VIRT_EXCEPTION             0x14
#define INT15_CPU_RESERVED                   0x15
#define INT16_CPU_RESERVED                   0x16
#define INT17_CPU_RESERVED                   0x17
#define INT18_CPU_RESERVED                   0x18
#define INT19_CPU_RESERVED                   0x19
#define INT1A_CPU_RESERVED                   0x1A
#define INT1B_CPU_RESERVED                   0x1B
#define INT1C_CPU_RESERVED                   0x1C
#define INT1D_CPU_RESERVED                   0x1D
#define INT1E_CPU_SECURITY_EXCEPTION         0x1E
#define INT1F_CPU_RESERVED                   0x1F

#define INT20_PIC_IRQ0                       0x20
#define INT21_PIC_IRQ1                       0x21
#define INT22_PIC_IRQ2                       0x22
#define INT23_PIC_IRQ3                       0x23
#define INT24_PIC_IRQ4                       0x24
#define INT25_PIC_IRQ5                       0x25
#define INT26_PIC_IRQ6                       0x26
#define INT27_PIC_IRQ7                       0x27
#define INT28_PIC_IRQ8                       0x28
#define INT29_PIC_IRQ9                       0x29
#define INT2A_PIC_IRQ10                      0x2A
#define INT2B_PIC_IRQ11                      0x2B
#define INT2C_PIC_IRQ12                      0x2C
#define INT2D_PIC_IRQ13                      0x2D
#define INT2E_PIC_IRQ14                      0x2E
#define INT2F_PIC_IRQ15                      0x2F

#define INTXX_APIC_IRQ0                      0x30 // 3 - IRQL_SCHEDULER
#define INTXX_APIC_TIMER                     0x31
#define INTXX_APIC_IRQ2                      0x42 // 4 - IRQL_DEVICE_LO
#define INTXX_APIC_IRQ3                      0x43
#define INTXX_APIC_IRQ4                      0x44
#define INTXX_APIC_IRQ5                      0x45
#define INTXX_APIC_IRQ6                      0x46
#define INTXX_APIC_IRQ7                      0x47
#define INTXX_APIC_IRQ8                      0x48
#define INTXX_APIC_IRQ9                      0x49
#define INTXX_APIC_IRQ10                     0x4A
#define INTXX_APIC_IRQ11                     0x4B
#define INTXX_APIC_IRQ13                     0x4D
#define INTXX_APIC_IRQ14                     0x4E
#define INTXX_APIC_IRQ15                     0x4F
#define INTXX_APIC_IRQ1                      0x51 // 5 - IRQL_DEVICE_HI
#define INTXX_APIC_IRQ12                     0x5C

#ifndef _FUTURA
#define IntEnableIRQs() do { } while(0)
#define IntDisableIRQs() do { } while(0)
#else
#define IntEnableIRQs() do { asm volatile ("sti"); } while(0)
#define IntDisableIRQs() do { asm volatile ("cli"); } while(0)
#endif

#define INT_PIC_MODE_8259 0
#define INT_PIC_MODE_APIC 1

static inline bool IntAreIRQsEnabled()
{
    uint32_t flags;
    asm volatile("pushf\n\tpop %0": "=g"(flags));
    return flags & (1 << 9);
}

static inline uint32_t IntEnterCriticalSection()
{
    uint32_t lock;
    asm volatile("pushf\n\tcli\n\tpop %0": "=r"(lock): :"memory");
    return lock;
}

static inline void IntLeaveCriticalSection(uint32_t lock)
{
    asm volatile("push %0\n\tpopf": :"rm"(lock): "memory", "cc");
}

typedef void (*IntCallbackFn)(void* ctx);

void IntInitialize();
void IntSetPicMode(int picMode);
int IntGetPicMode();
void IntSetAcpiPicMode();
void IntRegisterCallback(uint32_t interrupt, IntCallbackFn fn, void* ctx);
void IntUnregisterCallback(uint32_t interrupt, IntCallbackFn fn);
void IntUnregisterCallback2(uint32_t interrupt, IntCallbackFn fn, void* ctx);
void IntBeginDeferPageFaults();
void IntFinishDeferPageFaults();

static inline uint8_t IntApicIrqToIsr(uint8_t irq)
{
    switch (irq)
    {
    case 0: return INTXX_APIC_IRQ0;
    case 1: return INTXX_APIC_IRQ1;
    case 2: return INTXX_APIC_IRQ2;
    case 3: return INTXX_APIC_IRQ3;
    case 4: return INTXX_APIC_IRQ4;
    case 5: return INTXX_APIC_IRQ5;
    case 6: return INTXX_APIC_IRQ6;
    case 7: return INTXX_APIC_IRQ7;
    case 8: return INTXX_APIC_IRQ8;
    case 9: return INTXX_APIC_IRQ9;
    case 10: return INTXX_APIC_IRQ10;
    case 11: return INTXX_APIC_IRQ11;
    case 12: return INTXX_APIC_IRQ12;
    case 13: return INTXX_APIC_IRQ13;
    case 14: return INTXX_APIC_IRQ14;
    case 15: return INTXX_APIC_IRQ15;
    default:
        DbgUnreachableCase(irq);
        return 0;
    }
}

static inline uint8_t IntIsrToApicIrq(uint8_t isr)
{
    switch (isr)
    {
    case INTXX_APIC_IRQ0: return 0;
    case INTXX_APIC_IRQ1: return 1;
    case INTXX_APIC_IRQ2: return 2;
    case INTXX_APIC_IRQ3: return 3;
    case INTXX_APIC_IRQ4: return 4;
    case INTXX_APIC_IRQ5: return 5;
    case INTXX_APIC_IRQ6: return 6;
    case INTXX_APIC_IRQ7: return 7;
    case INTXX_APIC_IRQ8: return 8;
    case INTXX_APIC_IRQ9: return 9;
    case INTXX_APIC_IRQ10: return 10;
    case INTXX_APIC_IRQ11: return 11;
    case INTXX_APIC_IRQ12: return 12;
    case INTXX_APIC_IRQ13: return 13;
    case INTXX_APIC_IRQ14: return 14;
    case INTXX_APIC_IRQ15: return 15;
    default:
        DbgUnreachableCase(isr);
        return 0;
    }
}

#endif
