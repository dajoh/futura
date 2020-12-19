#ifndef VIRTIO_VIRTIO_PCI_H
#define VIRTIO_VIRTIO_PCI_H

#include <stdint.h>

// The bit of the ISR which indicates a device configuration change.
#define VIRTIO_PCI_ISR_CONFIG 0x02

// Vector value used to disable MSI for queue
#define VIRTIO_MSI_NO_VECTOR 0xFFFF

// Capability config types
#define VIRTIO_PCI_CAP_COMMON_CFG    1 /* Common configuration */
#define VIRTIO_PCI_CAP_NOTIFY_CFG    2 /* Notifications */
#define VIRTIO_PCI_CAP_ISR_CFG       3 /* ISR access */
#define VIRTIO_PCI_CAP_DEVICE_CFG    4 /* Device specific configuration */
#define VIRTIO_PCI_CAP_PCI_CFG       5 /* PCI configuration access */

// PCI capability list record offsets
#define VIRTIO_PCI_CAP_VNDR          0
#define VIRTIO_PCI_CAP_NEXT          1
#define VIRTIO_PCI_CAP_LEN           2
#define VIRTIO_PCI_CAP_CFG_TYPE      3
#define VIRTIO_PCI_CAP_BAR           4
#define VIRTIO_PCI_CAP_OFFSET        8
#define VIRTIO_PCI_CAP_LENGTH        12
#define VIRTIO_PCI_NOTIFY_CAP_MULT   16

#pragma pack(push, 1)
typedef volatile struct virtio_pci_common_cfg
{
    /* About the whole device. */
    uint32_t device_feature_select; /* read-write */
    uint32_t device_feature;        /* read-only */
    uint32_t guest_feature_select;  /* read-write */
    uint32_t guest_feature;         /* read-write */
    uint16_t msix_config;           /* read-write */
    uint16_t num_queues;            /* read-only */
    uint8_t device_status;          /* read-write */
    uint8_t config_generation;      /* read-only */

    /* About a specific virtqueue. */
    uint16_t queue_select;       /* read-write */
    uint16_t queue_size;         /* read-write, power of 2. */
    uint16_t queue_msix_vector;  /* read-write */
    uint16_t queue_enable;       /* read-write */
    uint16_t queue_notify_off;   /* read-only */
    uint64_t queue_desc;         /* read-write */
    uint64_t queue_avail;        /* read-write */
    uint64_t queue_used;         /* read-write */
} virtio_pci_common_cfg;
#pragma pack(pop)

#endif
