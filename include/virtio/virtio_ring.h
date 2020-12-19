#ifndef VIRTIO_VIRTIO_RING_H
#define VIRTIO_VIRTIO_RING_H

#include <stdint.h>

#define VRING_DESC_F_NEXT      1 /* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_WRITE     2 /* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_INDIRECT  4 /* This means the buffer contains a list of buffer descriptors. */

/* The Host uses this in used->flags to advise the Guest: don't kick me when
 * you add a buffer.  It's unreliable, so it's simply an optimization.  Guest
 * will still kick if it's out of buffers. */
#define VRING_USED_F_NO_NOTIFY         1

/* The Guest uses this in avail->flags to advise the Host: don't interrupt me
 * when you consume a buffer.  It's unreliable, so it's simply an
 * optimization.  */
#define VRING_AVAIL_F_NO_INTERRUPT     1

/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC    28

/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field. */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field. */
#define VIRTIO_RING_F_EVENT_IDX        29

/* Alignment requirements for vring elements.
 * When using pre-virtio 1.0 layout, these fall out naturally.
 */
#define VRING_AVAIL_ALIGN_SIZE 2
#define VRING_USED_ALIGN_SIZE  4
#define VRING_DESC_ALIGN_SIZE  16

#pragma pack(push, 1)

/* Virtio ring descriptors: 16 bytes.  These can chain together via "next". */
typedef struct vring_desc
{
    uint64_t addr; /* Address (guest-physical). */
    uint32_t len; /* Length. */
    uint16_t flags; /* The flags as indicated above. */
    uint16_t next; /* We chain unused descriptors via this, too */
} vring_desc;

typedef struct vring_avail
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vring_avail;

typedef struct vring_used_elem
{
    uint32_t id; /* Index of start of used descriptor chain. */
    uint32_t len; /* Total length of the descriptor chain which was used (written to) */
} vring_used_elem;

typedef struct vring_used
{
    uint16_t flags;
    uint16_t idx;
    vring_used_elem ring[];
} vring_used;

typedef struct vring
{
    uint32_t num;
    vring_desc* desc;
    vring_avail* avail;
    vring_used* used;
    vring_desc* first_unused_desc; // [Futura extra]
    uint32_t num_unused_desc; // [Futura extra]
    uint32_t num_pending; // [Futura extra]
    uint16_t last_seen_used; // [Futura extra]
} vring;

#pragma pack(pop)

static inline int vring_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
    return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old);
}

#endif
