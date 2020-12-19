#ifndef KERNEL_TEXTMODE_H
#define KERNEL_TEXTMODE_H

#include <stdarg.h>

#define TM_SCREEN_W 80
#define TM_SCREEN_H 25

#define TM_COLOR_BLACK     0
#define TM_COLOR_BLUE      1
#define TM_COLOR_GREEN     2
#define TM_COLOR_CYAN      3
#define TM_COLOR_RED       4
#define TM_COLOR_MAGENTA   5
#define TM_COLOR_BROWN     6
#define TM_COLOR_LTGRAY    7
#define TM_COLOR_DKGRAY    8
#define TM_COLOR_LTBLUE    9
#define TM_COLOR_LTGREEN   10
#define TM_COLOR_LTCYAN    11
#define TM_COLOR_LTRED     12
#define TM_COLOR_LTMAGENTA 13
#define TM_COLOR_YELLOW    14
#define TM_COLOR_WHITE     15

#define TM_COLOR_STACK_SIZE 16

void TmInitialize();
void TmClear();
void TmSetColor(int fg, int bg);
void TmGetColor(int* fg, int* bg);
void TmPushColor(int fg, int bg);
void TmPopColor();
void TmPutChar(char chr);
void TmPutString(const char* str);
void TmPrintf(const char* fmt, ...);
void TmVPrintf(const char* fmt, va_list args);
void TmColorPrintf(int fg, int bg, const char* fmt, ...);

#define TmPrintfErr(...) do { TmColorPrintf(TM_COLOR_LTRED, TM_COLOR_BLACK, __VA_ARGS__); } while (0)
#define TmPrintfWrn(...) do { TmColorPrintf(TM_COLOR_YELLOW, TM_COLOR_BLACK, __VA_ARGS__); } while (0)
#define TmPrintfInf(...) do { TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_BLACK, __VA_ARGS__); } while (0)
#define TmPrintfDbg(...) do { TmColorPrintf(TM_COLOR_LTGREEN, TM_COLOR_BLACK, __VA_ARGS__); } while (0)
#define TmPrintfVrb(...) do { TmColorPrintf(TM_COLOR_DKGRAY, TM_COLOR_BLACK, __VA_ARGS__); } while (0)

#endif
