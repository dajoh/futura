#include <string.h>
#include "memory.h"
#include "bitmap.h"
#include "textmode.h"
#include "interrupts.h"

extern Heap* KHeap;
extern const size_t KHeapSize;
extern PageDirectory* VirtPageDirectory;
extern PageTable* VirtPageTables;

extern int __kernel_beg;
extern int __kernel_end;
uint8_t* __kernel_brk = (uint8_t*)&__kernel_end;

static size_t PhysPages = 0;
static uint64_t PhysMaxAddr = 0;
static Bitmap* PhysPageBitmap = NULL;
static kvirt_t PhysMemoryMapAddr = 0;
static size_t PhysMemoryMapSize = 0;

static void PhysMarkBitmap(kphys_t beg, kphys_t end, bool free, const char* description);
static void* k_sbrk(intptr_t inc, size_t align);

static bool PhysFullyInitialized = false;
static ListHead PhysRegionList;
static inline PhysRegion* PhysRegionListFirst();
static inline PhysRegion* PhysRegionListNext(PhysRegion* region);
static inline PhysRegion* PhysRegionListPrev(PhysRegion* region);
static PhysRegion* PhysRegionListInsert(int type, uint64_t beg, uint64_t end, const char* description);
static PhysRegion* PhysRegionListInsert2(int type, uint64_t beg, uint64_t size, const char* description);
static void PhysRegionListCoalesce();

void PhysInitializeEarly(multiboot_info_t* info)
{
    // Copy memory map from BIOS into our sbrk so we still have access to it after we switch to the new page directory
    PhysMemoryMapAddr = (kvirt_t)k_sbrk(info->mmap_length, 1);
    PhysMemoryMapSize = info->mmap_length;
    k_memcpy((void*)PhysMemoryMapAddr, (void*)info->mmap_addr, info->mmap_length);

    // Find max physical address
    kvirt_t mmapPtr = PhysMemoryMapAddr;
    kvirt_t mmapEnd = PhysMemoryMapAddr + PhysMemoryMapSize;
    while (mmapPtr < mmapEnd)
    {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t*)mmapPtr;
        mmapPtr += entry->size + sizeof(uint32_t);

        uint64_t entryEnd = entry->addr + entry->len;
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entryEnd > PhysMaxAddr)
            PhysMaxAddr = entryEnd;
    }

    // Allocate physical page bitmap
    PhysMaxAddr = KPAGE_ALIGN_UP64(PhysMaxAddr);
    PhysPages = PhysMaxAddr / KPAGE_SIZE;
    PhysPageBitmap = k_sbrk(BitmapCalcSize(PhysPages), KPAGE_SIZE);
    BitmapInitialize(PhysPageBitmap, PhysPages);

    // Mark all available regions in memory map
    mmapPtr = PhysMemoryMapAddr;
    mmapEnd = PhysMemoryMapAddr + PhysMemoryMapSize;
    while (mmapPtr < mmapEnd)
    {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t*)mmapPtr;
        mmapPtr += entry->size + sizeof(uint32_t);
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
            PhysMarkBitmap(entry->addr, entry->addr + entry->len, true, "memory map");
    }

    // Mark all unavailable regions in memory map
    mmapPtr = PhysMemoryMapAddr;
    mmapEnd = PhysMemoryMapAddr + PhysMemoryMapSize;
    while (mmapPtr < mmapEnd)
    {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t*)mmapPtr;
        mmapPtr += entry->size + sizeof(uint32_t);
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE)
            PhysMarkBitmap(entry->addr, entry->addr + entry->len, false, "memory map");
    }

    // Mark known unavailable areas, in case they weren't reported by the BIOS
    PhysMarkBitmap(0x00000000, 0x00001000, false, "real-mode IVT/BDA");
    PhysMarkBitmap(0x00080000, 0x000A0000, false, "EBDA");
    PhysMarkBitmap(0x000A0000, 0x000C0000, false, "video memory");
    PhysMarkBitmap(0x000C0000, 0x000C8000, false, "video BIOS");
    PhysMarkBitmap(0x000C8000, 0x000F0000, false, "BIOS expansions");
    PhysMarkBitmap(0x000F0000, 0x00100000, false, "motherboard BIOS");

    // Mark the kernel. This includes our current stack, page tables, and sbrk
    PhysMarkBitmap(KEARLY_VIRT_TO_PHYS(&__kernel_beg), KEARLY_VIRT_TO_PHYS(__kernel_brk), false, "kernel inc. brk");

    // Debug dump
    PhysDebugDump();
}

