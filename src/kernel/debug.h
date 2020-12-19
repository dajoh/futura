#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include <stddef.h>

#define DbgPanic(...) DbgPanicImpl(__FILE__, __LINE__, __VA_ARGS__)
#define DbgUnreachable() DbgPanic("Unreachable code detected")
#define DbgUnreachableCase(v) DbgPanic("Unreachable code detected: %s has invalid value %d", #v, v)

#define DbgAssert(expr) \
  do { \
    if (!(expr)) DbgAssertImpl(__FILE__, __LINE__, #expr); \
  } while (0)

#define DbgAssertMsg(expr, ...) \
  do { \
    if (!(expr)) DbgAssertMsgImpl(__FILE__, __LINE__, #expr, __VA_ARGS__); \
  } while (0)

void DbgPanicImpl(const char* file, int line, const char* fmt, ...);
void DbgAssertImpl(const char* file, int line, const char* expr);
void DbgAssertMsgImpl(const char* file, int line, const char* expr, const char* fmt, ...);
void DbgHexdump(void* ptr, size_t size);

#endif
