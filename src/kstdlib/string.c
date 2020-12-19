#include <string.h>

void* k_memchr(const void* m, int c, size_t n)
{
    const unsigned char* p = (const unsigned char*)m;
    while (n--)
    {
        if (*p == (unsigned char)c)
            return (void*)p;
        ++p;
    }
    return NULL;
}

int k_memcmp(const void* m1, const void* m2, size_t n)
{
    const unsigned char* p1 = (const unsigned char*)m1;
    const unsigned char* p2 = (const unsigned char*)m2;
    while (n--)
    {
        if (*p1 != *p2)
            return *p1 - *p2;
        ++p1;
        ++p2;
    }
    return 0;
}

void* k_memcpy(void* KSTDRESTRICT dst, const void* KSTDRESTRICT src, size_t n)
{
    char* pdst = (char*)dst;
    const char* psrc = (const char*)src;
    while (n--)
        *pdst++ = *psrc++;
    return dst;
}

void* k_memmove(void* dst, const void* src, size_t n)
{
    if (dst <= src)
    {
        char* pdst = (char*)dst;
        const char* psrc = (const char*)src;
        while (n--)
            *pdst++ = *psrc++;
    }
    else
    {
        char* pdst = (char*)dst + n;
        const char* psrc = (const char*)src + n;
        while (n--)
            *--pdst = *--psrc;
    }
    return dst;
}

void* k_memset(void* m, int c, size_t n)
{
    unsigned char* p = (unsigned char*)m;
    while (n--)
        *p++ = (unsigned char)c;
    return m;
}

char* k_strcat(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src)
{
    char* beg = dst;
    if (*dst)
        while (*++dst)
            ;
    while ((*dst++ = *src++))
        ;
    return beg;
}

char* k_strchr(const char* s, int c)
{
    do
    {
        if (*s == (char)c)
            return (char*)s;
    } while (*s++);
    return NULL;
}

int k_strcmp(const char* s1, const char* s2)
{
    while ((*s1) && (*s1 == *s2))
    {
        ++s1;
        ++s2;
    }
    return (*(unsigned char*)s1 - *(unsigned char*)s2);
}

char* k_strcpy(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src)
{
    char* beg = dst;
    while ((*dst++ = *src++))
        ;
    return beg;
}

size_t k_strlen(const char* s)
{
    size_t len = 0;
    while (s[len])
        ++len;
    return len;
}

char* k_strncat(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src, size_t n)
{
    char* beg = dst;
	while (*dst)
        ++dst;
    while (n && (*dst++ = *src++))
        --n;
    if (n == 0)
        *dst = '\0';
    return beg;
}

int k_strncmp(const char* s1, const char* s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        ++s1;
        ++s2;
        --n;
    }
    return n == 0 ? 0 : *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* k_strncpy(char* KSTDRESTRICT dst, const char* KSTDRESTRICT src, size_t n)
{
    char* beg = dst;
    while (n && (*dst++ = *src++))
        --n;
    while (n-- > 1)
        *dst++ = '\0';
    return beg;
}

char* k_strrchr(const char* s, int c)
{
    size_t i = 0;
    while (s[i++])
        ;
    do
    {
        if (s[--i] == (char)c)
            return (char*)s + i;
    } while (i);
    return NULL;
}

char* k_strrev(char* s)
{
    if (*s)
    {
	    char* p1 = s;
	    char* p2 = s + k_strlen(s) - 1;
	    while (p2 > p1)
	    {
		    char tmp = *p1;
		    *p1 = *p2;
		    *p2 = tmp;
		    p1++;
		    p2--;
	    }
    }
    return s;
}

char* k_strstr(const char* s, const char* needle)
{
    const char* p1 = s;
    const char* p2;

    while (*s)
    {
        p2 = needle;

        while (*p2 && (*p1 == *p2))
        {
            ++p1;
            ++p2;
        }

        if (*p2 == '\0')
            return (char*)s;

        ++s;
        p1 = s;
    }

    return NULL;
}
