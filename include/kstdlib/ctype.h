#ifndef __CTYPE_H__
#define __CTYPE_H__

/* TODO: implement
static inline int k_tolower(int c);
static inline int k_toupper(int c);
*/

static inline int k_isdigit(int c)  { return (c >= '0' && c <= '9'); }
static inline int k_islower(int c)  { return (c >= 'a' && c <= 'z'); }
static inline int k_isupper(int c)  { return (c >= 'A' && c <= 'Z'); }
static inline int k_isalpha(int c)  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int k_isalnum(int c)  { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int k_isxdigit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static inline int k_isblank(int c)  { return c == ' ' || c == '\t'; }
static inline int k_isspace(int c)  { return c == ' ' || (c >= '\t' && c <= '\r'); }
static inline int k_iscntrl(int c)  { return (c >= 0 && c <= 31) || c == 127; }
static inline int k_ispunct(int c)  { return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126); }
static inline int k_isgraph(int c)  { return k_isalnum(c) || k_ispunct(c); }
static inline int k_isprint(int c)  { return c == ' ' || k_isalnum(c) || k_ispunct(c); }

#endif
