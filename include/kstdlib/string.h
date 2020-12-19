#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

#ifndef KSTDRESTRICT
#define KSTDRESTRICT __restrict
#endif

void* k_memchr(const void* m, int c, size_t n);
int k_memcmp(const void* m1, const void* m2, size_t n);
void* k_memcpy(void* KSTDRESTRICT dst, const void* KSTDRESTRICT src, size_t n);
void* k_memmove(void* dst, const void* src, size_t n);
void* k_memset(void* m, int c, size_t n);
char* k_strcat(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src);
char* k_strchr(const char* s, int c);
int k_strcmp(const char* s1, const char* s2);
char* k_strcpy(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src);
size_t k_strlen(const char* s);
char* k_strncat(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src, size_t n);
int k_strncmp(const char* s1, const char* s2, size_t n);
char* k_strncpy(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src, size_t n);
char* k_strrchr(const char* s, int c);
char* k_strrev(char* s);
char* k_strstr(const char* s, const char* needle);

#endif
