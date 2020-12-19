#ifndef KERNEL_BITMAP_H
#define KERNEL_BITMAP_H

#include <stddef.h>
#include <stdbool.h>

#define BITMAP_INVALID_OFFSET ((size_t)~0u)
#define BITMAP_WORD_TYPE size_t
#define BITMAP_WORD_BITS (sizeof(BITMAP_WORD_TYPE)*8)
#define BITMAP_WORD_ALL_SET ((size_t)~0u)

typedef struct Bitmap_s
{
	size_t Size;
	BITMAP_WORD_TYPE Words[1];
} Bitmap;

size_t BitmapCalcSize(size_t bits);

Bitmap* BitmapInitialize(void* mem, size_t bits);
void BitmapDebugDump(Bitmap* bmp);

bool BitmapGetBit(Bitmap* bmp, size_t offset);
void BitmapSetBit(Bitmap* bmp, size_t offset, bool val);
void BitmapSetBits(Bitmap* bmp, size_t offset, size_t length, bool val);

size_t BitmapFindFirstBit(Bitmap* bmp, size_t offset, bool val);
size_t BitmapFindFirstRegion(Bitmap* bmp, size_t offset, size_t length, bool val);

size_t BitmapCountSetBits(Bitmap* bmp);

#endif
