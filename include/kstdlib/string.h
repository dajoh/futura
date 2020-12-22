#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

#ifndef KSTDRESTRICT
#define KSTDRESTRICT __restrict
#endif

void* memchr(const void* m, int c, size_t n);
int memcmp(const void* m1, const void* m2, size_t n);
void* memcpy(void* KSTDRESTRICT dst, const void* KSTDRESTRICT src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* m, int c, size_t n);
char* strcat(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src);
char* strchr(const char* s, int c);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src);
size_t strlen(const char* s);
char* strncat(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src, size_t n);
int strncmp(const char* s1, const char* s2, size_t n);
char* strncpy(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src, size_t n);
char* strrchr(const char* s, int c);
char* strrev(char* s);
char* strstr(const char* s, const char* needle);

#endif
