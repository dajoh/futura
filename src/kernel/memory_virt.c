#include <string.h>
#include "debug.h"
#include "memory.h"
#include "lowlevel.h"
#include "textmode.h"
#include "interrupts.h"

#define PD_FLAG_PRESENT      (1 << 0)
#define PD_FLAG_READWRITE    (1 << 1)
#define PD_FLAG_USERSPACE    (1 << 2)
#define PD_FLAG_WRITETHRU    (1 << 3)
#define PD_FLAG_CACHEDISABLE (1 << 4)
#define PD_FLAG_ACCESSED     (1 << 5)
#define PD_FLAG_SIZELARGE    (1 << 7)

#define PT_FLAG_PRESENT      (1 << 0)
#define PT_FLAG_READWRITE    (1 << 1)
#define PT_FLAG_USERSPACE    (1 << 2)
#define PT_FLAG_WRITETHRU    (1 << 3)
#define PT_FLAG_CACHEDISABLE (1 << 4)
#define PT_FLAG_ACCESSED     (1 << 5)
#define PT_FLAG_DIRTY        (1 << 7)
#define PT_FLAG_GLOBAL       (1 << 9)

extern int __kernel_beg;
extern uint8_t* __kernel_brk;
extern Heap* KHeap;
extern const size_t KHeapSize;

PageDirectory* VirtPageDirectory;
PageTable* VirtPageTables;
ListHead VirtRegions;

static bool VirtFullyInitialized = false;
static VirtRegion* VirtBeginAlloc;
static inline VirtRegion* VirtRegionListFirst();
static inline VirtRegion* VirtRegionListNext(VirtRegion* region);

void VirtInitializeEarly()
{
    VirtPageDirectory = (PageDirectory*)KEARLY_PHYS_TO_VIRT(PhysAlloc(1025, PHYS_REGION_TYPE_KERNEL_PAGE_DIR, "vmm tables"));
    VirtPageTables = (PageTable*)(VirtPageDirectory + 1);

    if (KVIRT(VirtPageDirectory+1025) > 0xC0C00000)
        // you can increase this limit by adding more early page tables
        DbgPanic("new page tables not completely mapped by early page tables");

    for (size_t i = 0; i < 1024; i++)
    {
        PageTable* table = VirtPageTables + i;
        VirtPageDirectory->Entries[i] = KEARLY_VIRT_TO_PHYS(table) | PD_FLAG_READWRITE | PD_FLAG_PRESENT;
        memset(table, 0, sizeof(PageTable));
    }

    kvirt_t kernelBeg = KVIRT(&__kernel_beg);
    kvirt_t kernelBrk = KVIRT(__kernel_brk);
    size_t kernelPages = KPAGE_COUNT(kernelBrk - kernelBeg);

    VirtMapMemory(0x000A0000, 0xC00A0000, 32, VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, "video memory");
    VirtMapMemory(KEARLY_VIRT_TO_PHYS(kernelBeg), kernelBeg, kernelPages, VIRT_PROT_READWRITE, "kernel");
    VirtMapMemory(KEARLY_VIRT_TO_PHYS(VirtPageDirectory), KVIRT(VirtPageDirectory), 1025, VIRT_PROT_READWRITE, "vmm tables");
    pg_setdir(KEARLY_VIRT_TO_PHYS(VirtPageDirectory));
}

void VirtDebugDump()
{
    uint32_t irqLock = IntEnterCriticalSection();

    if (!ListIsEmpty(&VirtRegions))
    {
        TmPrintf("Physical | Virtual  | End      | Size        | Description\n");
        TmPrintf("---------+----------+----------+-------------+-------------------------------\n");

        VirtRegion* region = VirtRegionListFirst();
        do
        {
            TmPrintf(
                "%8X | %8X | %8X | %7u KiB | %s (%d)\n",
                region->Physical,
                region->Beg,
                region->End,
                region->Size / 1024,
                region->Description,
                region->Type);
            region = VirtRegionListNext(region);
        } while (region != NULL);

        TmPrintf("\n");
    }

    IntLeaveCriticalSection(irqLock);
}

