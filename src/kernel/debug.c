#include <stdio.h>
#include <ctype.h>
#include "debug.h"
#include "textmode.h"
#include "interrupts.h"

static char DbgPanicBuffer[2048];

void DbgPanicImpl(const char* file, int line, const char* fmt, ...)
{
	IntDisableIRQs();

	va_list args;
	va_start(args, fmt);
	k_vsnprintf(DbgPanicBuffer, sizeof(DbgPanicBuffer), fmt, args);
	va_end(args);

	TmSetColor(TM_COLOR_LTRED, TM_COLOR_BLACK);
	TmPrintf("KERNEL PANIC at %s:%d: %s\n", file, line, DbgPanicBuffer);

	for (;;)
		asm volatile("hlt");
}

void DbgAssertImpl(const char* file, int line, const char* expr)
{
	uint32_t irqLock = IntEnterCriticalSection();
	{
		int fg, bg;
		TmGetColor(&fg, &bg);
		TmSetColor(TM_COLOR_LTRED, TM_COLOR_BLACK);
		TmPrintf("Assert failed at %s:%d: %s", file, line, expr);
		TmSetColor(fg, bg);
		TmPutChar('\n');

		for (;;)
			asm volatile("hlt");
	}
	IntLeaveCriticalSection(irqLock);
}

void DbgAssertMsgImpl(const char* file, int line, const char* expr, const char* fmt, ...)
{
	uint32_t irqLock = IntEnterCriticalSection();
	{
		va_list args;
		va_start(args, fmt);
		k_vsnprintf(DbgPanicBuffer, sizeof(DbgPanicBuffer), fmt, args);
		va_end(args);

		int fg, bg;
		TmGetColor(&fg, &bg);
		TmSetColor(TM_COLOR_LTRED, TM_COLOR_BLACK);
		TmPrintf("Assert failed at %s:%d: %s (%s)", file, line, DbgPanicBuffer, expr);
		TmSetColor(fg, bg);
		TmPutChar('\n');

		for (;;)
			asm volatile("hlt");
	}
	IntLeaveCriticalSection(irqLock);
}

void DbgHexdump(void* ptr, size_t size)
{
	uint32_t irqLock = IntEnterCriticalSection();
	{
		size_t off = 0;
		uint8_t* mem = (uint8_t*)ptr;
		uint8_t* end = mem + size;

		while (off < size)
		{
			TmPrintf("%#p | ", mem + off);
			for (size_t i = 0; i < 16; i++)
			{
				uint8_t* cur = mem + off + i;
				if (cur < end)
					TmPrintf("%02X ", *cur);
				else
					TmPutString("   ");
			}
			TmPutString("| ");
			for (size_t i = 0; i < 16; i++)
			{
				uint8_t* cur = mem + off + i;
				if (cur < end)
					TmPutChar(k_isprint(*cur) ? *cur : '.');
				else
					break;
			}
			TmPutChar('\n');
			off += 16;
		}
	}
	IntLeaveCriticalSection(irqLock);
}