void PhysInitializeFull()
{
    ListInitialize(&PhysRegionList);

    // Add E820 regions
    kvirt_t mmapPtr = PhysMemoryMapAddr;
    kvirt_t mmapEnd = PhysMemoryMapAddr + PhysMemoryMapSize;
    while (mmapPtr < mmapEnd)
    {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t*)mmapPtr;
        mmapPtr += entry->size + sizeof(uint32_t);

        uint64_t beg = entry->addr;
        uint64_t end = entry->addr + entry->len;
        if (beg == end)
            continue;

        switch (entry->type)
        {
        case MULTIBOOT_MEMORY_AVAILABLE:        PhysRegionListInsert(PHYS_REGION_TYPE_E820_AVAILABLE, beg, end, "free"); break;
        case MULTIBOOT_MEMORY_RESERVED:         PhysRegionListInsert(PHYS_REGION_TYPE_E820_RESERVED, beg, end, "reserved"); break;
        case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: PhysRegionListInsert(PHYS_REGION_TYPE_E820_ACPI, beg, end, "ACPI"); break;
        case MULTIBOOT_MEMORY_NVS:              PhysRegionListInsert(PHYS_REGION_TYPE_E820_NVS, beg, end, "NVS"); break;
        case MULTIBOOT_MEMORY_BADRAM:           PhysRegionListInsert(PHYS_REGION_TYPE_E820_BAD, beg, end, "bad RAM"); break;
        default:                                PhysRegionListInsert(PHYS_REGION_TYPE_E820_UNKNOWN, beg, end, "unknown"); break;
        }
    }

    // Add known hardcoded regions
    PhysRegionListInsert(PHYS_REGION_TYPE_REALMODE_IVT, 0x00000000, 0x00001000, "IVT"); //"real-mode IVT/BDA");
    PhysRegionListInsert(PHYS_REGION_TYPE_EBDA, 0x00080000, 0x000A0000, "EBDA"); //"EBDA");
    PhysRegionListInsert(PHYS_REGION_TYPE_VIDEO_MEM, 0x000A0000, 0x000C0000, "vidmem"); //"video memory");
    PhysRegionListInsert(PHYS_REGION_TYPE_VIDEO_BIOS, 0x000C0000, 0x000C8000, "vidBIOS"); //"video BIOS");
    PhysRegionListInsert(PHYS_REGION_TYPE_BIOS_EXPANSIONS, 0x000C8000, 0x000F0000, "BIOSexp"); //"BIOS expansions");
    PhysRegionListInsert(PHYS_REGION_TYPE_MOTHERBOARD_BIOS, 0x000F0000, 0x00100000, "moboBIOS"); //"motherboard BIOS");
    PhysRegionListInsert(PHYS_REGION_TYPE_KERNEL_IMAGE, KEARLY_VIRT_TO_PHYS(&__kernel_beg), KEARLY_VIRT_TO_PHYS(&__kernel_end), "k.elf"); //"kernel image");
    PhysRegionListInsert(PHYS_REGION_TYPE_KERNEL_SBRK, KEARLY_VIRT_TO_PHYS(&__kernel_end), KEARLY_VIRT_TO_PHYS(__kernel_brk), "k_sbrk"); //"kernel sbrk");
    PhysRegionListInsert2(PHYS_REGION_TYPE_KERNEL_PAGE_DIR, KEARLY_VIRT_TO_PHYS(VirtPageDirectory), 1025 * KPAGE_SIZE, "vpagedir"); //"kernel page dir");
    PhysRegionListInsert2(PHYS_REGION_TYPE_KERNEL_HEAP, KEARLY_VIRT_TO_PHYS(KHeap), KHeapSize, "kheap"); //"kernel heap");

    // Sort and resolve overlaps
    PhysRegionListCoalesce();
    PhysFullyInitialized = true;

    // Debug dump
    PhysDebugDump();
}

