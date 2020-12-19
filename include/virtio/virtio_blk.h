#ifndef VIRTIO_VIRTIO_BLK_H
#define VIRTIO_VERTIO_BLK_H

#include <stdint.h>

// Feature flags
#define VIRTIO_BLK_F_SIZE_MAX        1  /* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX         2  /* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY        4  /* Legacy geometry available  */
#define VIRTIO_BLK_F_RO              5  /* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE        6  /* Block size of disk is available*/
#define VIRTIO_BLK_F_TOPOLOGY        10 /* Topology information is available */
#define VIRTIO_BLK_F_MQ              12 /* support more than one vq */
#define VIRTIO_BLK_F_DISCARD         13 /* DISCARD is supported */
#define VIRTIO_BLK_F_WRITE_ZEROES    14 /* WRITE ZEROES is supported */

// Command types
#define VIRTIO_BLK_T_IN              0  /* This is a flag! Specifies direction. */
#define VIRTIO_BLK_T_OUT             1  /* This is a flag! Specifies direction. */
#define VIRTIO_BLK_T_FLUSH           4  /* Cache flush command */
#define VIRTIO_BLK_T_GET_ID          8  /* Get device ID command */
#define VIRTIO_BLK_T_DISCARD         11 /* Discard command */
#define VIRTIO_BLK_T_WRITE_ZEROES    13 /* Write zeroes command */

// Command result
#define VIRTIO_BLK_S_OK              0 /* Success */
#define VIRTIO_BLK_S_IOERR           1 /* Device or driver error */
#define VIRTIO_BLK_S_UNSUPP          2 /* Request unsupported by device */

#pragma pack(push, 1)

typedef volatile struct virtio_blk_config
{
    /* The capacity (in 512-byte sectors). */
    uint64_t capacity;
    /* The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX) */
    uint32_t size_max;
    /* The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX) */
    uint32_t seg_max;
    /* geometry of the device (if VIRTIO_BLK_F_GEOMETRY) */
    struct virtio_blk_geometry
    {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    /* block size of device (if VIRTIO_BLK_F_BLK_SIZE) */
    uint32_t blk_size;

    /* the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY  */
    /* exponent for physical block per logical block. */
    uint8_t physical_block_exp;
    /* alignment offset in logical blocks. */
    uint8_t alignment_offset;
    /* minimum I/O size without performance penalty in logical blocks. */
    uint16_t min_io_size;
    /* optimal sustained I/O size in logical blocks. */
    uint32_t opt_io_size;

    /* writeback mode (if VIRTIO_BLK_F_CONFIG_WCE) */
    uint8_t wce;
    uint8_t unused;

    /* number of vqs, only available when VIRTIO_BLK_F_MQ is set */
    uint16_t num_queues;

    /* the next 3 entries are guarded by VIRTIO_BLK_F_DISCARD */
    /*
     * The maximum discard sectors (in 512-byte sectors) for
     * one segment.
     */
    uint32_t max_discard_sectors;
    /*
     * The maximum number of discard segments in a
     * discard command.
     */
    uint32_t max_discard_seg;
    /* Discard commands must be aligned to this number of sectors. */
    uint32_t discard_sector_alignment;

    /* the next 3 entries are guarded by VIRTIO_BLK_F_WRITE_ZEROES */
    /*
     * The maximum number of write zeroes sectors (in 512-byte sectors) in
     * one segment.
     */
    uint32_t max_write_zeroes_sectors;
    /*
     * The maximum number of segments in a write zeroes
     * command.
     */
    uint32_t max_write_zeroes_seg;
    /*
     * Set if a VIRTIO_BLK_T_WRITE_ZEROES request may result in the
     * deallocation of one or more of the sectors.
     */
    uint8_t write_zeroes_may_unmap;

    uint8_t unused1[3];
} virtio_blk_config;

typedef struct virtio_blk_req
{
    uint32_t type;   /* VIRTIO_BLK_T* */
    uint32_t ioprio; /* io priority. */
    uint64_t sector; /* Sector (ie. 512 byte offset) */
} virtio_blk_req;

typedef struct virtio_blk_discard_write_zeroes
{
    uint64_t sector;      /* discard/write zeroes start sector */
    uint32_t num_sectors; /* number of discard/write zeroes sectors */
    uint32_t flags;       /* flags for this range */
} virtio_blk_discard_write_zeroes;

#pragma pack(pop)

#endif
