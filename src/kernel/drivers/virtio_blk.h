#ifndef KERNEL_DRIVERS_VIRTIO_BLK_H
#define KERNEL_DRIVERS_VIRTIO_BLK_H

#include "virtio.h"

typedef struct DrvVirtioBlk
{
    DrvVirtio Drv;
} DrvVirtioBlk;

typedef struct AsyncCall
{
    bool Success;
    size_t Transferred;
    SchEvent* Event;
    void* UserData;
} AsyncCall;

typedef void (*AsyncCallbackFn)(AsyncCall* call);

DrvVirtioBlk* DrvVirtioBlk_Create(const PciDeviceInfo* pciInfo);
bool DrvVirtioBlk_Start(DrvVirtioBlk* drv);

size_t DrvVirtioBlk_Read(DrvVirtioBlk* drv, uint64_t sector, void* buf, size_t len);
void DrvVirtioBlk_ReadAsync(DrvVirtioBlk* drv, uint64_t sector, void* buf, size_t len, AsyncCall* call, AsyncCallbackFn* fn);

size_t DrvVirtioBlk_Write(DrvVirtioBlk* drv, uint64_t sector, const void* buf, size_t len);

#endif
