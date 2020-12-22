#ifndef __CTYPE_H__
#define __CTYPE_H__

static inline int isdigit(int c)  { return (c >= '0' && c <= '9'); }
static inline int islower(int c)  { return (c >= 'a' && c <= 'z'); }
static inline int isupper(int c)  { return (c >= 'A' && c <= 'Z'); }
static inline int isalpha(int c)  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int isalnum(int c)  { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int isxdigit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static inline int isblank(int c)  { return c == ' ' || c == '\t'; }
static inline int isspace(int c)  { return c == ' ' || (c >= '\t' && c <= '\r'); }
static inline int iscntrl(int c)  { return (c >= 0 && c <= 31) || c == 127; }
static inline int ispunct(int c)  { return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126); }
static inline int isgraph(int c)  { return isalnum(c) || ispunct(c); }
static inline int isprint(int c)  { return c == ' ' || isalnum(c) || ispunct(c); }
static inline int tolower(int c)  { return isupper(c) ? c + 32 : c; }
static inline int toupper(int c)  { return islower(c) ? c - 32 : c; }

#endif
