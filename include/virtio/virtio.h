#ifndef VIRTIO_VIRTIO_H
#define VIRTIO_VIRTIO_H

#include <stdint.h>
#include "virtio_ring.h"
#include "virtio_pci.h"
#include "virtio_blk.h"
#include "virtio_gpu.h"

// Device status bits
#define VIRTIO_CONFIG_S_ACKNOWLEDGE  1   /* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_DRIVER       2   /* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER_OK    4   /* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_FEATURES_OK  8   /* Driver has finished configuring features */
#define VIRTIO_CONFIG_S_NEEDS_RESET  64  /* Device entered invalid state, driver must reset it */
#define VIRTIO_CONFIG_S_FAILED       128 /* We've given up on this device. */

// Feature bits
#define VIRTIO_F_VERSION_1          32 /* v1.0 compliant. */
#define VIRTIO_F_ACCESS_PLATFORM    33
#define VIRTIO_F_RING_PACKED        34 /* This feature indicates support for the packed virtqueue layout. */
#define VIRTIO_F_ORDER_PLATFORM     36
#define VIRTIO_F_SR_IOV             37

#endif
