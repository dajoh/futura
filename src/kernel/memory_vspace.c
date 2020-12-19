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

extern PageDirectory* VirtPageDirectory;
extern PageTable* VirtPageTables;
extern ListHead VirtRegions;

VirtSpace* VirtSpaceActive = NULL;




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

static inline VirtRegion* VirtSpaceFirstRegion(VirtSpace* space)
{
    return ListIsEmpty(&space->Regions) ? NULL : CONTAINING_RECORD(space->Regions.Next, VirtRegion, ListEntry);
}

static inline VirtRegion* VirtSpaceNextRegion(VirtSpace* space, VirtRegion* region)
{
    return region->ListEntry.Next == &space->Regions ? NULL : CONTAINING_RECORD(region->ListEntry.Next, VirtRegion, ListEntry);
}

static void VirtSpaceInsertRegion(VirtSpace* space, VirtRegion* entry)
{
    VirtRegion* region = VirtSpaceFirstRegion(space);
    while (region)
    {
        if (entry->Beg <= region->Beg)
        {
            ListInsertBefore(&region->ListEntry, &entry->ListEntry);
            return;
        }
        region = VirtSpaceNextRegion(space, region);
    }

    ListPushBack(&space->Regions, &entry->ListEntry);
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

static void VirtSpaceMapMemory(VirtSpace* space, kphys_t physical, kvirt_t virtual, size_t pages, int protection, const char* reason)
{
    DbgAssert(virtual < 0xC0000000);
    DbgAssert(virtual + pages * KPAGE_SIZE <= 0xC0000000);

    kphys_t phys = physical;
    kvirt_t virt = virtual;
    uint32_t flags = PT_FLAG_USERSPACE; // TODO: don't set userspace flag for kernel pages
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
        PageTable* table = space->PageTables + pdIdx;
        table->Entries[ptIdx] = phys | flags;

        if (VirtSpaceActive == space)
            pg_flushtlb(virt);

        phys += KPAGE_SIZE;
        virt += KPAGE_SIZE;
    }

    TmPrintfVrb("VSmap   %8X to %8X    %-20s [%u MiB, %u KiB]\n", physical, virtual, reason, (pages * 4) / 1024, pages * 4);
}




VirtSpace* VirtSpaceCreate()
{
    kphys_t pageDirPhys = PhysAlloc(1025, PHYS_REGION_TYPE_KERNEL_PAGE_DIR, "upagedir");
    DbgAssert(pageDirPhys != 0);

    VirtSpace* space = kalloc(sizeof(VirtSpace));
    space->PageDirPhys = pageDirPhys;
    space->PageDir = VirtAlloc(pageDirPhys, 1025, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_KERNEL_PAGEDIR, "upagedir");
    space->PageTables = (PageTable*)(space->PageDir + 1);
    ListInitialize(&space->Regions);
    VirtSpaceInsertRegion(space, VirtRegionCreate(0, 0x00000000, 1, 0, VIRT_REGION_TYPE_USER_NULL, "u.null"));
    VirtSpaceInsertRegion(space, VirtRegionCreate(0, 0x001FF000, 1, 0, VIRT_REGION_TYPE_USER_NULL, "u.alloc"));
    space->BeginAlloc = CONTAINING_RECORD(space->Regions.Prev, VirtRegion, ListEntry);

    kphys_t pageTablePhys = pageDirPhys + sizeof(PageDirectory);
    for (size_t i = 0; i < 768; i++)
    {
        PageTable* table = space->PageTables + i;
        memset(table, 0, sizeof(PageTable));
        space->PageDir->Entries[i] = pageTablePhys | PD_FLAG_READWRITE | PD_FLAG_USERSPACE | PD_FLAG_PRESENT; // TODO: don't set userspace flag for kernel pages
        pageTablePhys += KPAGE_SIZE;
    }

    for (size_t i = 768; i < 1024; i++)
    {
        PageTable* table = VirtPageTables + i;
        space->PageDir->Entries[i] = KEARLY_VIRT_TO_PHYS(table) | PD_FLAG_READWRITE | PD_FLAG_USERSPACE | PD_FLAG_PRESENT; // TODO: don't set userspace flag for kernel pages
    }

    return space;
}

