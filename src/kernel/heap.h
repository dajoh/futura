#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H

#include <stddef.h>

typedef struct Heap_s Heap;

Heap* HeapInitialize(void* mem, size_t size);
void HeapDebugDump(Heap* heap);

void* HeapAlloc(Heap* heap, size_t size);
void* HeapRealloc(Heap* heap, void* ptr, size_t size);
void HeapFree(Heap* heap, void* ptr);
size_t HeapMSize(Heap* heap, void* ptr);

#endif
