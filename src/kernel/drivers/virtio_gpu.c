#include "../tsc.h"
#include "../debug.h"
#include "../memory.h"
#include "../textmode.h"
#include "../scheduler.h"
#include "../interrupts.h"
#include "virtio_gpu.h"

#include "../fbcon.h"
#include "../lodepng.h"
#include "../embedded.h"

#pragma pack(push, 1)
typedef struct virtio_gpu_resource_attach_backing_1
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    virtio_gpu_mem_entry entries[1];
} virtio_gpu_resource_attach_backing_1;
#pragma pack(pop)

DrvVirtioGpu* DrvVirtioGpu_Create(const PciDeviceInfo* pciInfo)
{
    if (pciInfo->VendorId != 0x1af4 || pciInfo->DeviceId != 0x1050)
        return false;

    DrvVirtioGpu* gpu = kcalloc(sizeof(DrvVirtioGpu));
    if (!DrvVirtioCreate(&gpu->Drv, pciInfo))
    {
        kfree(gpu);
        return NULL;
    }

    return gpu;
}

static void DrvVirtioGpu_ProcessOne(DrvVirtioGpu* gpu, size_t queue, vring_used_elem* elem)
{
    vring* q = &gpu->Drv.Queues[queue];
    vring_desc* head = &q->desc[elem->id];
    DrvVirtioRing_FreeChain(&gpu->Drv, queue, elem->id);
    gpu->Outstanding--;
}

static void DrvVirtioGpu_Process(DrvVirtioGpu* gpu, size_t queue)
{
    vring* q = &gpu->Drv.Queues[queue];
    while (true)
    {
        uint32_t irqLock = IntEnterCriticalSection();
        if (q->last_seen_used == q->used->idx)
        {
            IntLeaveCriticalSection(irqLock);
            break;
        }
        vring_used_elem* elem = &q->used->ring[q->last_seen_used % q->num];
        q->last_seen_used++;
        IntLeaveCriticalSection(irqLock);

        DrvVirtioGpu_ProcessOne(gpu, queue, elem);
    }
}

bool DrvVirtioGpu_GetDispInfo(DrvVirtioGpu* gpu, virtio_gpu_resp_display_info* resp)
{
    virtio_gpu_ctrl_hdr req;
    memset(&req, 0, sizeof(req));
    req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    vring_desc* descs[2];
    while (!DrvVirtioRing_AllocDescs(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 2))
        SchYield();

    descs[0]->addr = VirtToPhys(&req);
    descs[0]->len = sizeof(virtio_gpu_ctrl_hdr);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs[1]);

    descs[1]->addr = VirtToPhys(resp);
    descs[1]->len = sizeof(virtio_gpu_resp_display_info);
    descs[1]->flags = VRING_DESC_F_WRITE;
    descs[1]->next = UINT16_MAX;

    gpu->Outstanding++;
    DrvVirtioRing_BatchAdd(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 1);
    DrvVirtioRing_BatchComplete(&gpu->Drv, VIRTIO_GPU_Q_CONTROL);
    while (gpu->Outstanding != 0)
        DrvVirtioGpu_Process(gpu, VIRTIO_GPU_Q_CONTROL);
    
    return resp->hdr.type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO;
}