void VirtMapMemory(kphys_t physical, kvirt_t virtual, size_t pages, int protection, const char* reason)
{
    kphys_t phys = physical;
    kvirt_t virt = virtual;
    uint32_t flags = 0;
    if (protection & VIRT_PROT_READONLY)
        flags |= PT_FLAG_PRESENT;
    if (protection & VIRT_PROT_READWRITE)
        flags |= PT_FLAG_PRESENT | PT_FLAG_READWRITE;
    if (protection & VIRT_PROT_NOCACHE)
        flags |= PT_FLAG_CACHEDISABLE;

    for (size_t i = 0; i < pages; i++)
    {
        size_t pdIdx = virt >> 22;
        size_t ptIdx = (virt >> 12) & 0x3FF;
        PageTable* table = VirtPageTables + pdIdx;
        table->Entries[ptIdx] = phys | flags;
        pg_flushtlb(virt);
        phys += KPAGE_SIZE;
        virt += KPAGE_SIZE;
    }
    TmPrintfVrb("Vmap    %8X to %8X    %-20s [%u MiB, %u KiB]\n", physical, virtual, reason, (pages * 4) / 1024, pages * 4);
}

void VirtUnmapMemory(kvirt_t virtual, size_t pages, const char* reason)
{
    kphys_t phys = 0;
    kvirt_t virt = virtual;

    for (size_t i = 0; i < pages; i++)
    {
        size_t pdIdx = virt >> 22;
        size_t ptIdx = (virt >> 12) & 0x3FF;
        PageTable* table = VirtPageTables + pdIdx;
        if (i == 0) phys = table->Entries[ptIdx] & 0xFFFFF000;
        table->Entries[ptIdx] = 0;
        pg_flushtlb(virt);
        virt += KPAGE_SIZE;
    }
    TmPrintfVrb("Vunmap  %8X to %8X    %-20s [%u MiB, %u KiB]\n", phys, virtual, reason, (pages * 4) / 1024, pages * 4);
}

// --------------------------------------------------------------------------------
// Full implementation
// --------------------------------------------------------------------------------

static inline bool VirtRegionOverlaps(VirtRegion* a, VirtRegion* b)
{
    return a->Beg < b->End && b->Beg < a->End;
}

static inline bool VirtRegionContainsVirt(VirtRegion* r, kvirt_t addr)
{
    return addr >= r->Beg && addr < r->End;
}

static inline bool VirtRegionContainsPhys(VirtRegion* r, kphys_t addr)
{
    return addr >= r->Physical && addr < (r->Physical + r->Size);
}

static inline VirtRegion* VirtRegionListFirst()
{
    return ListIsEmpty(&VirtRegions) ? NULL : CONTAINING_RECORD(VirtRegions.Next, VirtRegion, ListEntry);
}

static inline VirtRegion* VirtRegionListNext(VirtRegion* region)
{
    return region->ListEntry.Next == &VirtRegions ? NULL : CONTAINING_RECORD(region->ListEntry.Next, VirtRegion, ListEntry);
}

static void VirtRegionInsert(VirtRegion* entry)
{
    VirtRegion* region = VirtRegionListFirst();
    while (region)
    {
        if (entry->Beg <= region->Beg)
        {
            ListInsertBefore(&region->ListEntry, &entry->ListEntry);
            return;
        }
        region = VirtRegionListNext(region);
    }

    ListPushBack(&VirtRegions, &entry->ListEntry);
}

static VirtRegion* VirtRegionCreate(kphys_t phys, kvirt_t virt, size_t pages, int protection, int type, const char* description)
{
    VirtRegion* region = kalloc(sizeof(VirtRegion));
    region->Protection = protection;
    region->Type = type;
    region->Physical = phys;
    region->Beg = virt;
    region->End = virt + pages * KPAGE_SIZE;
    region->Size = pages * KPAGE_SIZE;
    region->Description = description;
    return region;
}

