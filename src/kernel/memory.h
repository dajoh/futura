#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <grub/multiboot.h>
#include "list.h"
#include "heap.h"

// --------------------------------------------------------------------
// General memory subsystem interface
// --------------------------------------------------------------------

typedef uintptr_t kphys_t;
typedef uintptr_t kvirt_t;

#define KPHYS(p)                     ((kphys_t)(p))
#define KVIRT(p)                     ((kvirt_t)(p))
#define KEARLY_VIRT_TO_PHYS(p)       ((kphys_t)(p) - 0xC0000000)
#define KEARLY_PHYS_TO_VIRT(p)       ((kvirt_t)(p) + 0xC0000000)
#define KPAGE_SIZE                   4096
#define KPAGE_SHIFT                  12
#define KPAGE_MASK                   (~(uintptr_t)0xFFF)
#define KPAGE_MASK64                 (~(uint64_t)0xFFF)
#define KPAGE_COUNT(n)               (((n) + 0xFFF) >> KPAGE_SHIFT)
#define KPAGE_ALIGN_UP(n)            (((uintptr_t)(n) + 0xFFF) & KPAGE_MASK)
#define KPAGE_ALIGN_DOWN(n)          (((uintptr_t)(n)) & KPAGE_MASK)
#define KPAGE_ALIGN_UP64(n)          (((uint64_t)(n) + 0xFFF) & KPAGE_MASK64)
#define KPAGE_ALIGN_DOWN64(n)        (((uint64_t)(n)) & KPAGE_MASK64)

void MemInitialize(multiboot_info_t* info);
void MemDebugDump();

// --------------------------------------------------------------------
// Physical Memory Manager
// --------------------------------------------------------------------

#define PHYS_REGION_TYPE_E820_AVAILABLE      100
#define PHYS_REGION_TYPE_E820_RESERVED       101
#define PHYS_REGION_TYPE_E820_ACPI           102
#define PHYS_REGION_TYPE_E820_NVS            103
#define PHYS_REGION_TYPE_E820_BAD            104
#define PHYS_REGION_TYPE_E820_UNKNOWN        105

#define PHYS_REGION_TYPE_REALMODE_IVT        200
#define PHYS_REGION_TYPE_EBDA                201
#define PHYS_REGION_TYPE_VIDEO_MEM           202
#define PHYS_REGION_TYPE_VIDEO_BIOS          203
#define PHYS_REGION_TYPE_BIOS_EXPANSIONS     204
#define PHYS_REGION_TYPE_MOTHERBOARD_BIOS    205

#define PHYS_REGION_TYPE_CPU_LOCAL_APIC      300
#define PHYS_REGION_TYPE_CPU_IO_APIC         301

#define PHYS_REGION_TYPE_KERNEL_IMAGE        400
#define PHYS_REGION_TYPE_KERNEL_SBRK         401
#define PHYS_REGION_TYPE_KERNEL_PAGE_DIR     402
#define PHYS_REGION_TYPE_KERNEL_HEAP         403
#define PHYS_REGION_TYPE_KERNEL_TASK_STACK   404
#define PHYS_REGION_TYPE_KERNEL_USER_STACK   405

#define PHYS_REGION_TYPE_HARDWARE            500

#define PHYS_REGION_TYPE_USER_NULL           600
#define PHYS_REGION_TYPE_USER_STACK          601
#define PHYS_REGION_TYPE_USER_IMAGE          602

typedef struct
{
    ListEntry ListEntry;
    int Type;
    uint64_t Beg;
    uint64_t End;
    uint64_t Size;
    const char* Description;
} PhysRegion;

void PhysInitializeEarly(multiboot_info_t* info);
void PhysInitializeFull();
void PhysDebugDump();

void PhysMark(kphys_t start, size_t pages, int type, const char* description);
kphys_t PhysAlloc(size_t pages, int type, const char* description);
void PhysFree(kphys_t start);

// --------------------------------------------------------------------
// Virtual Memory Manager
// --------------------------------------------------------------------

#define VIRT_PROT_READONLY  (1 << 0)
#define VIRT_PROT_READWRITE (1 << 1)
#define VIRT_PROT_NOCACHE   (1 << 2)

#define VIRT_REGION_TYPE_HARDWARE          0
#define VIRT_REGION_TYPE_ACPI              1
#define VIRT_REGION_TYPE_KERNEL_IMAGE      2
#define VIRT_REGION_TYPE_KERNEL_PAGEDIR    3
#define VIRT_REGION_TYPE_KERNEL_HEAP       4
#define VIRT_REGION_TYPE_KERNEL_TASK_STACK 5
#define VIRT_REGION_TYPE_KERNEL_USER_STACK 6
#define VIRT_REGION_TYPE_FAULT             7
#define VIRT_REGION_TYPE_USER_NULL         10
#define VIRT_REGION_TYPE_USER_STACK        11
#define VIRT_REGION_TYPE_USER_IMAGE        12

#pragma pack(push, 1)
typedef struct
{
    uint32_t Entries[1024];
} PageDirectory;

typedef struct
{
    uint32_t Entries[1024];
} PageTable;
#pragma pack(pop)

typedef struct
{
    ListEntry ListEntry;
    int Protection;
    int Type;
    kphys_t Physical;
    kvirt_t Beg;
    kvirt_t End;
    size_t Size;
    const char* Description;
} VirtRegion;

void VirtInitializeEarly();
void VirtInitializeFull();
void VirtDebugDump();

void VirtMapMemory(kphys_t physical, kvirt_t virtual, size_t pages, int protection, const char* reason);
void VirtUnmapMemory(kvirt_t virtual, size_t pages, const char* reason);

void* PhysToVirt(kphys_t phys);
kphys_t VirtToPhys(void* virt);

void* VirtAlloc(kphys_t physical, size_t pages, int protection, int type, const char* description);
void* VirtAllocUnaligned(kphys_t physical, size_t pages, int protection, int type, const char* description);
void VirtFree(void* virtual);

// --------------------------------------------------------------------
// Virtual address spaces
// --------------------------------------------------------------------

typedef struct
{
    kphys_t PageDirPhys;
    PageDirectory* PageDir;
    PageTable* PageTables;
    ListHead Regions;
    VirtRegion* BeginAlloc;
} VirtSpace;

VirtSpace* VirtSpaceCreate();
void VirtSpaceDestroy(VirtSpace* space);
void VirtSpaceActivate(VirtSpace* space);
void VirtSpaceDebugDump(VirtSpace* space);

void* VirtSpaceMap(VirtSpace* space, kphys_t physical, kvirt_t virtual, size_t pages, int protection, int type, const char* description);
void* VirtSpaceAlloc(VirtSpace* space, kphys_t physical, size_t pages, int protection, int type, const char* description);
void VirtSpaceFree(VirtSpace* space, void* virtual);

// --------------------------------------------------------------------
// Kernel Heap
// --------------------------------------------------------------------

void KHeapInitialize();
void KHeapDebugDump();

void* kalloc(size_t size);
void* kalloc_aligned(size_t size, size_t align);
void* kcalloc(size_t size);
void* kcalloc_aligned(size_t size, size_t align);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);
size_t kmsize(void* ptr);

#endif