void VirtSpaceDestroy(VirtSpace* space)
{
    DbgAssert(VirtSpaceActive != space);
    PhysFree(VirtToPhys(space->PageDir));
    VirtFree(space->PageDir);
    kfree(space);
}

void VirtSpaceActivate(VirtSpace* space)
{
    uint32_t irqLock = IntEnterCriticalSection();
    if (VirtSpaceActive != space)
    {
        VirtSpaceActive = space;
        pg_setdir(space ? space->PageDirPhys : KEARLY_VIRT_TO_PHYS(VirtPageDirectory));
    }
    IntLeaveCriticalSection(irqLock);
}

void VirtSpaceDebugDump(VirtSpace* space)
{
    uint32_t irqLock = IntEnterCriticalSection();
    TmPrintf("Physical | Virtual  | End      | Size        | Description\n");
    TmPrintf("---------+----------+----------+-------------+-------------------------------\n");

    VirtRegion* region = VirtSpaceFirstRegion(space);
    while (region)
    {
        TmPrintf(
            "%8X | %8X | %8X | %7u KiB | U  %s (%d)\n",
            region->Physical,
            region->Beg,
            region->End,
            region->Size / 1024,
            region->Description,
            region->Type);
        region = VirtSpaceNextRegion(space, region);
    }

    ListEntry* entry = VirtRegions.Next;
    while (entry != &VirtRegions)
    {
        region = CONTAINING_RECORD(entry, VirtRegion, ListEntry);
        TmPrintf(
            "%8X | %8X | %8X | %7u KiB | K  %s (%d)\n",
            region->Physical,
            region->Beg,
            region->End,
            region->Size / 1024,
            region->Description,
            region->Type);
        entry = entry->Next;
    }
    
    TmPrintf("\n");
    IntLeaveCriticalSection(irqLock);
}

void* VirtSpaceMap(VirtSpace* space, kphys_t physical, kvirt_t virtual, size_t pages, int protection, int type, const char* description)
{
    DbgAssert(physical % KPAGE_SIZE == 0);
    DbgAssert(virtual % KPAGE_SIZE == 0);
    DbgAssert(pages > 0);

    uint32_t irqLock = IntEnterCriticalSection();
    VirtRegion* newRegion = VirtRegionCreate(physical, virtual, pages, protection, type, description);
    VirtRegion* region = VirtSpaceFirstRegion(space);
    while (region)
    {
        DbgAssert(!VirtRegionOverlaps(region, newRegion));
        region = VirtSpaceNextRegion(space, region);
    }

    VirtSpaceMapMemory(space, physical, virtual, pages, protection, description);
    VirtSpaceInsertRegion(space, newRegion);
    IntLeaveCriticalSection(irqLock);
    return (void*)virtual;
}

void* VirtSpaceAlloc(VirtSpace* space, kphys_t physical, size_t pages, int protection, int type, const char* description)
{
    DbgAssert(physical % KPAGE_SIZE == 0);
    DbgAssert(pages > 0);

    uint32_t irqLock = IntEnterCriticalSection();
    VirtRegion* newRegion = VirtRegionCreate(physical, 0, pages, protection, type, description);
    VirtRegion* region = space->BeginAlloc;
    while (region)
    {
        VirtRegion* nextRegion = VirtSpaceNextRegion(space, region);

        newRegion->Beg = region->End;
        newRegion->End = region->End + newRegion->Size;

        if (nextRegion && VirtRegionOverlaps(newRegion, nextRegion))
        {
            region = nextRegion;
            continue;
        }

        VirtSpaceMapMemory(space, physical, newRegion->Beg, pages, protection, description);
        ListInsertAfter(&region->ListEntry, &newRegion->ListEntry);
        IntLeaveCriticalSection(irqLock);
        return (void*)newRegion->Beg;
    }

    IntLeaveCriticalSection(irqLock);
    return NULL;
}

void VirtSpaceFree(VirtSpace* space, void* virtual)
{
    DbgPanic("not implemented");
}
