#include <psf.h>
#include <ctype.h>
#include "debug.h"
#include "fbcon.h"
#include "memory.h"
#include "textmode.h"
#include "embedded.h"
#include "scheduler.h"
#include "interrupts.h"
#include "drivers/virtio_gpu.h"

typedef struct
{
    uint32_t codepoint;
    uint8_t* bitmap;
} PsfGlyph;

static uint8_t* ConFb = NULL;
static int ConFbWidth = 0;
static int ConFbHeight = 0;
static int ConFbPixels = 0;

static uint8_t* ConFont = NULL;
static uint8_t* ConFontEnd = NULL;
static PsfHeader* ConFontHeader = NULL;
static int ConFontWidth = 0;
static int ConFontHeight = 0;
static int ConFontStride = 0;
static PsfGlyph* ConFontGlyphs = NULL;
static size_t ConFontGlyphCount = 0;
static PsfGlyph* ConFontGlyphCache[256];

static int ConX = 0;
static int ConY = 0;
static int ConWidth = 0;
static int ConHeight = 0;
static uint16_t* ConBuffer = NULL;
static uint32_t ConPalette[16] =
{
    0xFF000000,
    0xFFA80000,
    0xFF00A800,
    0xFFA8A800,
    0xFF0000A8,
    0xFFA800A8,
    0xFF0054A8,
    0xFFA8A8A8,
    0xFF545454,
    0xFFFE5454,
    0xFF54FE54,
    0xFFFEFE54,
    0xFF5454FE,
    0xFFFE54FE,
    0xFF54FEFE,
    0xFFFEFEFE
};
static bool ConIsReady = false;
static bool ConIsDirty = false;

static void ConFbClear()
{
    uint8_t* ptr = ConFb;
    for (int i = 0; i < ConFbPixels; i++)
        *(uint32_t*)ptr = 0xFF000000;
}

static void ConFbDrawRect(int x, int y, int w, int h, uint32_t color)
{
    for (int dy = y; dy < (y + h); dy++)
    for (int dx = x; dx < (x + w); dx++)
    {
        int off = (dy * ConFbWidth + dx) * 4;
        *(uint32_t*)(ConFb + off) = color;
    }
}

static PsfGlyph* ConFontLookupGlyph(uint32_t codepoint)
{
    for (size_t i = 0; i < ConFontGlyphCount; i++)
        if (ConFontGlyphs[i].codepoint == codepoint)
            return &ConFontGlyphs[i];
    return NULL;
}

static void ConFbDrawChar(int x, int y, char chr, uint32_t fg, uint32_t bg)
{
    x *= ConFontWidth;
    y *= ConFontHeight;

    PsfGlyph* glyph = ConFontGlyphCache[(uint8_t)chr];
    if (!glyph || isspace(chr))
    {
        ConFbDrawRect(x, y, ConFontWidth, ConFontHeight, bg);
        return;
    }

    size_t bit = 0;
    uint8_t* bmp = glyph->bitmap;
    for (int dy = y; dy < (y + ConFontHeight); dy++)
    {
        for (int dx = x + ConFontWidth - 1; dx >= x; dx--)
        {
            int set = bmp[bit / 8] & (1 << (bit % 8));
            int off = (dy * ConFbWidth + dx) * 4;
            *(uint32_t*)(ConFb + off) = set ? fg : bg;
            bit++;
        }
        bit += ConFontStride;
    }
}

static void ConBufSetCell(int x, int y, uint16_t val)
{
    DbgAssert(x >= 0 && x < ConWidth && y >= 0 && y < ConHeight);

    size_t offset = y * ConWidth + x;
    uint16_t oldVal = ConBuffer[offset];
    ConBuffer[offset] = val;

    if (val != oldVal)
    {
        size_t fg = (val >> 8) & 0x0F;
        size_t bg = val >> 12;
        ConFbDrawChar(x, y, val & 0xFF, ConPalette[fg], ConPalette[bg]);
    }
}

static void ConScroll(uint8_t color)
{
    for (int y = 1; y < ConHeight; y++)
    for (int x = 0; x < ConWidth; x++)
        ConBufSetCell(x, y - 1, ConBuffer[y * ConWidth + x]);

    for (int x = 0; x < ConWidth; x++)
        ConBufSetCell(x, ConHeight - 1, ' ' | ((uint16_t)color << 8));
}