void VirtInitializeFull()
{
    ListInitialize(&VirtRegions);

    kvirt_t kernelBeg = KVIRT(&__kernel_beg);
    kvirt_t kernelBrk = KVIRT(__kernel_brk);
    size_t kernelPages = KPAGE_COUNT(kernelBrk - kernelBeg);

    VirtRegion* vga = VirtRegionCreate(
        0x000A0000,
        0xC00A0000,
        32,
        VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE,
        VIRT_REGION_TYPE_HARDWARE,
        "VGA RAM");
    VirtRegionInsert(vga);

    VirtRegion* kernel = VirtRegionCreate(
        KEARLY_VIRT_TO_PHYS(kernelBeg),
        kernelBeg,
        kernelPages,
        VIRT_PROT_READWRITE,
        VIRT_REGION_TYPE_KERNEL_IMAGE,
        "kernel");
    VirtRegionInsert(kernel);

    VirtRegion* pageTables = VirtRegionCreate(
        KEARLY_VIRT_TO_PHYS(VirtPageDirectory),
        KVIRT(VirtPageDirectory),
        1025,
        VIRT_PROT_READWRITE,
        VIRT_REGION_TYPE_KERNEL_PAGEDIR,
        "kernel pagedir");
    VirtRegionInsert(pageTables);

    VirtRegion* kernelHeap = VirtRegionCreate(
        KEARLY_VIRT_TO_PHYS(KHeap),
        KVIRT(KHeap),
        KPAGE_COUNT(KHeapSize),
        VIRT_PROT_READWRITE,
        VIRT_REGION_TYPE_KERNEL_HEAP,
        "kernel heap");
    VirtRegionInsert(kernelHeap);

    VirtBeginAlloc = kernelHeap;
    VirtFullyInitialized = true;

    VirtDebugDump();
}

void* PhysToVirt(kphys_t addr)
{
    uint32_t irqLock = IntEnterCriticalSection();
    VirtRegion* region = VirtRegionListFirst();
    while (region)
    {
        if (VirtRegionContainsPhys(region, addr))
        {
            size_t offset = addr - region->Physical;
            void* result = (void*)(region->Beg + offset);
            IntLeaveCriticalSection(irqLock);
            return result;
        }
        region = VirtRegionListNext(region);
    }
    IntLeaveCriticalSection(irqLock);
    return NULL;
}

kphys_t VirtToPhys(void* virt)
{
    uint32_t irqLock = IntEnterCriticalSection();
    kvirt_t addr = KVIRT(virt);
    VirtRegion* region = VirtRegionListFirst();
    while (region)
    {
        if (VirtRegionContainsVirt(region, addr))
        {
            size_t offset = addr - region->Beg;
            kphys_t result = region->Physical + offset;
            IntLeaveCriticalSection(irqLock);
            return result;
        }
        region = VirtRegionListNext(region);
    }
    IntLeaveCriticalSection(irqLock);
    return 0;
}

void* VirtAlloc(kphys_t physical, size_t pages, int protection, int type, const char* description)
{
    DbgAssert(physical % KPAGE_SIZE == 0);
    DbgAssert(pages > 0);

    uint32_t irqLock = IntEnterCriticalSection();
    VirtRegion* newRegion = VirtRegionCreate(physical, 0, pages, protection, type, description);
    VirtRegion* region = VirtBeginAlloc;
    while (region)
    {
        VirtRegion* nextRegion = VirtRegionListNext(region);

        newRegion->Beg = region->End;
        newRegion->End = region->End + newRegion->Size;

        if (nextRegion && VirtRegionOverlaps(newRegion, nextRegion))
        {
            region = nextRegion;
            continue;
        }

        VirtMapMemory(physical, newRegion->Beg, pages, protection, description);
        ListInsertAfter(&region->ListEntry, &newRegion->ListEntry);
        IntLeaveCriticalSection(irqLock);
        return (void*)newRegion->Beg;
    }

    IntLeaveCriticalSection(irqLock);
    return NULL;
}

void* VirtAllocUnaligned(kphys_t physical, size_t pages, int protection, int type, const char* description)
{
    DbgAssert(pages > 0);
    kphys_t physPage = KPAGE_ALIGN_DOWN(physical);
    size_t offset = physical - physPage;
    uint8_t* virt = VirtAlloc(physPage, pages, protection, type, description);
    DbgAssert(virt != NULL);
    return virt + offset;
}

void VirtFree(void* virtual)
{
    uint32_t irqLock = IntEnterCriticalSection();
    kvirt_t addr = KVIRT(virtual);
    VirtRegion* region = VirtRegionListFirst();
    while (region)
    {
        if (VirtRegionContainsVirt(region, addr))
        {
            VirtUnmapMemory(region->Beg, region->Size / KPAGE_SIZE, region->Description);
            ListRemove(&region->ListEntry);
            kfree(region);
            IntLeaveCriticalSection(irqLock);
            return;
        }
        region = VirtRegionListNext(region);
    }
    DbgPanic("VmmExFree failed, couldn't find virtual memory region");
}
