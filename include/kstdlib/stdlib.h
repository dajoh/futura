#ifndef __STDLIB_H__
#define __STDLIB_H__

long strtol(const char* str, char** str_end, int base);
long long strtoll(const char* str, char** str_end, int base);
unsigned long strtoul(const char* str, char** str_end, int base);
unsigned long long strtoull(const char* str, char** str_end, int base);

#endif
