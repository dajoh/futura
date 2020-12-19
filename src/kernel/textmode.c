#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "debug.h"
#include "comport.h"
#include "lowlevel.h"
#include "textmode.h"
#include "interrupts.h"

static int TmX;
static int TmY;
static int TmColor;
static uint16_t* TmVideoMemory = (uint16_t*)0xC00B8000;
static uint16_t TmColorStack[TM_COLOR_STACK_SIZE];
static uint16_t* TmColorStackPtr = &TmColorStack[0];

static void TmScroll()
{
    uint16_t* firstLine = &TmVideoMemory[0];
    uint16_t* secondLine = &TmVideoMemory[TM_SCREEN_W];
    k_memmove(firstLine, secondLine, sizeof(uint16_t) * TM_SCREEN_W * (TM_SCREEN_H - 1));

    uint16_t* lastLine = &TmVideoMemory[TM_SCREEN_W * (TM_SCREEN_H - 1)];
    for (int x = 0; x < TM_SCREEN_W; x++)
        lastLine[x] = ' ' | (TmColor << 8);
}

static void TmEnableCursor(uint8_t start, uint8_t end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}

static void TmUpdateCursor()
{
    uint16_t pos = TmY * TM_SCREEN_W + TmX;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void TmInitialize()
{
    TmSetColor(TM_COLOR_LTGRAY, TM_COLOR_BLACK);
    TmClear();
    TmEnableCursor(14, 15);
}

void TmClear()
{
    TmX = 0;
    TmY = 0;
    for (int y = 0; y < TM_SCREEN_H; y++)
    for (int x = 0; x < TM_SCREEN_W; x++)
        TmVideoMemory[y * TM_SCREEN_W + x] = ' ' | (TmColor << 8);
    TmUpdateCursor();
}

void TmSetColor(int fg, int bg)
{
    TmColor = fg | (bg << 4);
}

void TmGetColor(int* fg, int* bg)
{
    if (fg) *fg = TmColor & 0x0F;
    if (bg) *bg = TmColor >> 4;
}

void TmPushColor(int fg, int bg)
{
    DbgAssertMsg(TmColorStackPtr != (TmColorStack + TM_COLOR_STACK_SIZE), "color stack full");
    *TmColorStackPtr++ = TmColor;
    TmSetColor(fg, bg);
}

void TmPopColor()
{
    DbgAssertMsg(TmColorStackPtr != TmColorStack, "color stack empty");
    TmColor = *--TmColorStackPtr;
}

static void TmPutCharEx(char chr, bool updateCursor)
{
    ComWrite((uint8_t)chr);

    switch (chr)
    {
    case '\r':
        TmX = 0;
        break;
    case '\n':
        TmX = 0;
        TmY++;
        break;
    case '\b':
        if (TmX != 0)
        {
            TmX--;
        }
        else if (TmY != 0)
        {
            TmX = TM_SCREEN_W - 1;
            TmY--;
        }
        break;
    case '\t':
        // TODO: this
        for (int i = 0; i < 4; i++) TmPutChar(' ');
        break;
    default:
        TmVideoMemory[TmY * TM_SCREEN_W + TmX] = chr | (TmColor << 8);
        TmX++;
        break;
    }

    if (TmX >= TM_SCREEN_W)
    {
        TmX = 0;
        TmY++;
    }

    while (TmY >= TM_SCREEN_H)
    {
        TmY--;
        TmScroll();
    }

    if (updateCursor)
        TmUpdateCursor();
}

void TmPutChar(char chr)
{
    TmPutCharEx(chr, true);
}

void TmPutString(const char* str)
{
    uint32_t irqLock = IntEnterCriticalSection();
    while (*str)
        TmPutCharEx(*str++, false);
    TmUpdateCursor();
    IntLeaveCriticalSection(irqLock);
}

void TmPrintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TmVPrintf(fmt, args);
    va_end(args);
}

void TmVPrintf(const char* fmt, va_list args)
{
    char buf[2048];
    k_vsnprintf(buf, sizeof(buf), fmt, args);
    TmPutString(buf);
}

void TmColorPrintf(int fg, int bg, const char* fmt, ...)
{
    uint32_t irqLock = IntEnterCriticalSection();
    TmPushColor(fg, bg);
    {
        va_list args;
        va_start(args, fmt);
        TmVPrintf(fmt, args);
        va_end(args);
    }
    TmPopColor();
    IntLeaveCriticalSection(irqLock);
}
