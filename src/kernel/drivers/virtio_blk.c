#include "../tsc.h"
#include "../debug.h"
#include "../memory.h"
#include "../textmode.h"
#include "../scheduler.h"
#include "../interrupts.h"
#include "virtio_blk.h"

DrvVirtioBlk* DrvVirtioBlk_Create(const PciDeviceInfo* pciInfo)
{
    if (pciInfo->VendorId != 0x1af4 || pciInfo->DeviceId != 0x1001)
        return false;

    DrvVirtioBlk* drv = kcalloc(sizeof(DrvVirtioBlk));
    if (!DrvVirtioCreate(&drv->Drv, pciInfo))
    {
        kfree(drv);
        return NULL;
    }

    return drv;
}

static void DrvVirtioBlk_Process(DrvVirtioBlk* drv, size_t queue);

static void DrvVirtioBlk_OnInterrupt(DrvVirtio* drv)
{
    DrvVirtioBlk* blk = (DrvVirtioBlk*)drv;
    TmPrintf("[VirtIO-BLK] !!! Interrupt caught !!!\n");
    DrvVirtioBlk_Process(blk, 0);
}

bool DrvVirtioBlk_Start(DrvVirtioBlk* drv)
{
    uint32_t blkReqFeatures[2] = {0, (1 << (VIRTIO_F_VERSION_1 - 32))};
    uint32_t blkOptFeatures[2] = {(1 << VIRTIO_BLK_F_RO) | (1 << VIRTIO_BLK_F_BLK_SIZE) | (1 << VIRTIO_BLK_F_DISCARD) | (1 << VIRTIO_BLK_F_WRITE_ZEROES), 0};
    drv->Drv.InterruptFn = DrvVirtioBlk_OnInterrupt;
    return DrvVirtioStart(&drv->Drv, blkReqFeatures, blkOptFeatures);
}

typedef struct DrvVirtioBlk_IoOp
{
    uint32_t Id;
    bool Finished;
    size_t Transferred;
    AsyncCall* AsyncCall;
    AsyncCallbackFn AsyncCallback;
    virtio_blk_req Req;
    uint8_t ReturnCode;
} DrvVirtioBlk_IoOp;

static void DrvVirtioBlk_ProcessOne(DrvVirtioBlk* drv, size_t queue, vring_used_elem* elem)
{
    vring* q = &drv->Drv.Queues[queue];
    vring_desc* head = &q->desc[elem->id];
    virtio_blk_req* req = (virtio_blk_req*)PhysToVirt(KPHYS(head->addr));
    DrvVirtioBlk_IoOp* op = CONTAINING_RECORD(req, DrvVirtioBlk_IoOp, Req);
    TmPrintf("[VirtIO-BLK] IO operation #%u finished [qdesc=%u, xfer=%u, err=%u]\n", op->Id, elem->id, elem->len, op->ReturnCode);
    DrvVirtioRing_FreeChain(&drv->Drv, queue, elem->id);
    op->Finished = true;
    op->Transferred = elem->len - 1;
    if (op->AsyncCall != NULL)
    {
        op->AsyncCall->Success = op->ReturnCode == 0;
        op->AsyncCall->Transferred = op->Transferred;
        if (op->AsyncCall->Event != NULL)
            SchEventSignal(op->AsyncCall->Event); // TODO: hmm, what to do if this enables interrupts?
        if (op->AsyncCallback != NULL)
            op->AsyncCallback(op->AsyncCall); // TODO: hmm, what to do if this enables interrupts?
    }
}

static void DrvVirtioBlk_Process(DrvVirtioBlk* drv, size_t queue)
{
    vring* q = &drv->Drv.Queues[queue];
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

        DrvVirtioBlk_ProcessOne(drv, queue, elem);
    }
}

static uint32_t DrvVirtioBlk_NextOpId = 1;

