#include <string.h>
#include "virtio.h"
#include "../debug.h"
#include "../memory.h"
#include "../textmode.h"
#include "../interrupts.h"

bool DrvVirtioCreate(DrvVirtio* drv, const PciDeviceInfo* pciInfo)
{
    if (pciInfo->VendorId != 0x1af4)
        return false;
    if (pciInfo->DeviceId != 0x1001 && pciInfo->DeviceId != 0x1050)
        return false;

    uint32_t bus = pciInfo->Bus;
    uint32_t device = pciInfo->Device;
    uint32_t function = pciInfo->Function;
    k_memset(drv, 0, sizeof(DrvVirtio));

    drv->DeviceType = pciInfo->DeviceId == 0x1001 ? DRV_VIRTIO_DEVICE_TYPE_BLK : DRV_VIRTIO_DEVICE_TYPE_GPU;
    drv->PciVendorId = pciInfo->VendorId;
    drv->PciDeviceId = pciInfo->DeviceId;
    drv->PciBus = pciInfo->Bus;
    drv->PciDevice = pciInfo->Device;
    drv->PciFunction = pciInfo->Function;
    drv->PciBaseClass = pciInfo->BaseClass;
    drv->PciSubClass = pciInfo->SubClass;
    drv->PciIntLine = PciReadByte(bus, device, function, PCI_OFFSET_INT_LINE);
    drv->PciIntPin = PciReadByte(bus, device, function, PCI_OFFSET_INT_PIN);

    uint32_t barAddrs[6] =
    {
        PciReadLong(bus, device, function, PCI_OFFSET_BAR0) & 0xFFFFFFF0,
        PciReadLong(bus, device, function, PCI_OFFSET_BAR1) & 0xFFFFFFF0,
        PciReadLong(bus, device, function, PCI_OFFSET_BAR2) & 0xFFFFFFF0,
        PciReadLong(bus, device, function, PCI_OFFSET_BAR3) & 0xFFFFFFF0,
        PciReadLong(bus, device, function, PCI_OFFSET_BAR4) & 0xFFFFFFF0,
        PciReadLong(bus, device, function, PCI_OFFSET_BAR5) & 0xFFFFFFF0
    };

    uint8_t cap_ptr = PciReadByte(bus, device, function, PCI_OFFSET_CAP_PTR) & 0xFC;
    while (cap_ptr != 0x00)
    {
        uint8_t cap_vndr = PciReadByte(bus, device, function, cap_ptr+VIRTIO_PCI_CAP_VNDR);
        uint8_t cap_next = PciReadByte(bus, device, function, cap_ptr+VIRTIO_PCI_CAP_NEXT);
        if (cap_vndr == 0x09)
        {
            uint8_t cfg_type = PciReadByte(bus, device, function, cap_ptr+VIRTIO_PCI_CAP_CFG_TYPE);
            uint8_t bar      = PciReadByte(bus, device, function, cap_ptr+VIRTIO_PCI_CAP_BAR);
            uint32_t offset  = PciReadLong(bus, device, function, cap_ptr+VIRTIO_PCI_CAP_OFFSET);
            uint32_t length  = PciReadLong(bus, device, function, cap_ptr+VIRTIO_PCI_CAP_LENGTH);
            switch (cfg_type)
            {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                drv->CommonCfgPhys = barAddrs[bar] + offset;
                drv->CommonCfgSize = length;
                break;
            case VIRTIO_PCI_CAP_NOTIFY_CFG:
                drv->NotifyCfgPhys = barAddrs[bar] + offset;
                drv->NotifyCfgSize = length;
                drv->NotifyCfgMult = PciReadLong(bus, device, function, cap_ptr+VIRTIO_PCI_NOTIFY_CAP_MULT);
                break;
            case VIRTIO_PCI_CAP_ISR_CFG:
                drv->IsrCfgPhys = barAddrs[bar] + offset;
                drv->IsrCfgSize = length;
                break;
            case VIRTIO_PCI_CAP_DEVICE_CFG:
                drv->DeviceCfgPhys = barAddrs[bar] + offset;
                drv->DeviceCfgSize = length;
                break;
            }
        }
        cap_ptr = cap_next;
    }

    drv->CommonCfg = VirtAllocUnaligned(drv->CommonCfgPhys, KPAGE_COUNT(drv->CommonCfgSize), VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_HARDWARE, "virtio-cfg");
    drv->NotifyCfg = VirtAllocUnaligned(drv->NotifyCfgPhys, KPAGE_COUNT(drv->NotifyCfgSize), VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_HARDWARE, "virtio-nfy");
    drv->IsrCfg    = VirtAllocUnaligned(drv->IsrCfgPhys,    KPAGE_COUNT(drv->IsrCfgSize),    VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_HARDWARE, "virtio-isr");
    drv->DeviceCfg = VirtAllocUnaligned(drv->DeviceCfgPhys, KPAGE_COUNT(drv->DeviceCfgSize), VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_HARDWARE, "virtio-dev");

    return true;
}

