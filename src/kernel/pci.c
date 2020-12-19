#include <acpi/acpi.h>
#include "pci.h"
#include "list.h"
#include "debug.h"
#include "memory.h"
#include "textmode.h"
#include "interrupts.h"

typedef struct PciDiscoverCallbackRecord_s
{
    ListEntry List;
    PciDiscoverCallbackFn Function;
    void* Context;
} PciDiscoverCallbackRecord;

static ListHead PciDiscoverCallbackList;
static uint8_t* PciAcpiPrt = NULL;
static size_t PciAcpiPrtSize = 0;

static void PciCheckAllBuses();
static void PciCheckBus(uint32_t bus);
static void PciCheckDevice(uint32_t bus, uint32_t device);
static void PciCheckFunction(uint32_t bus, uint32_t device, uint32_t function);

void PciInitialize()
{
    ListInitialize(&PciDiscoverCallbackList);

    ACPI_HANDLE pciBus = NULL;
    DbgAssert(ACPI_SUCCESS(AcpiGetHandle(NULL, "\\_SB.PCI0", &pciBus)));

    ACPI_BUFFER prtBuf = {ACPI_ALLOCATE_BUFFER, NULL};
    DbgAssert(ACPI_SUCCESS(AcpiGetIrqRoutingTable(pciBus, &prtBuf)));

    PciAcpiPrt = prtBuf.Pointer;
    PciAcpiPrtSize = prtBuf.Length;
}

uint8_t PciLookupIntPinISR(uint32_t bus, uint32_t device, uint8_t pciIntPin)
{
    DbgAssert(bus == 0 /* PCI0 */);
    DbgAssert(pciIntPin >= PCI_INT_PIN_INTA && pciIntPin <= PCI_INT_PIN_INTD);

    ACPI_PCI_ROUTING_TABLE* entry = (ACPI_PCI_ROUTING_TABLE*)PciAcpiPrt;
    ACPI_PCI_ROUTING_TABLE* bufEnd = (ACPI_PCI_ROUTING_TABLE*)(PciAcpiPrt + PciAcpiPrtSize);
    while (entry < bufEnd)
    {
        if (entry->Pin + 1 == pciIntPin && entry->Address >> 16 == device)
        {
            TmPrintf("PRT Entry PCI Bus: %u\n", bus);
            TmPrintf("PRT Entry PCI Dev: %u\n", device);
            TmPrintf("PRT Entry PCI Pin: INT%c#\n", 'A' + entry->Pin);
            TmPrintf("PRT Entry SrcName: %s\n", entry->Source);
            TmPrintf("PRT Entry SrcIdx:  %u\n", entry->SourceIndex);
            DbgAssertMsg(entry->Source[0] != '\0', "currently only indirect PRT entries are supported");

            ACPI_HANDLE src = NULL;
            DbgAssert(ACPI_SUCCESS(AcpiGetHandle(NULL, entry->Source, &src)));

            ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, NULL};
            DbgAssert(ACPI_SUCCESS(AcpiGetCurrentResources(src, &buf)));

            ACPI_RESOURCE* res = (ACPI_RESOURCE*)buf.Pointer;
            ACPI_RESOURCE* resEnd = (ACPI_RESOURCE*)((uint8_t*)buf.Pointer + buf.Length);
            while (res < resEnd)
            {
                if (res->Type == ACPI_RESOURCE_TYPE_IRQ)
                {
                    ACPI_RESOURCE_IRQ* irq = &res->Data.Irq;
                    TmPrintf("Intrpts:"); for (size_t i = 0; i < irq->InterruptCount; i++) TmPrintf(" %u,", irq->Interrupts[i]); TmPrintf("\b \n");
                    if (irq->InterruptCount != 0)
                    {
                        DbgAssert(irq->InterruptCount == 1);
                        ACPI_FREE(buf.Pointer);
                        return IntApicIrqToIsr(irq->Interrupts[0]); // TODO: are these ISA IRQs or IOAPIC GSIs?
                    }
                }
                else if (res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ)
                {
                    ACPI_RESOURCE_EXTENDED_IRQ* irq = &res->Data.ExtendedIrq;
                    TmPrintf("IntrptX:"); for (size_t i = 0; i < irq->InterruptCount; i++) TmPrintf(" %u,", irq->Interrupts[i]); TmPrintf("\b \n");
                    if (irq->InterruptCount != 0)
                    {
                        DbgAssert(irq->InterruptCount == 1);
                        ACPI_FREE(buf.Pointer);
                        return IntApicIrqToIsr(irq->Interrupts[0]); // TODO: are these ISA IRQs or IOAPIC GSIs?
                    }
                }
                res = (ACPI_RESOURCE*)((uint8_t*)res + res->Length);
            }
            ACPI_FREE(buf.Pointer);
        }

        entry = (ACPI_PCI_ROUTING_TABLE*)((uint8_t*)entry + entry->Length);
    }

    DbgPanic("couldn't resolve PCI INTPIN ISR");
    return 0x00;
}

