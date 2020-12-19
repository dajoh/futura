#include <stdint.h>
#include <string.h>
#include "heap.h"
#include "debug.h"
#include "textmode.h"

#define HEAP_MIN_ALLOC       (sizeof(HeapFreeBlock) - sizeof(HeapBlock))
#define HEAP_SIG_HEAP_HEADER "HEAP"
#define HEAP_SIG_USED_BLOCK  "USED"
#define HEAP_SIG_FREE_BLOCK  "FREE"

#pragma pack(push, 1)
typedef struct HeapBlock_s
{
	size_t Size;
	char Signature[4];
} HeapBlock;

typedef struct HeapFreeBlock_s
{
	size_t Size;
	char Signature[4];
	struct HeapFreeBlock_s* Next;
	struct HeapFreeBlock_s* Prev;
} HeapFreeBlock;

typedef struct Heap_s
{
	char Signature[4];
	size_t Size;
	size_t UsedBlocks;
	size_t FreeBlocks;
	size_t BytesAllocated;
	size_t BytesAvailable;
	size_t BytesOverhead;
	HeapFreeBlock* FirstFreeBlock;
	HeapFreeBlock* LastFreeBlock;
} Heap;
#pragma pack(pop)

static void HeapCoalesce(Heap* heap, HeapFreeBlock* first, HeapFreeBlock* second)
{
	HeapFreeBlock* firstEnd = (HeapFreeBlock*)((uint8_t*)first + first->Size + sizeof(HeapBlock));
	if (firstEnd == second)
	{
		// Update heap stats
		heap->FreeBlocks--;
		heap->BytesAvailable += sizeof(HeapBlock);
		heap->BytesOverhead -= sizeof(HeapBlock);

		// Merge blocks
		first->Size += second->Size + sizeof(HeapBlock);
		first->Next = second->Next;
		if (first->Next)
			first->Next->Prev = first;
		else
			heap->LastFreeBlock = first;
	}
}

Heap* HeapInitialize(void* mem, size_t size)
{
	Heap* heap = (Heap*)mem;
	HeapFreeBlock* block = (HeapFreeBlock*)(heap + 1);

	// Setup first free block
	block->Size = size - sizeof(Heap) - sizeof(HeapBlock);
	k_memcpy(block->Signature, HEAP_SIG_FREE_BLOCK, sizeof(block->Signature));
	block->Next = NULL;
	block->Prev = NULL;

	// Setup heap header
	k_memcpy(heap->Signature, HEAP_SIG_HEAP_HEADER, sizeof(heap->Signature));
	heap->Size = size;
	heap->UsedBlocks = 0;
	heap->FreeBlocks = 1;
	heap->BytesAllocated = 0;
	heap->BytesAvailable = block->Size;
	heap->BytesOverhead = sizeof(Heap) + sizeof(HeapBlock);
	heap->FirstFreeBlock = block;
	heap->LastFreeBlock = block;

	return heap;
}

void HeapDebugDump(Heap* heap)
{
	// find largest free block
	HeapFreeBlock* entry = heap->FirstFreeBlock;
	HeapFreeBlock* largest = entry;
	while (entry)
	{
		if (entry->Size > largest->Size)
			largest = entry;
		entry = entry->Next;
	}
	
	// dump stats
	int frag = largest ? (100 - (100 * largest->Size) / heap->BytesAvailable) : 0;
	TmPrintf("-------------------- Heap Dump --------------------\n");
	TmPrintf("Size:               %u bytes (%d KiB)\n", heap->Size, heap->Size / 1024);
	TmPrintf("Allocated:          %u bytes\n", heap->BytesAllocated);
	TmPrintf("Available:          %u bytes\n", heap->BytesAvailable);
	TmPrintf("Overhead:           %u bytes\n", heap->BytesOverhead);
	TmPrintf("Total blocks:       %u\n", heap->UsedBlocks + heap->FreeBlocks);
	TmPrintf("Used blocks:        %u\n", heap->UsedBlocks);
	TmPrintf("Free blocks:        %u\n", heap->FreeBlocks);
	TmPrintf("Largest free block: %u bytes\n", largest->Size);
	TmPrintf("Fragmentation:      %d%%\n", frag);
	TmPrintf("First free block:   %08X\n", heap->FirstFreeBlock);
	TmPrintf("Last free block:    %08X\n", heap->LastFreeBlock);
	TmPrintf("\n");

	// dump free list
	TmPrintf(">>> Free list:\n");
	TmPrintf("Idx   | Beg addr | End addr | Size     \n");
	TmPrintf("------+----------+----------+----------\n");
	HeapFreeBlock* freeBlock = heap->FirstFreeBlock;
	int freeBlockIdx = 0;
	while (freeBlock)
	{
		uint8_t* end = (uint8_t*)freeBlock + freeBlock->Size + sizeof(HeapBlock);
		TmPrintf("#%-4d | %08X | %08X | %8u\n", freeBlockIdx++, freeBlock, end, freeBlock->Size);
		freeBlock = freeBlock->Next;
	}
	TmPrintf("\n");

	// dump memory
	TmPrintf(">>> Memory dump:\n");
	TmPrintf("Beg addr | End addr | Size     | Block kind    \n");
	TmPrintf("---------+----------+----------+---------------\n");
	TmPrintf("%08X | %08X | %8u | %s\n", heap, heap + 1, sizeof(Heap), "Heap header");
	HeapBlock* block = (HeapBlock*)(heap + 1);
	HeapBlock* heapEnd = (HeapBlock*)((uint8_t*)heap + heap->Size);
	do
	{
		HeapBlock* next = (HeapBlock*)((uint8_t*)(block + 1) + block->Size);
		const char* type = "CORRUPT";
		if (k_memcmp(block->Signature, HEAP_SIG_USED_BLOCK, sizeof(block->Signature)) == 0) type = "Used";
		if (k_memcmp(block->Signature, HEAP_SIG_FREE_BLOCK, sizeof(block->Signature)) == 0) type = "Available";
		TmPrintf("%08X | %08X | %8u | %s\n", block, next, block->Size, type);
		block = next;
	} while (block < heapEnd);
	TmPrintf("\n\n");
}