void PhysDebugDump()
{
    uint32_t irqLock = IntEnterCriticalSection();

    if (PhysFullyInitialized && !ListIsEmpty(&PhysRegionList))
    {
        TmPrintf("Start        | End          | Status         | Size\n");
        TmPrintf("-------------+--------------+----------------+---------------------------------\n");

        PhysRegion* region = PhysRegionListFirst();
        do
        {
            TmPrintf(
                "%12llX | %12llX | %-8s (%u) | %llu KiB + %llu bytes (%llu MiB)\n",
                region->Beg,
                region->End,
                region->Description, region->Type,
                region->Size / 1024, region->Size % 1024, region->Size / 1048576);
            region = PhysRegionListNext(region);
        } while (region != NULL);

        TmPrintf("\n");
    }

    size_t free = BitmapCountSetBits(PhysPageBitmap);
    size_t used = PhysPages - free;
    TmPrintfDbg("Max physical address:  0x%08llX\n", PhysMaxAddr);
    TmPrintfDbg("Total physical memory: %u MiB (%u KiB, %u pages)\n", (PhysPages*KPAGE_SIZE) / 1048576, (PhysPages*KPAGE_SIZE) / 1024, PhysPages);
    TmPrintfDbg("Total free memory:     %u MiB (%u KiB, %u pages)\n", (free*KPAGE_SIZE) / 1048576, (free*KPAGE_SIZE) / 1024, free);
    TmPrintfDbg("Total used memory:     %u MiB (%u KiB, %u pages)\n", (used*KPAGE_SIZE) / 1048576, (used*KPAGE_SIZE) / 1024, used);

    IntLeaveCriticalSection(irqLock);
}

static void PhysMarkBitmap(kphys_t beg, kphys_t end, bool free, const char* description)
{
    beg = free ? KPAGE_ALIGN_UP(beg) : KPAGE_ALIGN_DOWN(beg);
    end = free ? KPAGE_ALIGN_DOWN(end) : KPAGE_ALIGN_UP(end);

    size_t off = beg / KPAGE_SIZE;
    size_t len = (end - beg) / KPAGE_SIZE;
    if (len > 0)
    {
        if (off >= PhysPageBitmap->Size)
            return;
        if (off + len > PhysPageBitmap->Size)
            len -= (off + len) - PhysPageBitmap->Size;

        if (free)
            TmPrintfVrb("Pfree   %8X to %8X    %-20s [%u MiB, %u KiB]\n", beg, end, description, (len * 4) / 1024, len * 4);
        else
            TmPrintfVrb("Pused   %8X to %8X    %-20s [%u MiB, %u KiB]\n", beg, end, description, (len * 4) / 1024, len * 4);

        BitmapSetBits(PhysPageBitmap, off, len, free);
    }
}

void PhysMark(kphys_t start, size_t pages, int type, const char* description)
{
    DbgAssert(PhysFullyInitialized);
    DbgAssert(pages > 0);
    DbgAssert(start % KPAGE_SIZE == 0);
    DbgAssert(type != PHYS_REGION_TYPE_E820_AVAILABLE);

    uint32_t irqLock = IntEnterCriticalSection();
    {
        kphys_t end = start + pages * KPAGE_SIZE;
        PhysMarkBitmap(start, end, false, description);
        PhysRegionListInsert(type, start, end, description);
        PhysRegionListCoalesce();
    }
    IntLeaveCriticalSection(irqLock);
}

