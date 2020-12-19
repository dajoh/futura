#ifndef __STDIO_H__
#define __STDIO_H__

#include <stddef.h>
#include <stdarg.h>

int k_vsnprintf(char* buf, size_t cap, const char* fmt, va_list args);
int k_snprintf(char* buf, size_t cap, const char* fmt, ...);

#endif
