#include <string.h>
#include "debug.h"
#include "bitmap.h"
#include "textmode.h"

size_t BitmapCalcSize(size_t bits)
{
	size_t words = (bits + (BITMAP_WORD_BITS - 1)) / BITMAP_WORD_BITS;
	return sizeof(Bitmap) + sizeof(BITMAP_WORD_TYPE) * (words - 1);
}

Bitmap* BitmapInitialize(void* mem, size_t bits)
{
	Bitmap* bmp = (Bitmap*)mem;
	k_memset(bmp, 0, BitmapCalcSize(bits));
	bmp->Size = bits;
	return bmp;
}

void BitmapDebugDump(Bitmap* bmp)
{
	TmPrintf(">>> Bitmap dump:\n");
	TmPrintf("|");
	for (size_t i = 0; i < bmp->Size; i++)
		TmPrintf("%d", BitmapGetBit(bmp, i));
	TmPrintf("|\n");
	TmPrintf("|");
	for (size_t i = 0; i < bmp->Size; i++)
		TmPrintf("%d", i % 10);
	TmPrintf("|\n\n");
}

bool BitmapGetBit(Bitmap* bmp, size_t offset)
{
	DbgAssert(offset < bmp->Size);
	size_t idx = offset / BITMAP_WORD_BITS;
	size_t bit = offset % BITMAP_WORD_BITS;
	return bmp->Words[idx] & (1 << bit);
}

void BitmapSetBit(Bitmap* bmp, size_t offset, bool val)
{
	DbgAssert(offset < bmp->Size);
	size_t idx = offset / BITMAP_WORD_BITS;
	size_t bit = offset % BITMAP_WORD_BITS;
	if (val)
		bmp->Words[idx] |= (1 << bit);
	else
		bmp->Words[idx] &= ~(1 << bit);
}

void BitmapSetBits(Bitmap* bmp, size_t offset, size_t length, bool val)
{
	size_t end = offset + length;
	DbgAssert(offset < bmp->Size);
	DbgAssert(end <= bmp->Size);
	for (size_t bit = offset; bit < end; bit++)
		BitmapSetBit(bmp, bit, val);
}

size_t BitmapFindFirstBit(Bitmap* bmp, size_t offset, bool val)
{
	DbgAssert(offset < bmp->Size);
	size_t end = bmp->Size;
	for (size_t bit = offset; bit < end; bit++)
		if (BitmapGetBit(bmp, bit) == val)
			return bit;
	return BITMAP_INVALID_OFFSET;
}

size_t BitmapFindFirstRegion(Bitmap* bmp, size_t offset, size_t length, bool val)
{
	DbgAssert(offset < bmp->Size);
	DbgAssert(length > 0);
	size_t bit = offset;
	size_t end = bmp->Size;
	size_t lim = end - length + 1;
	while (bit < lim)
	{
		if (BitmapGetBit(bmp, bit++) == val)
		{
			size_t len = 1;
			while (len < length && bit < end && BitmapGetBit(bmp, bit++) == val)
				len++;
			if (len == length)
				return bit - len;
			bit--;
		}
	}
	return BITMAP_INVALID_OFFSET;
}

size_t BitmapCountSetBits(Bitmap* bmp)
{
	if (bmp->Size == 0)
		return 0;
	
	size_t result = 0;
	size_t words = (bmp->Size + (BITMAP_WORD_BITS - 1)) / BITMAP_WORD_BITS;

	for (size_t idx = 0; idx < words - 1; idx++)
	{
		BITMAP_WORD_TYPE word = bmp->Words[idx];
		if (word == 0)
			continue;

		if (word == BITMAP_WORD_ALL_SET)
		{
			result += BITMAP_WORD_BITS;
			continue;
		}

		for (size_t off = 0; off < BITMAP_WORD_BITS; off++)
			if (word & (1 << off))
				result++;
	}

	size_t rest = bmp->Size % BITMAP_WORD_BITS;
	if (rest != 0)
	{
		BITMAP_WORD_TYPE word = bmp->Words[words - 1];
		for (size_t off = 0; off < rest; off++)
			if (word & (1 << off))
				result++;
	}

	return result;
}
