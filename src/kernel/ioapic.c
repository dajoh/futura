#include <acpi/acpi.h>
#include "apic.h"
#include "debug.h"
#include "ioapic.h"
#include "memory.h"
#include "textmode.h"
#include "interrupts.h"

// Terminology used in this source file:
// IRQ - Interrupt Request         - ISA standard IRQ0-IRQ15 (timer, keyboard, etc)
// GSI - Global System Interrupt   - IOAPIC input pin number
// ISR - Interrupt Service Routine - CPU interrupt code

#define IOAPIC_MMIO_OFF_REGSEL 0x00 // IOAPIC Register Select - this selects the register to be read/written using WINDOW
#define IOAPIC_MMIO_OFF_WINDOW 0x10 // IOAPIC Register Data Window - this is used for reading/writing to the register selected by REGSEL

#define IOAPIC_REG_ID          0x00 // IOAPIC ID
#define IOAPIC_REG_VER         0x01 // IOAPIC Version
#define IOAPIC_REG_ARB         0x02 // IOAPIC Arbitration ID
#define IOAPIC_REG_TABLE       0x10 // IOAPIC Redirection Table

static volatile uint32_t* IoApicBase = (uint32_t*)0;
static uint32_t IoApicMaxEntries = 0;
static int IoApicIrqToGsiMap[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

static inline uint32_t IoApicRead(uint32_t reg)
{
   IoApicBase[0] = (reg & 0xff);
   return IoApicBase[4];
}
 
static inline void IoApicWrite(uint32_t reg, uint32_t value)
{
   IoApicBase[0] = (reg & 0xff);
   IoApicBase[4] = value;
}

static bool IoApicDeterminePolarity(uint8_t bus, uint32_t intiFlags)
{
    switch (intiFlags & ACPI_MADT_POLARITY_MASK)
    {
    case ACPI_MADT_POLARITY_ACTIVE_HIGH:
        return false;
    case ACPI_MADT_POLARITY_ACTIVE_LOW:
        return true;
    default:
        DbgAssertMsg(bus == 0, "bus type must be ISA");
        return false; // ISA bus is by default active high
    }
}

static bool IoApicDetermineTrigger(uint8_t bus, uint32_t intiFlags)
{
    switch (intiFlags & ACPI_MADT_TRIGGER_MASK)
    {
    case ACPI_MADT_TRIGGER_EDGE:
        return false;
    case ACPI_MADT_TRIGGER_LEVEL:
        return true;
    default:
        DbgAssertMsg(bus == 0, "bus type must be ISA");
        return false; // ISA bus is by default edge triggered
    }
}

static void IoApicWriteEntry(uint8_t gsi, uint8_t cpu, uint8_t isr, uint8_t bus, uint32_t intiFlags, bool mask)
{
    uint32_t reg = IOAPIC_REG_TABLE + (gsi * 2);
    uint32_t pol = IoApicDeterminePolarity(bus, intiFlags);
    uint32_t trg = IoApicDetermineTrigger(bus, intiFlags);
    uint32_t loBits = isr | (0b000 << 8) | (pol << 13) | (trg << 15) | (mask << 16);
    uint32_t hiBits = cpu << 24;
    IoApicWrite(reg + 1, hiBits);
    IoApicWrite(reg + 0, loBits);
}

static void IoApicMapIRQ(ACPI_TABLE_MADT* madt, uint8_t gsi, uint8_t defaultIrq)
{
    uint8_t actualIrq = defaultIrq;
    uint32_t intiFlags = 0;

    ACPI_SUBTABLE_HEADER* entryHdr = (ACPI_SUBTABLE_HEADER*)(madt + 1);
    ACPI_SUBTABLE_HEADER* tableEnd = (ACPI_SUBTABLE_HEADER*)((uint8_t*)madt + madt->Header.Length);
    while (entryHdr < tableEnd)
    {
        ACPI_MADT_INTERRUPT_OVERRIDE* entry = (ACPI_MADT_INTERRUPT_OVERRIDE*)entryHdr;
        if (entryHdr->Type == ACPI_MADT_TYPE_INTERRUPT_OVERRIDE && entry->Bus == 0 /* ISA bus */)
        {
            if (entry->SourceIrq == defaultIrq && entry->GlobalIrq != gsi)
                return; // This ISA IRQ is mapped to a different GSI

            if (entry->GlobalIrq == gsi)
            {
                actualIrq = entry->SourceIrq;
                intiFlags = entry->IntiFlags;
                break;
            }
        }
        entryHdr = (ACPI_SUBTABLE_HEADER*)((uint8_t*)entryHdr + entryHdr->Length);
    }

    uint8_t isr = IntApicIrqToIsr(actualIrq);
    TmPrintfVrb("Mapping IRQ%u to IOAPIC pin %02Xh firing ISR%02Xh\n", actualIrq, gsi, isr);
    IoApicIrqToGsiMap[actualIrq] = gsi;
    IoApicWriteEntry(gsi, /* CPU LAPIC ID */ 0x00, isr, /* ISA bus */ 0, intiFlags, false);
}

bool IoApicInitialize()
{
    ACPI_TABLE_MADT* madt = NULL;
    DbgAssert(ACPI_SUCCESS(AcpiGetTable(ACPI_SIG_MADT, 0, (ACPI_TABLE_HEADER**)&madt)));

    ACPI_SUBTABLE_HEADER* entryHdr = (ACPI_SUBTABLE_HEADER*)(madt + 1);
    ACPI_SUBTABLE_HEADER* tableEnd = (ACPI_SUBTABLE_HEADER*)((uint8_t*)madt + madt->Header.Length);
    while (entryHdr < tableEnd)
    {
        if (entryHdr->Type == ACPI_MADT_TYPE_IO_APIC)
        {
            ACPI_MADT_IO_APIC* entry = (ACPI_MADT_IO_APIC*)entryHdr;
            DbgAssertMsg(IoApicBase == NULL, "only 1 IOAPIC supported");
            DbgAssertMsg(entry->GlobalIrqBase == 0, "IOAPIC must have global IRQ base 0, got %u", entry->GlobalIrqBase);

            IoApicBase = VirtAlloc(entry->Address, 1, VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_HARDWARE, "IOAPIC");
            PhysMark(entry->Address, 1, PHYS_REGION_TYPE_CPU_IO_APIC, "IOAPIC");

            IoApicMaxEntries = ((IoApicRead(IOAPIC_REG_VER) >> 16) & 0xFF) + 1;
            TmPrintfDbg("Found IOAPIC (addr=%p, pins=%u)\n", IoApicBase, IoApicMaxEntries);
        }
        entryHdr = (ACPI_SUBTABLE_HEADER*)((uint8_t*)entryHdr + entryHdr->Length);
    }

    if (IoApicBase == NULL || IoApicMaxEntries == 0)
    {
        TmPrintfErr("No IOAPIC found!\n");
        return false;
    }

    // Systems that support both APIC and dual 8259 interrupt models must map global system interrupts
    // 0-15 to the 8259 IRQs 0-15, except where Interrupt Source Overrides are provided (see
    // Section 5.2.12.5, "Interrupt Source Override Structure" below). This means that I/O APIC interrupt
    // inputs 0-15 must be mapped to global system interrupts 0-15 and have identical sources as the 8259
    // IRQs 0-15 unless overrides are used.
    uint32_t irqLock = IntEnterCriticalSection();
    {
        // TODO: This might not be the correct flow, instead build IRQ to GSI map first?
        IoApicMapIRQ(madt, 0, 0);
        IoApicMapIRQ(madt, 1, 1);
        IoApicMapIRQ(madt, 2, 2);
        IoApicMapIRQ(madt, 3, 3);
        IoApicMapIRQ(madt, 4, 4);
        IoApicMapIRQ(madt, 5, 5);
        IoApicMapIRQ(madt, 6, 6);
        IoApicMapIRQ(madt, 7, 7);
        IoApicMapIRQ(madt, 8, 8);
        IoApicMapIRQ(madt, 9, 9);
        IoApicMapIRQ(madt, 10, 10);
        IoApicMapIRQ(madt, 11, 11);
        IoApicMapIRQ(madt, 12, 12);
        IoApicMapIRQ(madt, 13, 13);
        IoApicMapIRQ(madt, 14, 14);
        IoApicMapIRQ(madt, 15, 15);
        IoApicMaskIRQ(INTXX_APIC_IRQ0);
    }
    IntLeaveCriticalSection(irqLock);

    return true;
}

void IoApicMaskIRQ(uint8_t isr)
{
    uint32_t irq = IntIsrToApicIrq(isr);
    int32_t gsi = IoApicIrqToGsiMap[irq];
    if (gsi == -1)
        return;
    uint32_t reg = IOAPIC_REG_TABLE + (gsi * 2);
    uint32_t hiBits = IoApicRead(reg + 1);
    uint32_t loBits = IoApicRead(reg + 0);
    loBits |= 1 << 16;
    IoApicWrite(reg + 1, hiBits);
    IoApicWrite(reg + 0, loBits);
    TmPrintfVrb("Masked IRQ%u with IOAPIC (ISR%02Xh, pin=%02Xh)\n", irq, isr, gsi);
}

void IoApicUnmaskIRQ(uint8_t isr)
{
    uint32_t irq = IntIsrToApicIrq(isr);
    int32_t gsi = IoApicIrqToGsiMap[irq];
    if (gsi == -1)
        return;
    uint32_t reg = IOAPIC_REG_TABLE + (gsi * 2);
    uint32_t hiBits = IoApicRead(reg + 1);
    uint32_t loBits = IoApicRead(reg + 0);
    loBits &= ~(1 << 16);
    IoApicWrite(reg + 1, hiBits);
    IoApicWrite(reg + 0, loBits);
    TmPrintfVrb("Unmasked IRQ%u with IOAPIC (ISR%02Xh, pin=%02Xh)\n", irq, isr, gsi);
}
