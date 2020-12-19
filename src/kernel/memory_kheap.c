#include <string.h>
#include "debug.h"
#include "memory.h"
#include "interrupts.h"

Heap* KHeap = NULL;
const size_t KHeapSize = 16*1024*1024;

void KHeapInitialize()
{
    kphys_t phys = PhysAlloc(KPAGE_COUNT(KHeapSize), PHYS_REGION_TYPE_KERNEL_HEAP, "kheap");
    kvirt_t virt = KEARLY_PHYS_TO_VIRT(phys);
    DbgAssert(phys != 0);
    VirtMapMemory(phys, virt, KPAGE_COUNT(KHeapSize), VIRT_PROT_READWRITE, "kheap");
    KHeap = HeapInitialize((void*)virt, KHeapSize);
}

void KHeapDebugDump()
{
    uint32_t irqLock = IntEnterCriticalSection();
    HeapDebugDump(KHeap);
    IntLeaveCriticalSection(irqLock);
}

void* internal_kalloc(size_t size)
{
    uint32_t irqLock = IntEnterCriticalSection();
    void* ret = HeapAlloc(KHeap, size);
    IntLeaveCriticalSection(irqLock);
    return ret;
}

void* internal_kalloc_aligned(size_t size, size_t align)
{
    void* mem = kalloc(size + align - 1);
    uintptr_t addr = (uintptr_t)mem;
    if (addr % align != 0)
        addr += align - (addr % align);
    return (void*)addr;
}

void* internal_kcalloc(size_t size)
{
    void* mem = kalloc(size);
    if (mem)
        k_memset(mem, 0, size);
    return mem;
}

void* internal_kcalloc_aligned(size_t size, size_t align)
{
    void* mem = kalloc_aligned(size, align);
    if (mem)
        k_memset(mem, 0, size);
    return mem;
}

void* internal_krealloc(void* ptr, size_t size)
{
    uint32_t irqLock = IntEnterCriticalSection();
    void* ret = HeapRealloc(KHeap, ptr, size);
    IntLeaveCriticalSection(irqLock);
    return ret;
}

void internal_kfree(void* ptr)
{
    uint32_t irqLock = IntEnterCriticalSection();
    HeapFree(KHeap, ptr);
    IntLeaveCriticalSection(irqLock);
}

size_t internal_kmsize(void* ptr)
{
    uint32_t irqLock = IntEnterCriticalSection();
    size_t ret = HeapMSize(KHeap, ptr);
    IntLeaveCriticalSection(irqLock);
    return ret;
}

#ifdef KERNEL_RELEASE_HEAPALLOC
void* kalloc(size_t size)
{
    return internal_kalloc(size);
}

void* kalloc_aligned(size_t size, size_t align)
{
    return internal_kalloc_aligned(size, align);
}

void* kcalloc(size_t size)
{
    return internal_kcalloc(size);
}

void* kcalloc_aligned(size_t size, size_t align)
{
    return internal_kcalloc_aligned(size, align);
}

void* krealloc(void* ptr, size_t size)
{
    return internal_krealloc(ptr, size);
}

void kfree(void* ptr)
{
    internal_kfree(ptr);
}

size_t kmsize(void* ptr)
{
    return internal_kmsize(ptr);
}
#else

#define KHEAP_DBG_INIT_BYTE        0xDB
#define KHEAP_DBG_GUARD_BYTE       0xC4
#define KHEAP_DBG_GUARD_BYTES      32
#define KHEAP_DBG_HALF_GUARD_BYTES (KHEAP_DBG_GUARD_BYTES/2)

void* kalloc(size_t size)
{
    uint8_t* guard1 = internal_kalloc(size + KHEAP_DBG_GUARD_BYTES);
    uint8_t* center = guard1 + KHEAP_DBG_HALF_GUARD_BYTES;
    uint8_t* guard2 = center + size;
    k_memset(guard1, KHEAP_DBG_GUARD_BYTE, KHEAP_DBG_HALF_GUARD_BYTES);
    k_memset(center, KHEAP_DBG_INIT_BYTE, size);
    k_memset(guard2, KHEAP_DBG_GUARD_BYTE, KHEAP_DBG_HALF_GUARD_BYTES);
    return center;
}

void* kalloc_aligned(size_t size, size_t align)
{
    DbgPanic("dbg kalloc_aligned not implemented");
    return internal_kalloc_aligned;
}

void* kcalloc(size_t size)
{
    uint8_t* guard1 = internal_kalloc(size + KHEAP_DBG_GUARD_BYTES);
    uint8_t* center = guard1 + KHEAP_DBG_HALF_GUARD_BYTES;
    uint8_t* guard2 = center + size;
    k_memset(guard1, KHEAP_DBG_GUARD_BYTE, KHEAP_DBG_HALF_GUARD_BYTES);
    k_memset(center, 0, size);
    k_memset(guard2, KHEAP_DBG_GUARD_BYTE, KHEAP_DBG_HALF_GUARD_BYTES);
    return center;
}

void* kcalloc_aligned(size_t size, size_t align)
{
    DbgPanic("dbg kcalloc_aligned not implemented");
    return internal_kcalloc_aligned(size, align);
}

void* krealloc(void* ptr, size_t size)
{
    DbgPanic("dbg krealloc not implemented");
    return internal_krealloc(ptr, size);
}

void kfree(void* ptr)
{
    uint8_t* guard1 = (uint8_t*)ptr - KHEAP_DBG_HALF_GUARD_BYTES;
    for (size_t i = 0; i < KHEAP_DBG_HALF_GUARD_BYTES; i++) if (guard1[i] != KHEAP_DBG_GUARD_BYTE) { DbgHexdump(guard1 - 16, 256); DbgPanic("kheap corrupted"); }
    size_t size = internal_kmsize(guard1) - KHEAP_DBG_GUARD_BYTES;
    uint8_t* guard2 = guard1 + KHEAP_DBG_HALF_GUARD_BYTES + size;
    for (size_t i = 0; i < KHEAP_DBG_HALF_GUARD_BYTES; i++) if (guard2[i] != KHEAP_DBG_GUARD_BYTE) { DbgHexdump(guard1 - 16, 256); DbgPanic("kheap corrupted"); }
    internal_kfree(guard1);
}

size_t kmsize(void* ptr)
{
    uint8_t* guard1 = (uint8_t*)ptr - KHEAP_DBG_HALF_GUARD_BYTES;
    for (size_t i = 0; i < KHEAP_DBG_HALF_GUARD_BYTES; i++) if (guard1[i] != KHEAP_DBG_GUARD_BYTE) { DbgHexdump(guard1 - 16, 256); DbgPanic("kheap corrupted"); }
    size_t size = internal_kmsize(guard1) - KHEAP_DBG_GUARD_BYTES;
    uint8_t* guard2 = guard1 + KHEAP_DBG_HALF_GUARD_BYTES + size;
    for (size_t i = 0; i < KHEAP_DBG_HALF_GUARD_BYTES; i++) if (guard2[i] != KHEAP_DBG_GUARD_BYTE) { DbgHexdump(guard1 - 16, 256); DbgPanic("kheap corrupted"); }
    return size;
}
#endif