void PciRegisterDiscoverCallback(PciDiscoverCallbackFn fn, void* ctx)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        PciDiscoverCallbackRecord* record = kalloc(sizeof(PciDiscoverCallbackRecord));
        record->Function = fn;
        record->Context = ctx;
        ListPushBack(&PciDiscoverCallbackList, &record->List);
    }
    IntLeaveCriticalSection(irqLock);
}

void PciUnregisterDiscoverCallback(PciDiscoverCallbackFn fn)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        ListEntry* entry = PciDiscoverCallbackList.Next;
        while (entry != &PciDiscoverCallbackList)
        {
            PciDiscoverCallbackRecord* record = CONTAINING_RECORD(entry, PciDiscoverCallbackRecord, List);
            if (record->Function == fn)
            {
                ListRemove(&record->List);
                kfree(record);
                break;
            }
            entry = record->List.Next;
        }
    }
    IntLeaveCriticalSection(irqLock);
}

void PciDiscoverDevices()
{
    PciCheckAllBuses();
}

static void PciCheckAllBuses()
{
    uint8_t headerType = PciReadByte(0, 0, 0, PCI_OFFSET_HEADER_TYPE);
    if ((headerType & 0x80) == 0)
        return PciCheckBus(0);
    
    for (uint32_t bus = 0; bus < 8; bus++)
    {
        if (PciReadWord(0, 0, bus, PCI_OFFSET_VENDOR_ID) != 0xFFFF)
            break;
        PciCheckBus(bus);
    }
}

static void PciCheckBus(uint32_t bus)
{
    for (uint32_t device = 0; device < 32; device++)
        PciCheckDevice(bus, device);
}

static void PciCheckDevice(uint32_t bus, uint32_t device)
{
    uint16_t vendorId = PciReadWord(bus, device, 0, PCI_OFFSET_VENDOR_ID);
    if (vendorId == 0xFFFF)
        return;
    PciCheckFunction(bus, device, 0);

    uint8_t headerType = PciReadByte(bus, device, 0, PCI_OFFSET_HEADER_TYPE);
    if (headerType & 0x80)
        for (uint32_t function = 1; function < 8; function++)
            if (PciReadWord(bus, device, function, PCI_OFFSET_VENDOR_ID) != 0xFFFF)
                PciCheckFunction(bus, device, function);
}

static void PciCheckFunction(uint32_t bus, uint32_t device, uint32_t function)
{
    uint8_t baseClass = PciReadByte(bus, device, function, PCI_OFFSET_BASE_CLASS);
    uint8_t subClass = PciReadByte(bus, device, function, PCI_OFFSET_SUB_CLASS);
    if (baseClass == 0x06 && subClass == 0x04)
    {
        uint8_t secondaryBus = PciReadByte(bus, device, function, PCI_OFFSET_SECONDARY_BUS);
        PciCheckBus(secondaryBus);
    }

    uint16_t vendorId = PciReadWord(bus, device, function, PCI_OFFSET_VENDOR_ID);
    uint16_t deviceId = PciReadWord(bus, device, function, PCI_OFFSET_DEVICE_ID);
    uint8_t headerType = PciReadByte(bus, device, function, PCI_OFFSET_HEADER_TYPE);
    TmPrintfDbg("PCI device: [%04x:%04x] Bus=%u Dev=%u Fn=%u HDR=%02X CLASS=%02X SUB=%02X\n", deviceId, vendorId, bus, device, function, headerType, baseClass, subClass);

    uint32_t irqLock = IntEnterCriticalSection();
    {
        PciDeviceInfo info;
        info.Bus = bus;
        info.Device = device;
        info.Function = function;
        info.VendorId = vendorId;
        info.DeviceId = deviceId;
        info.BaseClass = baseClass;
        info.SubClass = subClass;

        ListEntry* entry = PciDiscoverCallbackList.Next;
        while (entry != &PciDiscoverCallbackList)
        {
            PciDiscoverCallbackRecord* record = CONTAINING_RECORD(entry, PciDiscoverCallbackRecord, List);
            record->Function(&info, record->Context);
            entry = record->List.Next;
        }
    }
    IntLeaveCriticalSection(irqLock);
}