static void ConPutCharEx(char chr, uint8_t color)
{
    switch (chr)
    {
    case '\r':
        ConX = 0;
        break;
    case '\n':
        ConX = 0;
        ConY++;
        break;
    case '\b':
        if (ConX != 0)
        {
            ConX--;
        }
        else if (ConY != 0)
        {
            ConX = ConWidth - 1;
            ConY--;
        }
        break;
    case '\t':
        for (int i = 0; i < 4; i++) ConPutCharEx(' ', color); // TODO: this
        break;
    default:
        ConBufSetCell(ConX, ConY, chr | ((uint16_t)color << 8));
        ConX++;
        break;
    }

    if (ConX >= ConWidth)
    {
        ConX = 0;
        ConY++;
    }

    while (ConY >= ConHeight)
    {
        ConY--;
        ConScroll(color);
    }
}

void ConClear(uint8_t color)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        ConX = 0;
        ConY = 0;
        for (int y = 0; y < ConHeight; y++)
        for (int x = 0; x < ConWidth; x++)
            ConBufSetCell(x, y, ' ' | ((uint16_t)color << 8));
        ConIsDirty = true;
    }
    IntLeaveCriticalSection(irqLock);
}

void ConPutChar(char chr, uint8_t color)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        ConPutCharEx(chr, color);
        ConIsDirty = true;
    }
    IntLeaveCriticalSection(irqLock);
}

void ConPutString(const char* str, uint8_t color)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        while (*str)
            ConPutCharEx(*str++, color);
        ConIsDirty = true;
    }
    IntLeaveCriticalSection(irqLock);
}

static uint32_t ConPresentTask(void* ctx)
{
    while (true)
    {
        if (ConIsDirty)
        {
            uint32_t irqLock = IntEnterCriticalSection();
            {
                DrvVirtioGpu_PresentActive();
                ConIsDirty = false;
            }
            IntLeaveCriticalSection(irqLock);
        }
        SchYield();
    }

    return 0;
}

void ConInitialize(void* fb, int width, int height)
{
    // setup framebuffer
    ConFb = fb;
    ConFbWidth = width;
    ConFbHeight = height;
    ConFbPixels = width * height;

    // setup font
    ConFont = gohufont_uni_14b_psfu;
    ConFontEnd = ConFont + gohufont_uni_14b_psfu_len;
    ConFontHeader = (PsfHeader*)ConFont;
    ConFontWidth = ConFontHeader->width;
    ConFontHeight = ConFontHeader->height;
    ConFontStride = ((ConFontWidth + 7) / 8) * 8 - ConFontWidth;
    ConFontGlyphs = kcalloc(sizeof(PsfGlyph) * ConFontHeader->numglyph);
    ConFontGlyphCount = ConFontHeader->numglyph;
    DbgAssert(ConFontHeader->magic == PSF_FONT_MAGIC);
    DbgAssert(ConFontHeader->flags != 0);

    // load glyphs
    uint8_t* uniPtr = ConFont + sizeof(PsfHeader) + ConFontGlyphCount * ConFontHeader->bytesperglyph;
    for (size_t i = 0; i < ConFontGlyphCount; i++)
    {
        uint8_t* uniBeg = uniPtr;
        while (*uniPtr++ != 0xFF) ;
        size_t uniLen = uniPtr - uniBeg;
        ConFontGlyphs[i].codepoint = *uniBeg; // TODO: handle multiple codepoints
        ConFontGlyphs[i].bitmap = ConFont + sizeof(PsfHeader) + i * ConFontHeader->bytesperglyph;
    }
    TmPrintfDbg("%u glyphs loaded\n", ConFontGlyphCount);

    // build glyph cache
    for (size_t i = 0; i < 256; i++)
        ConFontGlyphCache[i] = ConFontLookupGlyph(i);

    // setup console params
    ConWidth = width / ConFontWidth;
    ConHeight = height / ConFontHeight;
    ConBuffer = kcalloc(sizeof(uint16_t) * ConWidth * ConHeight);

    // clear and copy from old textmode
    uint32_t irqLock = IntEnterCriticalSection();
    {
        ConFbClear();

        uint16_t* tmBuffer = (uint16_t*)0xC00B8000;
        for (int y = 0; y < TM_SCREEN_H; y++)
        for (int x = 0; x < TM_SCREEN_W; x++)
            ConBufSetCell(x, y, tmBuffer[y * TM_SCREEN_W + x]);
        TmGetPos(&ConX, &ConY);

        SchCreateTask("fbcon", 64*1024, ConPresentTask, NULL);

        ConIsReady = true;
        ConIsDirty = true;
    }
    IntLeaveCriticalSection(irqLock);
}

bool ConIsInitialized()
{
    return ConIsReady;
}