kphys_t PhysAlloc(size_t pages, int type, const char* description)
{
    DbgAssert(pages > 0);

    uint32_t irqLock = IntEnterCriticalSection();
    size_t start = (1024 * 1024) / KPAGE_SIZE;
    size_t off = BitmapFindFirstRegion(PhysPageBitmap, start, pages, true);
    if (off == BITMAP_INVALID_OFFSET)
    {
        IntLeaveCriticalSection(irqLock);
        return 0;
    }

    kphys_t beg = off * KPAGE_SIZE;
    kphys_t end = beg + pages * KPAGE_SIZE;
    PhysMarkBitmap(beg, end, false, description);

    if (PhysFullyInitialized)
    {
        PhysRegionListInsert(type, beg, end, description);
        PhysRegionListCoalesce();
    }

    IntLeaveCriticalSection(irqLock);
    return beg;
}

void PhysFree(kphys_t start)
{
    DbgAssert(PhysFullyInitialized);
    DbgAssert(start % KPAGE_SIZE == 0);

    uint32_t irqLock = IntEnterCriticalSection();
    PhysRegion* region = PhysRegionListFirst();
    while (region)
    {
        if (region->Beg == start)
        {
            DbgAssert(region->Type != PHYS_REGION_TYPE_E820_AVAILABLE);
            region->Type = PHYS_REGION_TYPE_E820_AVAILABLE;
            region->Description = "free";
            PhysRegionListCoalesce();
            PhysMarkBitmap(region->Beg, region->End, true, "free");
            IntLeaveCriticalSection(irqLock);
            return;
        }
        region = PhysRegionListNext(region);
    }

    DbgPanic("PhysFree called with invalid address");
}

void* k_sbrk(intptr_t inc, size_t align)
{
    if (align > 1 && inc != 0)
    {
        uintptr_t ptr = (uintptr_t)__kernel_brk;
        uintptr_t rem = ptr % align;
        if (rem != 0)
            __kernel_brk += align - rem;
    }

    void* ret = __kernel_brk;
    __kernel_brk += inc;
    return ret;
}

// --------------------------------------------------------------------------------
// Full implementation
// --------------------------------------------------------------------------------

static inline bool PhysRegionOverlaps(PhysRegion* a, PhysRegion* b)
{
    return a->Beg < b->End && b->Beg < a->End;
}

static inline bool PhysRegionContains(PhysRegion* a, PhysRegion* b)
{
    return b->Beg >= a->Beg && b->Beg < a->End &&
           b->End > a->Beg && b->End <= a->End;
}

static inline PhysRegion* PhysRegionListFirst()
{
    return ListIsEmpty(&PhysRegionList) ? NULL : CONTAINING_RECORD(PhysRegionList.Next, PhysRegion, ListEntry);
}

static inline PhysRegion* PhysRegionListNext(PhysRegion* region)
{
    return region->ListEntry.Next == &PhysRegionList ? NULL : CONTAINING_RECORD(region->ListEntry.Next, PhysRegion, ListEntry);
}

static inline PhysRegion* PhysRegionListPrev(PhysRegion* region)
{
    return region->ListEntry.Prev == &PhysRegionList ? NULL : CONTAINING_RECORD(region->ListEntry.Prev, PhysRegion, ListEntry);
}

static void PhysRegionListInsertSorted(PhysRegion* newRegion)
{
    PhysRegion* region = PhysRegionListFirst();
    while (region)
    {
        if (newRegion->Beg <= region->Beg)
        {
            ListInsertBefore(&region->ListEntry, &newRegion->ListEntry);
            return;
        }
        region = PhysRegionListNext(region);
    }

    ListPushBack(&PhysRegionList, &newRegion->ListEntry);
}

