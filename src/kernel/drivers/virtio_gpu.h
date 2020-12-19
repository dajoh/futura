#ifndef KERNEL_DRIVERS_VIRTIO_GPU_H
#define KERNEL_DRIVERS_VIRTIO_GPU_H

#include "virtio.h"

typedef struct DrvVirtioGpu
{
    DrvVirtio Drv;
    virtio_gpu_config* Cfg;
    uint64_t Processed;
    uint64_t Outstanding;
    int Width;
    int Height;
} DrvVirtioGpu;

DrvVirtioGpu* DrvVirtioGpu_Create(const PciDeviceInfo* pciInfo);
bool DrvVirtioGpu_Start(DrvVirtioGpu* gpu);
void DrvVirtioGpu_Present(DrvVirtioGpu* gpu);
void DrvVirtioGpu_PresentActive();

#endif