void* HeapAlloc(Heap* heap, size_t size)
{
	if (size < HEAP_MIN_ALLOC)
		size = HEAP_MIN_ALLOC;

	// Find largest free block (or perfect match)
	HeapFreeBlock* entry = heap->FirstFreeBlock;
	HeapFreeBlock* block = entry;
	while (entry)
	{
		if (entry->Size == size)
		{
			block = entry;
			break;
		}

		if (entry->Size > block->Size)
			block = entry;
		entry = entry->Next;
	}

	if (!block || block->Size < size)
		// Out of memory
		return NULL;

	size_t slack = block->Size - size;
	if (slack < sizeof(HeapFreeBlock))
	{
		size += slack;

		// Unlink free block
		if (block->Prev)
			block->Prev->Next = block->Next;
		else
			heap->FirstFreeBlock = block->Next;
		if (block->Next)
			block->Next->Prev = block->Prev;
		else
			heap->LastFreeBlock = block->Prev;

		// Update heap stats
		heap->UsedBlocks++;
		heap->FreeBlocks--;
		heap->BytesAllocated += size;
		heap->BytesAvailable -= size;

		// Setup used block
		k_memcpy(block->Signature, HEAP_SIG_USED_BLOCK, sizeof(block->Signature));
		return (HeapBlock*)block + 1;
	}

	// Create new free block
	HeapFreeBlock* newFreeBlock = (HeapFreeBlock*)((uint8_t*)block + size + sizeof(HeapBlock));
	newFreeBlock->Size = slack - sizeof(HeapBlock);
	k_memcpy(newFreeBlock->Signature, HEAP_SIG_FREE_BLOCK, sizeof(newFreeBlock->Signature));
	newFreeBlock->Next = block->Next;
	newFreeBlock->Prev = block->Prev;

	// Link with prev/next entries
	if (newFreeBlock->Prev)
		newFreeBlock->Prev->Next = newFreeBlock;
	else
		heap->FirstFreeBlock = newFreeBlock;
	if (newFreeBlock->Next)
		newFreeBlock->Next->Prev = newFreeBlock;
	else
		heap->LastFreeBlock = newFreeBlock;

	// Update heap stats
	heap->UsedBlocks++;
	heap->BytesAllocated += size;
	heap->BytesAvailable -= size + sizeof(HeapBlock);
	heap->BytesOverhead += sizeof(HeapBlock);

	// Setup used block
	block->Size = size;
	k_memcpy(block->Signature, HEAP_SIG_USED_BLOCK, sizeof(block->Signature));
	return (HeapBlock*)block + 1;
}

void* HeapRealloc(Heap* heap, void* ptr, size_t size)
{
	if (!ptr)
		return HeapAlloc(heap, size);

	size_t len = HeapMSize(heap, ptr);
	if (len >= size)
		return ptr;

	uint8_t* mem = (uint8_t*)HeapAlloc(heap, size);
	if (!mem)
		return NULL;

	k_memcpy(mem, ptr, len);
	HeapFree(heap, ptr);
	return mem;
}

void HeapFree(Heap* heap, void* ptr)
{
	DbgAssert(ptr != NULL);

	HeapFreeBlock* block = (HeapFreeBlock*)((HeapBlock*)ptr - 1);
	if (block->Signature[0] != HEAP_SIG_USED_BLOCK[0])
		return;

	// convert block
	k_memcpy(block->Signature, HEAP_SIG_FREE_BLOCK, sizeof(block->Signature));
	block->Next = NULL;
	block->Prev = NULL;

	// update stats
	heap->UsedBlocks--;
	heap->FreeBlocks++;
	heap->BytesAllocated -= block->Size;
	heap->BytesAvailable += block->Size;

	// Insert block
	HeapFreeBlock* entry = heap->FirstFreeBlock;
	while (entry)
	{
		if (block < entry)
		{
			// Link block
			block->Prev = entry->Prev;
			block->Next = entry;
			if (block->Prev)
				block->Prev->Next = block;
			else
				heap->FirstFreeBlock = block;
			entry->Prev = block;
			break;
		}

		entry = entry->Next;
	}

	if (entry == NULL)
	{
		// Append block
		if (heap->LastFreeBlock)
		{
			block->Prev = heap->LastFreeBlock;
			heap->LastFreeBlock->Next = block;
			heap->LastFreeBlock = block;
		}
		else
		{
			heap->FirstFreeBlock = block;
			heap->LastFreeBlock = block;
		}
	}

	// Coalesce blocks
	if (block->Next) HeapCoalesce(heap, block, block->Next);
	if (block->Prev) HeapCoalesce(heap, block->Prev, block);
}

size_t HeapMSize(Heap* heap, void* ptr)
{
	HeapBlock* block = (HeapBlock*)ptr - 1;
	return block->Signature[0] == HEAP_SIG_USED_BLOCK[0] ? block->Size : 0;
}