static PhysRegion* PhysRegionListInsert(int type, uint64_t beg, uint64_t end, const char* description)
{
    DbgAssert(end > beg);
    TmPrintfVrb("Pregion  %9llX to %9llX added: %s\n", beg, end, description);
    PhysRegion* newRegion = kalloc(sizeof(PhysRegion));
    newRegion->Type = type;
    newRegion->Beg = beg;
    newRegion->End = end;
    newRegion->Size = end - beg;
    newRegion->Description = description;
    PhysRegionListInsertSorted(newRegion);
    return newRegion;
}

static PhysRegion* PhysRegionListInsert2(int type, uint64_t beg, uint64_t size, const char* description)
{
    return PhysRegionListInsert(type, beg, beg + size, description);
}

/*static void PhysRegionDebugDump(PhysRegion* region)
{
    TmPrintf("{%llX to %llX, %s (%d)}\n", region->Beg, region->End, region->Description, region->Type);
}*/

static void PhysRegionListCoalesce()
{
    if (ListIsEmpty(&PhysRegionList))
        return;

    PhysRegion* prev = PhysRegionListFirst();
    PhysRegion* curr = PhysRegionListNext(prev);
    while (curr)
    {
        //TmPrintf("\nComparing:  ");
        //PhysRegionDebugDump(prev);
        //TmPrintf("With:       ");
        //PhysRegionDebugDump(curr);

        if (PhysRegionOverlaps(prev, curr))
        {
            if (prev->Type == curr->Type)
            {
                //TmPrintf("OVERLAP! same type, merging:\n");
                curr->Beg = prev->Beg < curr->Beg ? prev->Beg : curr->Beg;
                curr->End = prev->End > curr->End ? prev->End : curr->End;
                curr->Size = curr->End - curr->Beg;
                ListRemove(&prev->ListEntry);
                kfree(prev);
            }
            else
            {
                PhysRegion* winner = prev->Type > curr->Type ? prev : curr;
                PhysRegion* loser = prev->Type > curr->Type ? curr : prev;

                if (PhysRegionContains(winner, loser))
                {
                    //TmPrintf("OVERLAP! different types, type %d wins and contains loser\n\n", winner->Type);
                    ListRemove(&loser->ListEntry);
                    kfree(loser);

                    prev = winner;
                    curr = PhysRegionListNext(winner);
                    continue;
                }

                if (PhysRegionContains(loser, winner))
                {
                    //TmPrintf("OVERLAP! different types, type %d wins and is contained by loser\n\n", winner->Type);

                    if (loser->Beg == winner->Beg)
                    {
                        // adjust loser
                        loser->Beg = winner->End;
                        loser->Size = loser->End - loser->Beg;

                        // might need reordering..
                        ListRemove(&loser->ListEntry);
                        PhysRegionListInsertSorted(loser);

                        // iterate
                        prev = loser;
                        curr = PhysRegionListNext(loser);
                        continue;
                    }

                    // create new block for end if needed
                    uint64_t splitBeg = winner->End;
                    uint64_t splitEnd = loser->End;
                    if (splitBeg != splitEnd)
                        PhysRegionListInsert(loser->Type, splitBeg, splitEnd, loser->Description);

                    // adjust loser
                    loser->End = winner->Beg;
                    loser->Size = loser->End - loser->Beg;

                    // iterate
                    prev = winner;
                    curr = PhysRegionListNext(winner);
                    continue;
                }

                if (winner->Beg < loser->Beg)
                {
                    //TmPrintf("OVERLAP! different types, type %d wins and cuts into loser start\n\n", winner->Type);
                    loser->Beg = winner->End;
                    loser->Size = loser->End - loser->Beg;
                }
                else
                {
                    //TmPrintf("OVERLAP! different types, type %d wins and cuts into loser end\n\n", winner->Type);
                    loser->End = winner->Beg;
                    loser->Size = loser->End - loser->Beg;
                }
            }
        }
        else
        {
            //TmPrintf("no overlap, doing nothing\n");
        }

        prev = curr;
        curr = PhysRegionListNext(curr);
    }
}
