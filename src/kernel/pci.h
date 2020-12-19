#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

#include "lowlevel.h"

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_VALUE_PORT   0xCFC

#define PCI_OFFSET_VENDOR_ID     0x00
#define PCI_OFFSET_DEVICE_ID     0x02
#define PCI_OFFSET_COMMAND       0x04
#define PCI_OFFSET_STATUS        0x06
#define PCI_OFFSET_PROG_IF       0x09
#define PCI_OFFSET_HEADER_TYPE   0x0E
#define PCI_OFFSET_BASE_CLASS    0x0B
#define PCI_OFFSET_SUB_CLASS     0x0A
#define PCI_OFFSET_SECONDARY_BUS 0x19
#define PCI_OFFSET_BAR0          0x10
#define PCI_OFFSET_BAR1          0x14
#define PCI_OFFSET_BAR2          0x18
#define PCI_OFFSET_BAR3          0x1C
#define PCI_OFFSET_BAR4          0x20
#define PCI_OFFSET_BAR5          0x24
#define PCI_OFFSET_CAP_PTR       0x34
#define PCI_OFFSET_INT_LINE      0x3C
#define PCI_OFFSET_INT_PIN       0x3D

#define PCI_INT_PIN_NONE         0x00
#define PCI_INT_PIN_INTA         0x01
#define PCI_INT_PIN_INTB         0x02
#define PCI_INT_PIN_INTC         0x03
#define PCI_INT_PIN_INTD         0x04

typedef struct PciDeviceInfo_s
{
    uint32_t Bus;
    uint32_t Device;
    uint32_t Function;
    uint16_t VendorId;
    uint16_t DeviceId;
    uint8_t BaseClass;
    uint8_t SubClass;
} PciDeviceInfo;

typedef void (*PciDiscoverCallbackFn)(const PciDeviceInfo* info, void* ctx);

void PciInitialize();
uint8_t PciLookupIntPinISR(uint32_t bus, uint32_t device, uint8_t pciIntPin);
void PciRegisterDiscoverCallback(PciDiscoverCallbackFn fn, void* ctx);
void PciUnregisterDiscoverCallback(PciDiscoverCallbackFn fn);
void PciDiscoverDevices();

static inline uint8_t PciReadByte(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset)
{
    outl(PCI_ADDRESS_PORT, 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC));
    return inb(PCI_VALUE_PORT + (offset & 3));
}

static inline uint16_t PciReadWord(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset)
{
    outl(PCI_ADDRESS_PORT, 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC));
    return inw(PCI_VALUE_PORT + (offset & 2));
}

static inline uint32_t PciReadLong(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset)
{
    outl(PCI_ADDRESS_PORT, 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC));
    return inl(PCI_VALUE_PORT);
}

static inline void PciWriteByte(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint8_t value)
{
    outl(PCI_ADDRESS_PORT, 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC));
    outb(PCI_VALUE_PORT + (offset & 3), value);
}

static inline void PciWriteWord(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint16_t value)
{
    outl(PCI_ADDRESS_PORT, 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC));
    outw(PCI_VALUE_PORT + (offset & 2), value);
}

static inline void PciWriteLong(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint32_t value)
{
    outl(PCI_ADDRESS_PORT, 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC));
    outl(PCI_VALUE_PORT, value);
}

#endif
