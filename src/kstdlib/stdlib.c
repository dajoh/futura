#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static inline bool isbasedigit(char c, int base)
{
	if (base <= 10)
		return c >= '0' && c <= '9' - (10 - base);
	return (c >= '0' && c <= '9') ||
		   (c >= 'a' && c <= 'z' - (36 - base)) ||
		   (c >= 'A' && c <= 'Z' - (36 - base));
}

static inline unsigned int basedigitvalue(char c)
{
	if (c >= '0' && c <= '9')
		return (unsigned int)c - '0';
	if (c >= 'a' && c <= 'z')
		return (unsigned int)c - 'a' + 10;
	if (c >= 'A' && c <= 'Z')
		return (unsigned int)c - 'A' + 10;
	return 0;
}

static bool strtox(const char* str, int base, char* psign, uintmax_t* num, const char** end)
{
	// validate base
	if (base != 0 && (base < 2 || base > 36))
	{
		*num = 0;
		*end = str;
		return false;
	}
	
	const char* ptr = str;

	// Discards any whitespace characters (as identified by calling isspace()) until the first non-whitespace character is found
	while (isspace(*ptr))
		ptr++;

	// (optional) plus or minus sign
	char sign = *ptr;
	if (sign == '-' || sign == '+')
		ptr++;
	else
		sign = '+';
	*psign = sign;

	// (optional) prefix 0x/0X indicating hexadecimal base (applies only when the base is 16 or ​0​)
	if ((base == 0 || base == 16) && *ptr == '0' && (*(ptr + 1) == 'x' || *(ptr + 1) == 'X'))
	{
		base = 16;
		ptr += 2;
	}

	// (optional) prefix 0 indicating octal base (applies only when the base is 8 or ​0​)
	bool zero_prefix = (base == 0 || base == 8) && *ptr == '0';
	if (zero_prefix)
	{
		base = 8;
		ptr++;
	}
	
	// if we still don't know what base it is, default to 10
	if (base == 0)
		base = 10;

	// a sequence of digits
	const char* beg = ptr;
	while (isbasedigit(*ptr, base))
		ptr++;

	// no digits?
	if (ptr == beg)
	{
		*num = 0;
		*end = zero_prefix ? ptr : str;
		return zero_prefix;
	}

	// parse digits
	uintmax_t ret = 0;
	const char* digit = beg;
	do
	{
		uintmax_t next = ret * (unsigned int)base + basedigitvalue(*digit++);

        // check for overflow
		if (next <= ret)
		{
			errno = ERANGE;
			*num = UINTMAX_MAX;
			*end = ptr;
			return false;
		}

		ret = next;
	} while (digit != ptr);

	*num = ret;
	*end = ptr;
	return true;
}

long strtol(const char* str, char** str_end, int base)
{
	char sign;
	uintmax_t num;
	const char* end;
	bool ret = strtox(str, base, &sign, &num, &end);

	if (str_end)
		*str_end = (char*)end;
	
	if (!ret && num != UINTMAX_MAX)
		return 0;

	if (sign == '+')
	{
		if (num <= (uintmax_t)LONG_MAX)
			return (long)num;
		errno = ERANGE;
		return LONG_MAX;
	}
	else
	{
		if (num <= (uintmax_t)LONG_MAX + 1ull)
			return (long)-(intmax_t)num;
		errno = ERANGE;
		return LONG_MIN;
	}
}

long long strtoll(const char* str, char** str_end, int base)
{
	char sign;
	uintmax_t num;
	const char* end;
	bool ret = strtox(str, base, &sign, &num, &end);

	if (str_end)
		*str_end = (char*)end;

	if (!ret && num != UINTMAX_MAX)
		return 0;

	if (sign == '+')
	{
		if (num <= (uintmax_t)LLONG_MAX)
			return (long long)num;
		errno = ERANGE;
		return LLONG_MAX;
	}
	else
	{
		if (num <= (uintmax_t)LLONG_MAX + 1ull)
			return (long long)-(intmax_t)num;
		errno = ERANGE;
		return LLONG_MIN;
	}
}

unsigned long strtoul(const char* str, char** str_end, int base)
{
	char sign;
	uintmax_t num;
	const char* end;
	bool ret = strtox(str, base, &sign, &num, &end);

	if (str_end)
		*str_end = (char*)end;

	if (!ret && num != UINTMAX_MAX)
		return 0;

	if (num > (uintmax_t)ULONG_MAX)
	{
		errno = ERANGE;
		return ULONG_MAX;
	}
	
	if (sign == '+')
		return (unsigned long)num;
	else
		return (unsigned long)-(long)(unsigned long)num;
}

unsigned long long strtoull(const char* str, char** str_end, int base)
{
	char sign;
	uintmax_t num;
	const char* end;
	bool ret = strtox(str, base, &sign, &num, &end);

	if (str_end)
		*str_end = (char*)end;

	if (!ret)
		return num;

	if (sign == '+')
		return num;
	else
		return (unsigned long long)-(long long)num;
}
