#ifndef __STDIO_H__
#define __STDIO_H__

#include <stddef.h>
#include <stdarg.h>

int vsnprintf(char* buf, size_t cap, const char* fmt, va_list args);
int snprintf(char* buf, size_t cap, const char* fmt, ...);

#endif