static void DrvVirtio_HandleISR(void* ctx)
{
    DrvVirtio* drv = (DrvVirtio*)ctx;
    DrvVirtioReadISR(drv);
    if (drv->InterruptFn)
        drv->InterruptFn(drv);
}

bool DrvVirtioStart(DrvVirtio* drv, uint32_t reqFeatures[2], uint32_t optFeatures[2])
{
    virtio_pci_common_cfg* cfg = drv->CommonCfg;

    // 1. Reset the device. Writing 0 into this field resets the device.
    cfg->device_status = 0;

    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    cfg->device_status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;

    // 3. Set the DRIVER status bit: the guest OS knows how to drive the device.
    cfg->device_status |= VIRTIO_CONFIG_S_DRIVER;

    // 4a. Read device feature bits
    for (size_t i = 0; i < 2; i++)
    {
        cfg->device_feature_select = i;
        drv->DevFeatures[i] = cfg->device_feature;
        drv->DrvFeatures[i] = drv->DevFeatures[i] & (reqFeatures[i] | optFeatures[i]);
    }

    // Dump feature flags...
    TmPrintf("devfeatures0: %08X    devfeatures1: %08X\n", drv->DevFeatures[0], drv->DevFeatures[1]);
    TmPrintf("drvfeatures0: %08X    drvfeatures1: %08X\n", drv->DrvFeatures[0], drv->DrvFeatures[1]);
    TmPrintf("reqfeatures0: %08X    reqfeatures1: %08X\n", reqFeatures[0], reqFeatures[1]);
    TmPrintf("optfeatures0: %08X    optfeatures1: %08X\n", optFeatures[0], optFeatures[1]);

    // 4b. Write the subset of feature bits understood by the OS and driver to the device
    for (size_t i = 0; i < 2; i++)
    {
        if ((drv->DevFeatures[i] & reqFeatures[i]) != reqFeatures[i])
        {
            DbgPanic("required virtio device feature missing");
            return false;
        }
        cfg->guest_feature_select = i;
        cfg->guest_feature = drv->DrvFeatures[i];
    }

    // 5. Set the FEATURES_OK status bit.
    cfg->device_status |= VIRTIO_CONFIG_S_FEATURES_OK;

    // 6. Re-read device status to ensure the FEATURES_OK bit is still set
    DbgAssert((cfg->device_status & VIRTIO_CONFIG_S_FEATURES_OK) != 0);

    // 7. Perform device-specific setup, including discovery of virtqueues for the device
    drv->NumQueues = cfg->num_queues;
    drv->Queues = kalloc(sizeof(vring) * drv->NumQueues);
    for (uint32_t i = 0; i < drv->NumQueues; i++)
    {
        cfg->queue_select = i;
        size_t queueSize = cfg->queue_size;
        size_t descTableSize = sizeof(vring_desc) * queueSize;
        size_t availRingSize = sizeof(vring_avail) + sizeof(uint16_t) + sizeof(uint16_t) * queueSize;
        size_t usedRingSize = sizeof(vring_used) + sizeof(uint16_t) + sizeof(vring_used_elem) * queueSize;
        size_t totalSize = descTableSize + availRingSize;
        size_t padding = totalSize % VRING_USED_ALIGN_SIZE == 0 ? 0 : VRING_USED_ALIGN_SIZE - (totalSize % VRING_USED_ALIGN_SIZE);
        totalSize += padding + usedRingSize;
        size_t pageCount = (totalSize + (KPAGE_SIZE - 1)) / KPAGE_SIZE;

        TmPrintf("Queue #%u    MSI-X:         0x%04X\n", i+1, cfg->queue_msix_vector);
        TmPrintf("Queue #%u    Enable:        %u\n", i+1, cfg->queue_enable);
        TmPrintf("Queue #%u    NotifyOff:     %u\n", i+1, cfg->queue_notify_off);
        TmPrintf("Queue #%u    Desc:          0x%08llX\n", i+1, cfg->queue_desc);
        TmPrintf("Queue #%u    Avail:         0x%08llX\n", i+1, cfg->queue_avail);
        TmPrintf("Queue #%u    Used:          0x%08llX\n", i+1, cfg->queue_used);
        TmPrintf("Queue #%u    QueueSize:     %u\n", i+1, queueSize);
        TmPrintf("Queue #%u    DescTableSize: %u\n", i+1, descTableSize);
        TmPrintf("Queue #%u    AvailRingSize: %u\n", i+1, availRingSize);
        TmPrintf("Queue #%u    UsedRingSize:  %u\n", i+1, usedRingSize);
        TmPrintf("Queue #%u    Padding:       %u\n", i+1, padding);
        TmPrintf("Queue #%u    TotalSize:     %u\n", i+1, totalSize);
        TmPrintf("Queue #%u    PageCount:     %u\n", i+1, pageCount);

        // Allocate memory for queue
        kphys_t queuePhys = PhysAlloc(pageCount, PHYS_REGION_TYPE_HARDWARE, "virtq");
        uint8_t* queueMemory = VirtAlloc(queuePhys, pageCount, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_HARDWARE, "virtq");
        k_memset(queueMemory, 0, totalSize);

        // Setup queue memory
        vring* ring = &drv->Queues[i];
        ring->num = queueSize;
        ring->desc = (vring_desc*)(queueMemory);
        ring->avail = (vring_avail*)(queueMemory + descTableSize);
        ring->used = (vring_used*)(queueMemory + descTableSize + availRingSize + padding);
        ring->first_unused_desc = ring->desc;
        ring->num_unused_desc = queueSize;
        ring->num_pending = 0;
        ring->last_seen_used = 0;
        for (size_t j = 0; j < queueSize - 1; j++)
        {
            ring->desc[j].flags = VRING_DESC_F_NEXT;
            ring->desc[j].next = j + 1;
        }

        // Enable queue
        cfg->queue_desc = VirtToPhys(ring->desc);
        cfg->queue_avail = VirtToPhys(ring->avail);
        cfg->queue_used = VirtToPhys(ring->used);
        cfg->queue_enable = 1;
        TmPrintf("[VirtIO] Enabled Queue #%u\n", i+1);
    }

    // Find the ISR and subscribe..
    if (drv->PciIntPin != PCI_INT_PIN_NONE)
    {
        drv->Interrupt = PciLookupIntPinISR(drv->PciBus, drv->PciDevice, drv->PciIntPin);
        TmPrintf("[VirtIO] Using ISR%02Xh for interrupts\n", drv->Interrupt);
        IntRegisterCallback(drv->Interrupt, DrvVirtio_HandleISR, drv);
    }

    // 8. Set the DRIVER_OK status bit. At this point the device is "live".
    cfg->device_status |= VIRTIO_CONFIG_S_DRIVER_OK;

    return true;
}

