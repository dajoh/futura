#ifndef KERNEL_DRIVERS_VIRTIO_H
#define KERNEL_DRIVERS_VIRTIO_H

#include <stddef.h>
#include <stdbool.h>
#include <virtio/virtio.h>
#include "../pci.h"

#define DRV_VIRTIO_DEVICE_TYPE_BLK 0
#define DRV_VIRTIO_DEVICE_TYPE_GPU 1

struct DrvVirtio;

typedef void (*DrvVirtioInterruptFn)(struct DrvVirtio* drv);

typedef struct DrvVirtio
{
    int DeviceType;

    uint16_t PciVendorId;
    uint16_t PciDeviceId;
    uint32_t PciBus;
    uint32_t PciDevice;
    uint32_t PciFunction;
    uint8_t PciBaseClass;
    uint8_t PciSubClass;
    uint8_t PciIntLine;
    uint8_t PciIntPin;

    uint32_t CommonCfgPhys;
    uint32_t CommonCfgSize;
    uint32_t NotifyCfgPhys;
    uint32_t NotifyCfgSize;
    uint32_t NotifyCfgMult;
    uint32_t IsrCfgPhys;
    uint32_t IsrCfgSize;
    uint32_t DeviceCfgPhys;
    uint32_t DeviceCfgSize;

    virtio_pci_common_cfg* CommonCfg;
    void* NotifyCfg;
    void* IsrCfg;
    void* DeviceCfg;

    uint32_t DevFeatures[2];
    uint32_t DrvFeatures[2];

    vring* Queues;
    uint32_t NumQueues;
    uint8_t Interrupt;
    DrvVirtioInterruptFn InterruptFn;
} DrvVirtio;

bool DrvVirtioCreate(DrvVirtio* drv, const PciDeviceInfo* pciInfo);
bool DrvVirtioStart(DrvVirtio* drv, uint32_t reqFeatures[2], uint32_t optFeatures[2]);
uint8_t DrvVirtioReadISR(DrvVirtio* drv);

bool DrvVirtioRing_AllocDescs(DrvVirtio* drv, size_t queue, vring_desc** descs, size_t count);
void DrvVirtioRing_FreeChain(DrvVirtio* drv, size_t queue, uint16_t id);
void DrvVirtioRing_BatchAdd(DrvVirtio* drv, size_t queue, const vring_desc** descs, size_t count);
void DrvVirtioRing_BatchComplete(DrvVirtio* drv, size_t queue);

static inline uint16_t DrvVirtioRing_DescIndex(DrvVirtio* drv, size_t queue, vring_desc* desc)
{
    return desc - drv->Queues[queue].desc;
}

#endif