size_t DrvVirtioBlk_Read(DrvVirtioBlk* drv, uint64_t sector, void* user_buf, size_t user_len)
{
    // Build request
    DrvVirtioBlk_IoOp* op = kalloc(sizeof(DrvVirtioBlk_IoOp));
    op->Id = DrvVirtioBlk_NextOpId++;
    op->Finished = false;
    op->Transferred = 0;
    op->AsyncCall = NULL;
    op->AsyncCallback = NULL;
    op->Req.type = VIRTIO_BLK_T_IN;
    op->Req.ioprio = 0;
    op->Req.sector = sector;
    op->ReturnCode = 0;

    // Allocate and set up buffer descriptors
    vring_desc* descs[3];
    while (!DrvVirtioRing_AllocDescs(&drv->Drv, 0, descs, 3))
        SchYield();

    // Request header is device read-only
    descs[0]->addr = VirtToPhys(&op->Req);
    descs[0]->len = sizeof(virtio_blk_req);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&drv->Drv, 0, descs[1]);

    // Request body is device write-only
    descs[1]->addr = VirtToPhys(user_buf);
    descs[1]->len = user_len;
    descs[1]->flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    descs[1]->next = DrvVirtioRing_DescIndex(&drv->Drv, 0, descs[2]);

    // Request footer is device write-only
    descs[2]->addr = VirtToPhys(&op->ReturnCode);
    descs[2]->len = sizeof(uint8_t);
    descs[2]->flags = VRING_DESC_F_WRITE;
    descs[2]->next = UINT16_MAX;

    // Submit & wait for completion
    uint32_t irqLock = IntEnterCriticalSection();
    {
        DrvVirtioRing_BatchAdd(&drv->Drv, 0, &descs[0], 1); // only add desciptor chain heads
        DrvVirtioRing_BatchComplete(&drv->Drv, 0);
        TmPrintf("[VirtIO-BLK] IO operation #%u submitted [sector=%llu, length=%u]\n", op->Id, sector, user_len);
    }
    IntLeaveCriticalSection(irqLock);

    while (!op->Finished)
        DrvVirtioBlk_Process(drv, 0);

    // Free request
    size_t result = op->ReturnCode == 0 ? op->Transferred : 0;
    kfree(op);
    return result;
}

void DrvVirtioBlk_ReadAsync(DrvVirtioBlk* drv, uint64_t sector, void* user_buf, size_t user_len, AsyncCall* call, AsyncCallbackFn* fn)
{
    DbgAssert(call != NULL);

    // Build request
    DrvVirtioBlk_IoOp* op = kalloc(sizeof(DrvVirtioBlk_IoOp));
    op->Id = DrvVirtioBlk_NextOpId++;
    op->Finished = false;
    op->Transferred = 0;
    op->AsyncCall = call;
    op->AsyncCallback = fn;
    op->Req.type = VIRTIO_BLK_T_IN;
    op->Req.ioprio = 0;
    op->Req.sector = sector;
    op->ReturnCode = 0;

    // Allocate and set up buffer descriptors
    vring_desc* descs[3];
    while (!DrvVirtioRing_AllocDescs(&drv->Drv, 0, &descs, 3))
        SchYield();

    // Request header is device read-only
    descs[0]->addr = VirtToPhys(&op->Req);
    descs[0]->len = sizeof(virtio_blk_req);
    descs[0]->flags = VRING_DESC_F_NEXT;
    descs[0]->next = DrvVirtioRing_DescIndex(&drv->Drv, 0, descs[1]);

    // Request body is device write-only
    descs[1]->addr = VirtToPhys(user_buf);
    descs[1]->len = user_len;
    descs[1]->flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    descs[1]->next = DrvVirtioRing_DescIndex(&drv->Drv, 0, descs[2]);

    // Request footer is device write-only
    descs[2]->addr = VirtToPhys(&op->ReturnCode);
    descs[2]->len = sizeof(uint8_t);
    descs[2]->flags = VRING_DESC_F_WRITE;
    descs[2]->next = UINT16_MAX;

    // Submit
    uint32_t irqLock = IntEnterCriticalSection();
    {
        DrvVirtioRing_BatchAdd(&drv->Drv, 0, &descs[0], 1); // only add desciptor chain heads
        DrvVirtioRing_BatchComplete(&drv->Drv, 0);
        TmPrintf("[VirtIO-BLK] IO operation #%u submitted [sector=%llu, length=%u, !ASYNC!]\n", op->Id, sector, user_len);
    }
    IntLeaveCriticalSection(irqLock);
}

size_t DrvVirtioBlk_Write(DrvVirtioBlk* drv, uint64_t sector, const void* buf, size_t len)
{
    return (size_t)-1;
}