bool DrvVirtioGpu_CreateResource2D(DrvVirtioGpu* gpu, uint32_t resId, uint32_t fmt, uint32_t w, uint32_t h)
{
    virtio_gpu_resource_create_2d req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    req.resource_id = resId;
    req.format = fmt;
    req.width = w;
    req.height = h;

    vring_desc* descs[2];
    while (!DrvVirtioRing_AllocDescs(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 2))
        SchYield();

    descs[0]->addr = VirtToPhys(&req);
    descs[0]->len = sizeof(virtio_gpu_resource_create_2d);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs[1]);

    virtio_gpu_ctrl_hdr resp;
    descs[1]->addr = VirtToPhys(&resp);
    descs[1]->len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1]->flags = VRING_DESC_F_WRITE;
    descs[1]->next = UINT16_MAX;

    gpu->Outstanding++;
    DrvVirtioRing_BatchAdd(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 1);
    DrvVirtioRing_BatchComplete(&gpu->Drv, VIRTIO_GPU_Q_CONTROL);
    while (gpu->Outstanding != 0)
        DrvVirtioGpu_Process(gpu, VIRTIO_GPU_Q_CONTROL);

    return resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool DrvVirtioGpu_AttachResourceBacking(DrvVirtioGpu* gpu, uint32_t resId, uint64_t addr, size_t length)
{
    virtio_gpu_resource_attach_backing_1 req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    req.resource_id = resId;
    req.nr_entries = 1;
    req.entries[0].addr = addr;
    req.entries[0].length = length;

    vring_desc* descs[2];
    while (!DrvVirtioRing_AllocDescs(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 2))
        SchYield();

    descs[0]->addr = VirtToPhys(&req);
    descs[0]->len = sizeof(virtio_gpu_resource_attach_backing_1);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs[1]);

    virtio_gpu_ctrl_hdr resp;
    descs[1]->addr = VirtToPhys(&resp);
    descs[1]->len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1]->flags = VRING_DESC_F_WRITE;
    descs[1]->next = UINT16_MAX;

    gpu->Outstanding++;
    DrvVirtioRing_BatchAdd(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 1);
    DrvVirtioRing_BatchComplete(&gpu->Drv, VIRTIO_GPU_Q_CONTROL);
    while (gpu->Outstanding != 0)
        DrvVirtioGpu_Process(gpu, VIRTIO_GPU_Q_CONTROL);

    return resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool DrvVirtioGpu_SetScanout(DrvVirtioGpu* gpu, uint32_t scanoutId, uint32_t resId, int x, int y, int w, int h)
{
    virtio_gpu_set_scanout req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.r.x = x;
    req.r.y = y;
    req.r.width = w;
    req.r.height = h;
    req.resource_id = resId;
    req.scanout_id = scanoutId;

    vring_desc* descs[2];
    while (!DrvVirtioRing_AllocDescs(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 2))
        SchYield();

    descs[0]->addr = VirtToPhys(&req);
    descs[0]->len = sizeof(virtio_gpu_set_scanout);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs[1]);

    virtio_gpu_ctrl_hdr resp;
    descs[1]->addr = VirtToPhys(&resp);
    descs[1]->len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1]->flags = VRING_DESC_F_WRITE;
    descs[1]->next = UINT16_MAX;

    gpu->Outstanding++;
    DrvVirtioRing_BatchAdd(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 1);
    DrvVirtioRing_BatchComplete(&gpu->Drv, VIRTIO_GPU_Q_CONTROL);
    while (gpu->Outstanding != 0)
        DrvVirtioGpu_Process(gpu, VIRTIO_GPU_Q_CONTROL);

    return resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool DrvVirtioGpu_TransferToHost2D(DrvVirtioGpu* gpu, uint32_t resId, int x, int y, int w, int h)
{
    virtio_gpu_transfer_to_host_2d req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    req.r.x = x;
    req.r.y = y;
    req.r.width = w;
    req.r.height = h;
    req.offset = 0;
    req.resource_id = resId;

    vring_desc* descs[2];
    while (!DrvVirtioRing_AllocDescs(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 2))
        SchYield();

    descs[0]->addr = VirtToPhys(&req);
    descs[0]->len = sizeof(virtio_gpu_transfer_to_host_2d);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs[1]);

    virtio_gpu_ctrl_hdr resp;
    descs[1]->addr = VirtToPhys(&resp);
    descs[1]->len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1]->flags = VRING_DESC_F_WRITE;
    descs[1]->next = UINT16_MAX;

    gpu->Outstanding++;
    DrvVirtioRing_BatchAdd(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 1);
    DrvVirtioRing_BatchComplete(&gpu->Drv, VIRTIO_GPU_Q_CONTROL);
    while (gpu->Outstanding != 0)
        DrvVirtioGpu_Process(gpu, VIRTIO_GPU_Q_CONTROL);

    return resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool DrvVirtioGpu_ResourceFlush(DrvVirtioGpu* gpu, uint32_t resId, int x, int y, int w, int h)
{
    virtio_gpu_resource_flush req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    req.r.x = x;
    req.r.y = y;
    req.r.width = w;
    req.r.height = h;
    req.resource_id = resId;

    vring_desc* descs[2];
    while (!DrvVirtioRing_AllocDescs(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 2))
        SchYield();

    descs[0]->addr = VirtToPhys(&req);
    descs[0]->len = sizeof(virtio_gpu_resource_flush);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs[1]);

    virtio_gpu_ctrl_hdr resp;
    descs[1]->addr = VirtToPhys(&resp);
    descs[1]->len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1]->flags = VRING_DESC_F_WRITE;
    descs[1]->next = UINT16_MAX;

    gpu->Outstanding++;
    DrvVirtioRing_BatchAdd(&gpu->Drv, VIRTIO_GPU_Q_CONTROL, descs, 1);
    DrvVirtioRing_BatchComplete(&gpu->Drv, VIRTIO_GPU_Q_CONTROL);
    while (gpu->Outstanding != 0)
        DrvVirtioGpu_Process(gpu, VIRTIO_GPU_Q_CONTROL);

    return resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool DrvVirtioGpu_Start(DrvVirtioGpu* gpu)
{
    uint32_t reqFeatures[2] = {0, (1 << (VIRTIO_F_VERSION_1 - 32))};
    uint32_t optFeatures[2] = {0, 0};
    if (!DrvVirtioStart(&gpu->Drv, reqFeatures, optFeatures))
        return false;
    gpu->Cfg = (virtio_gpu_config*)gpu->Drv.DeviceCfg;

    // Get display info
    virtio_gpu_resp_display_info dispInfo;
    DbgAssert(DrvVirtioGpu_GetDispInfo(gpu, &dispInfo));

    // Determine resolution to use
    int resWidth = 1280;
    int resHeight = 720;
    /*for (size_t i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++)
    {
        if (dispInfo.pmodes[i].enabled)
        {
            resWidth = dispInfo.pmodes[i].r.width;
            resHeight = dispInfo.pmodes[i].r.height;
            break;
        }
    }*/

    TmPrintf("[VirtIO-GPU] Using resolution: %dx%d\n", resWidth, resHeight);
    gpu->Width = resWidth;
    gpu->Height = resHeight;

    // Allocate framebuffer memory
    size_t fbByteSize = resWidth * resHeight * 4;
    size_t fbPageCount = KPAGE_COUNT(fbByteSize);
    kphys_t framebufferPhys = PhysAlloc(fbPageCount, PHYS_REGION_TYPE_HARDWARE, "virtio-gpu-fb");
    uint8_t* framebuffer = VirtAlloc(framebufferPhys, fbPageCount, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_HARDWARE, "virtio-gpu-fb");
    memset(framebuffer, 0, fbByteSize);

    // Create framebuffer resource
    DbgAssert(DrvVirtioGpu_CreateResource2D(gpu, 1, VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM, resWidth, resHeight));
    // Attach guest memory to framebuffer
    DbgAssert(DrvVirtioGpu_AttachResourceBacking(gpu, 1, framebufferPhys, fbPageCount * KPAGE_SIZE));
    // Make framebuffer current in scanout
    DbgAssert(DrvVirtioGpu_SetScanout(gpu, 0, 1, 0, 0, resWidth, resHeight));
    // Display framebuffer
    DrvVirtioGpu_Present(gpu);

    // Initialize framebuffer console
    ConInitialize(framebuffer, resWidth, resHeight);

    return true;
}

static DrvVirtioGpu* CurrentGpu = NULL;

void DrvVirtioGpu_Present(DrvVirtioGpu* gpu)
{
    CurrentGpu = gpu;
    DbgAssert(DrvVirtioGpu_TransferToHost2D(gpu, 1, 0, 0, gpu->Width, gpu->Height));
    DbgAssert(DrvVirtioGpu_ResourceFlush(gpu, 1, 0, 0, gpu->Width, gpu->Height));
}

void DrvVirtioGpu_PresentActive()
{
    DrvVirtioGpu_Present(CurrentGpu);
}
