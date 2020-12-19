#include "pic.h"
#include "apic.h"
#include "debug.h"
#include "memory.h"
#include "lowlevel.h"
#include "textmode.h"
#include "scheduler.h"
#include "interrupts.h"

#define CPUID_GETFEATURES 1
#define CPUID_FEAT_EDX_APIC (1 << 9)
#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Indicates if the processor is the bootstrap processor (BSP)
#define IA32_APIC_BASE_MSR_ENABLE 0x800 // Enables or disables the local APIC

#define APIC_REG_ID         0x020 // Local APIC ID
#define APIC_REG_VER        0x030 // Local APIC Version
#define APIC_REG_SIV        0x0F0 // Spurious Interrupt Vector
#define APIC_REG_TPR        0x080 // Task Priority Register
#define APIC_REG_EOI        0x0B0 // End of interrupt Register
#define APIC_REG_LVT_TIMER  0x320 // Timer LVT Register
#define APIC_REG_TIMER_INIT 0x380 // Timer initial count Register
#define APIC_REG_TIMER_CURR 0x390 // Timer current count Register
#define APIC_REG_TIMER_DIV  0x3E0 // Timer divide Register
#define APIC_TIMER_LVT_PERIODIC (1 << 17)

uint32_t ApicFrequency = 0;
static volatile uint8_t* ApicBase = NULL;

static bool ApicCpuHasLocalApic()
{
    uint32_t eax, edx;
    cpuid(CPUID_GETFEATURES, &eax, &edx);
    return (edx & CPUID_FEAT_EDX_APIC) != 0;
}

static uintptr_t ApicGetApicBase()
{
   uint64_t base = rdmsr(IA32_APIC_BASE_MSR);
   return (base & 0xfffff000);
}

static bool ApicIsLocalApicEnabled()
{
    return (rdmsr(IA32_APIC_BASE_MSR) & IA32_APIC_BASE_MSR_ENABLE) != 0;
}

static uint32_t ApicReadRegister(size_t offset)
{
    DbgAssert((offset % 16) == 0); // reads/writes must be 128-bit aligned
    return *(volatile uint32_t*)(ApicBase + offset);
}

static void ApicWriteRegister(size_t offset, uint32_t value)
{
    DbgAssert((offset % 16) == 0); // reads/writes must be 128-bit aligned
    *(volatile uint32_t*)(ApicBase + offset) = value;
}

bool ApicInitialize()
{
    if (!ApicCpuHasLocalApic())
    {
        TmPrintfErr("No local APIC found!\n");
        return false;
    }

    // Get info about local APIC
    TmPrintfVrb("Local APIC enabled: %s\n", ApicIsLocalApicEnabled() ? "yes" : "no");
    TmPrintfVrb("Current local APIC base: %p (raw 0x%llX)\n", ApicGetApicBase(), rdmsr(IA32_APIC_BASE_MSR));

    // Map local APIC registers into virtual memory
    ApicBase = VirtAlloc(ApicGetApicBase(), 1, VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_HARDWARE, "APIC");
    PhysMark(ApicGetApicBase(), 1, PHYS_REGION_TYPE_CPU_LOCAL_APIC, "APIC");

    // Dump info about local APIC
    TmPrintfVrb("Local APIC ID: %u (0x%x)\n", ApicReadRegister(APIC_REG_ID), ApicReadRegister(APIC_REG_ID));
    TmPrintfVrb("Local APIC version: %u (0x%x)\n", ApicReadRegister(APIC_REG_VER), ApicReadRegister(APIC_REG_VER));

    // Disable the PIC
    PicDisable();

    // Enable the local APIC & set it up to a known good state
    wrmsr(IA32_APIC_BASE_MSR, rdmsr(IA32_APIC_BASE_MSR) | IA32_APIC_BASE_MSR_ENABLE);
    ApicWriteRegister(APIC_REG_SIV, 0x1FF);
    ApicWriteRegister(APIC_REG_TPR, 0);
    ApicWriteRegister(APIC_REG_TIMER_DIV, 3); // 3 = 0b11 = divide by 16

    // Notify interrupt handler that we've switched PIC
    IntSetPicMode(INT_PIC_MODE_APIC);
    TmPrintfDbg("Switched from 8259 PICs to local APIC\n");

    // Enable local APIC timer
    ApicWriteRegister(APIC_REG_LVT_TIMER, INTXX_APIC_TIMER);

    // Calibrate timer
    ApicWriteRegister(APIC_REG_TIMER_INIT, UINT32_MAX);
    SchStall(500 * 1000);
    uint32_t rawHz = (UINT32_MAX - ApicReadRegister(APIC_REG_TIMER_CURR)) * 2;
    uint32_t rawKhz = rawHz / 1000;
    uint32_t roundKhz = ((rawKhz + 50) / 100) * 100;
    ApicFrequency = roundKhz * 1000;
    TmPrintfDbg("Local APIC timer frequency: %u Hz (%u KHz raw=%u, %u Hz)\n", ApicFrequency, roundKhz, rawKhz, rawHz);

    // Enable 100Hz timer
    ApicWriteRegister(APIC_REG_LVT_TIMER, INTXX_APIC_TIMER | APIC_TIMER_LVT_PERIODIC);
    ApicWriteRegister(APIC_REG_TIMER_INIT, (ApicFrequency / 1000) * 10);
    return true;
}

void ApicSetTPR(uint8_t tpr)
{
    ApicWriteRegister(APIC_REG_TPR, tpr);
}

uint8_t ApicGetTPR()
{
    return (uint8_t)ApicReadRegister(APIC_REG_TPR);
}

void ApicSendEOI(uint8_t interrupt)
{
    ApicWriteRegister(APIC_REG_EOI, 0);
}
