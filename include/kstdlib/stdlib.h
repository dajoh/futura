#ifndef __STDLIB_H__
#define __STDLIB_H__

long k_strtol(const char* str, char** str_end, int base);
long long k_strtoll(const char* str, char** str_end, int base);
unsigned long k_strtoul(const char* str, char** str_end, int base);
unsigned long long k_strtoull(const char* str, char** str_end, int base);

#endif
