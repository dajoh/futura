#ifndef VIRTIO_VIRTIO_GPU_H
#define VIRTIO_VERTIO_GPU_H

#include <stdint.h>

// Feature flags
#define VIRTIO_GPU_F_VIRGL               0
#define VIRTIO_GPU_F_EDID                1
#define VIRTIO_GPU_F_RESOURCE_UUID       2

// Queues
#define VIRTIO_GPU_Q_CONTROL             0
#define VIRTIO_GPU_Q_CURSOR              1

#pragma pack(push, 1)

enum virtio_gpu_ctrl_type
{
    VIRTIO_GPU_UNDEFINED = 0,

    /* 2d commands */
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,
    VIRTIO_GPU_CMD_GET_EDID,
    VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID,

    /* 3d commands */
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
    VIRTIO_GPU_CMD_SUBMIT_3D,

    /* cursor commands */
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR,

    /* success responses */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,
    VIRTIO_GPU_RESP_OK_EDID,
    VIRTIO_GPU_RESP_OK_RESOURCE_UUID,

    /* error responses */
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)
typedef struct virtio_gpu_ctrl_hdr
{
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} virtio_gpu_ctrl_hdr;

/* data passed in the cursor vq */

typedef struct virtio_gpu_cursor_pos
{
    uint32_t scanout_id;
    uint32_t x;
    uint32_t y;
    uint32_t padding;
} virtio_gpu_cursor_pos;

/* VIRTIO_GPU_CMD_UPDATE_CURSOR, VIRTIO_GPU_CMD_MOVE_CURSOR */
typedef struct virtio_gpu_update_cursor
{
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_cursor_pos pos;      /* update & move */
    uint32_t resource_id;           /* update only */
    uint32_t hot_x;                 /* update only */
    uint32_t hot_y;                 /* update only */
    uint32_t padding;
} virtio_gpu_update_cursor;

/* data passed in the control vq, 2d related */

typedef struct virtio_gpu_rect
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_rect;

/* VIRTIO_GPU_CMD_RESOURCE_UNREF */
typedef struct virtio_gpu_resource_unref
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_unref;

/* VIRTIO_GPU_CMD_RESOURCE_CREATE_2D: create a 2d resource with a format */
typedef struct virtio_gpu_resource_create_2d
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} virtio_gpu_resource_create_2d;

/* VIRTIO_GPU_CMD_SET_SCANOUT */
typedef struct virtio_gpu_set_scanout
{
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} virtio_gpu_set_scanout;

/* VIRTIO_GPU_CMD_RESOURCE_FLUSH */
typedef struct virtio_gpu_resource_flush
{
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_flush;

/* VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D: simple transfer to_host */
typedef struct virtio_gpu_transfer_to_host_2d
{
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_transfer_to_host_2d;

typedef struct virtio_gpu_mem_entry
{
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} virtio_gpu_mem_entry;

/* VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING */
typedef struct virtio_gpu_resource_attach_backing
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_resource_attach_backing;

/* VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING */
typedef struct virtio_gpu_resource_detach_backing
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_detach_backing;

/* VIRTIO_GPU_RESP_OK_DISPLAY_INFO */
#define VIRTIO_GPU_MAX_SCANOUTS 16
typedef struct virtio_gpu_resp_display_info
{
    virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_display_one
    {
        virtio_gpu_rect r;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} virtio_gpu_resp_display_info;

/* data passed in the control vq, 3d related */

typedef struct virtio_gpu_box
{
    uint32_t x, y, z;
    uint32_t w, h, d;
} virtio_gpu_box;

/* VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D, VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D */
typedef struct virtio_gpu_transfer_host_3d
{
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_box box;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t level;
    uint32_t stride;
    uint32_t layer_stride;
} virtio_gpu_transfer_host_3d;

/* VIRTIO_GPU_CMD_RESOURCE_CREATE_3D */
#define VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP (1 << 0)
typedef struct virtio_gpu_resource_create_3d
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t target;
    uint32_t format;
    uint32_t bind;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t nr_samples;
    uint32_t flags;
    uint32_t padding;
} virtio_gpu_resource_create_3d;

/* VIRTIO_GPU_CMD_CTX_CREATE */
typedef struct virtio_gpu_ctx_create
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t nlen;
    uint32_t padding;
    char debug_name[64];
} virtio_gpu_ctx_create;

/* VIRTIO_GPU_CMD_CTX_DESTROY */
typedef struct virtio_gpu_ctx_destroy
{
    virtio_gpu_ctrl_hdr hdr;
} virtio_gpu_ctx_destroy;

/* VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE, VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE */
typedef struct virtio_gpu_ctx_resource
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_ctx_resource;

/* VIRTIO_GPU_CMD_SUBMIT_3D */
typedef struct virtio_gpu_cmd_submit
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t size;
    uint32_t padding;
} virtio_gpu_cmd_submit;

/* VIRTIO_GPU_CMD_GET_CAPSET_INFO */
#define VIRTIO_GPU_CAPSET_VIRGL 1
#define VIRTIO_GPU_CAPSET_VIRGL2 2
typedef struct virtio_gpu_get_capset_info
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t capset_index;
    uint32_t padding;
} virtio_gpu_get_capset_info;

/* VIRTIO_GPU_RESP_OK_CAPSET_INFO */
typedef struct virtio_gpu_resp_capset_info
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t capset_id;
    uint32_t capset_max_version;
    uint32_t capset_max_size;
    uint32_t padding;
} virtio_gpu_resp_capset_info;

/* VIRTIO_GPU_CMD_GET_CAPSET */
typedef struct virtio_gpu_get_capset
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t capset_id;
    uint32_t capset_version;
} virtio_gpu_get_capset;

/* VIRTIO_GPU_RESP_OK_CAPSET */
typedef struct virtio_gpu_resp_capset
{
    virtio_gpu_ctrl_hdr hdr;
    uint8_t capset_data[];
} virtio_gpu_resp_capset;

/* VIRTIO_GPU_CMD_GET_EDID */
typedef struct virtio_gpu_cmd_get_edid
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t scanout;
    uint32_t padding;
} virtio_gpu_cmd_get_edid;

/* VIRTIO_GPU_RESP_OK_EDID */
typedef struct virtio_gpu_resp_edid
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t size;
    uint32_t padding;
    uint8_t edid[1024];
} virtio_gpu_resp_edid;

#define VIRTIO_GPU_EVENT_DISPLAY (1 << 0)
typedef volatile struct virtio_gpu_config
{
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
} virtio_gpu_config;

/* simple formats for fbcon/X use */
enum virtio_gpu_formats
{
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  = 4,

    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM  = 68,

    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM  = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM  = 134,
};

/* VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID */
typedef struct virtio_gpu_resource_assign_uuid
{
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_assign_uuid;

/* VIRTIO_GPU_RESP_OK_RESOURCE_UUID */
typedef struct virtio_gpu_resp_resource_uuid
{
    virtio_gpu_ctrl_hdr hdr;
    uint8_t uuid[16];
} virtio_gpu_resp_resource_uuid;

#pragma pack(pop)

#endif