uint8_t DrvVirtioReadISR(DrvVirtio* drv)
{
    // To avoid an extra access, simply reading this register resets it to 0 and causes the device to de-assert the interrupt.
    // In this way, driver read of ISR status causes the device to de-assert an interrupt.
    return *(volatile uint8_t*)drv->IsrCfg;
}

static vring_desc* DrvVirtioRing_AllocOneDesc(vring* ring)
{
    DbgAssert(ring->num_unused_desc != 0);

    vring_desc* desc = ring->first_unused_desc;

    ring->num_unused_desc--;
    if (desc->flags & VRING_DESC_F_NEXT)
        ring->first_unused_desc = &ring->desc[desc->next];
    else
        ring->first_unused_desc = NULL;

    if (ring->num_unused_desc == 0)
        DbgAssert(ring->first_unused_desc == NULL);
    else
        DbgAssert(ring->first_unused_desc != NULL);

    return desc;
}

// TODO: these functions should probably be called under an IRQ lock
bool DrvVirtioRing_AllocDescs(DrvVirtio* drv, size_t queue, vring_desc** descs, size_t count)
{
    vring* ring = &drv->Queues[queue];
    if (ring->num_unused_desc < count)
        return false;

    for (size_t i = 0; i < count; i++)
        descs[i] = DrvVirtioRing_AllocOneDesc(ring);

    return true;
}

// TODO: these functions should probably be called under an IRQ lock
void DrvVirtioRing_FreeChain(DrvVirtio* drv, size_t queue, uint16_t id)
{
    vring* ring = &drv->Queues[queue];
    vring_desc* head = &ring->desc[id];
    vring_desc* last = head;

    size_t count = 1;
    while (last->flags & VRING_DESC_F_NEXT)
    {
        count++;
        last = &ring->desc[last->next];
    }

    if (ring->first_unused_desc != NULL)
    {
        last->next = ring->first_unused_desc - ring->desc;
        last->flags = VRING_DESC_F_NEXT;
    }
    ring->first_unused_desc = head;
    ring->num_unused_desc += count;
}

void DrvVirtioRing_BatchAdd(DrvVirtio* drv, size_t queue, const vring_desc** descs, size_t count)
{
    vring* ring = &drv->Queues[queue];

    for (size_t i = 0; i < count; i++)
    {
        uint16_t index = descs[i] - ring->desc;
        ring->avail->ring[(ring->avail->idx + ring->num_pending) % ring->num] = index;
        ring->num_pending++;
    }
}

void DrvVirtioRing_BatchComplete(DrvVirtio* drv, size_t queue)
{
    vring* ring = &drv->Queues[queue];
    if (ring->num_pending == 0)
        return;

    ring->avail->idx += ring->num_pending;
    ring->num_pending = 0;

    // When VIRTIO_F_NOTIFICATION_DATA has not been negotiated, the driver sends an available buffer notification to the device by writing the 16-bit virtqueue index of this virtqueue to the Queue Notify address.
    // TODO: only notify if device wants it! not supressing!
    *(volatile uint16_t*)((uint8_t*)drv->NotifyCfg + (queue * drv->NotifyCfgMult)) = ring->avail->idx;
}
