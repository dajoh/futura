#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define PRINTF_FLAG_LEFT  (1 << 0)
#define PRINTF_FLAG_SIGN  (1 << 1)
#define PRINTF_FLAG_SPACE (1 << 2)
#define PRINTF_FLAG_ALT   (1 << 3)
#define PRINTF_FLAG_ZERO  (1 << 4)

#define PRINTF_SIZE_DEFAULT     0
#define PRINTF_SIZE_HH_CHAR     1
#define PRINTF_SIZE_H_SHORT     2
#define PRINTF_SIZE_L_LONG      3
#define PRINTF_SIZE_LL_LONGLONG 4
#define PRINTF_SIZE_J_INTMAX    5
#define PRINTF_SIZE_Z_SIZE      6
#define PRINTF_SIZE_T_PTRDIFF   7

static size_t unsigned_to_str(char* buf, uintmax_t num, int base, bool uppercase)
{
	if (base < 2 || base > 36)
		return 0;

	const char* digits = uppercase ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
	char* ptr = buf;
	do
	{
		*ptr++ = digits[num % base];
		num /= base;
	} while (num != 0);
	*ptr = '\0';

	strrev(buf);
	return ptr - buf;
}

static const char* str_to_unsigned(const char* buf, uintmax_t* num)
{
	uintmax_t ret = 0;
	const char* ptr = buf;

	while (isdigit(*ptr))
		ret = ret * 10 + (*ptr++ - '0');

	*num = ret;
	return ptr;
}

int vsnprintf(char* buf, size_t cap, const char* fmt, va_list args)
{
	#define write_c(c)     do { char __c = c; if (len++ < cap) buf[len - 1] = __c; } while (0)
	#define write_s(s)     do { const char* __s = s; while (*__s) write_c(*__s++); } while (0)
	#define write_ns(s, n) do { const char* __s = s; int __n = n; while (__n-- && *__s) write_c(*__s++); } while (0)
	#define write_p(c, n)  do { int __n = (int)n; for (int __i = 0; __i < __n; __i++) write_c(c); } while (0)
	#define write_p0()     do { if (width > arg_width && !left_align) write_p(left_pad_char, width - arg_width); } while (0)
	#define write_p1()     do { if (width > arg_width && left_align) write_p(right_pad_char, width - arg_width); } while (0)

	size_t len = 0;
	char num[21];

	while (true)
	{
		// Check for end of string
		char first = *fmt++;
		if (first == '\0')
		{
			write_c('\0');
			break;
		}

		// Check for %
		if (first != '%')
		{
			write_c(first);
			continue;
		}
		
		// Check for %%
		if (*fmt == '%')
		{
			fmt++;
			write_c('%');
			continue;
		}

		// Store beginning of formatter
		const char* beg = fmt - 1;
		
		// Parse flags
		int flags = 0;
		while (true)
		{
			switch (*fmt)
			{
			case '-': flags |= PRINTF_FLAG_LEFT; fmt++; continue;
			case '+': flags |= PRINTF_FLAG_SIGN; fmt++; continue;
			case ' ': flags |= PRINTF_FLAG_SPACE; fmt++; continue;
			case '#': flags |= PRINTF_FLAG_ALT; fmt++; continue;
			case '0': flags |= PRINTF_FLAG_ZERO; fmt++; continue;
			}
			break;
		}

		// Parse width
		uint64_t width = 0;
		if (*fmt == '*')
		{
			fmt++;
			int arg = va_arg(args, int);
			if (arg < 0)
			{
				flags |= PRINTF_FLAG_LEFT;
				width = (uint64_t)-arg;
			}
			else
			{
				width = (uint64_t)arg;
			}
		}
		else
		{
			fmt = str_to_unsigned(fmt, &width);
		}

		// Parse precision
		uint64_t precision = 0;
		bool has_precision = false;
		if (*fmt == '.')
		{
			fmt++;
			if (*fmt == '*')
			{
				fmt++;
				int arg = va_arg(args, int);
				if (arg >= 0)
				{
					has_precision = true;
					precision = (uint64_t)arg;
				}
			}
			else
			{
				has_precision = true;
				fmt = str_to_unsigned(fmt, &precision);
			}
		}

		// Parse size
		int size = PRINTF_SIZE_DEFAULT;
		switch (*fmt)
		{
		case 'h': fmt++; size = PRINTF_SIZE_H_SHORT; if (*fmt == 'h') { fmt++; size = PRINTF_SIZE_HH_CHAR; } break;
		case 'l': fmt++; size = PRINTF_SIZE_L_LONG; if (*fmt == 'l') { fmt++; size = PRINTF_SIZE_LL_LONGLONG; } break;
		case 'j': fmt++; size = PRINTF_SIZE_J_INTMAX; break;
		case 'z': fmt++; size = PRINTF_SIZE_Z_SIZE; break;
		case 't': fmt++; size = PRINTF_SIZE_T_PTRDIFF; break;
		}

		// Determine padding characters
		bool left_align = (flags & PRINTF_FLAG_LEFT) != 0;
		char left_pad_char = (flags & PRINTF_FLAG_ZERO) && !has_precision ? '0' : ' ';
		char right_pad_char = ' ';
		
		// Parse type
		switch (*fmt++)
		{
		case 'c':
		{
			char arg = (char)va_arg(args, int);
			size_t arg_width = 1;
			write_p0();
			write_c(arg);
			write_p1();
			break;
		}
		case 's':
		{
			const char* arg = va_arg(args, const char*);
			size_t arg_width = strlen(arg);
			if (has_precision && precision < arg_width)
				arg_width = (size_t)precision;
			write_p0();
			write_ns(arg, arg_width);
			write_p1();
			break;
		}
		case 'd':
		case 'i':
		{
			intmax_t arg;
			switch (size)
			{
			case PRINTF_SIZE_HH_CHAR: arg = (signed char)va_arg(args, int); break;
			case PRINTF_SIZE_H_SHORT: arg = (short)va_arg(args, int); break;
			case PRINTF_SIZE_L_LONG: arg = va_arg(args, long); break;
			case PRINTF_SIZE_LL_LONGLONG: arg = va_arg(args, long long); break;
			case PRINTF_SIZE_J_INTMAX: arg = va_arg(args, intmax_t); break;
			case PRINTF_SIZE_Z_SIZE: arg = va_arg(args, intptr_t); break;
			case PRINTF_SIZE_T_PTRDIFF: arg = va_arg(args, ptrdiff_t); break;
			default: arg = va_arg(args, int); break;
			}

			char prefix = '\0';
			if (flags & PRINTF_FLAG_SPACE && arg >= 0) prefix = ' ';
			if (flags & PRINTF_FLAG_SIGN && arg >= 0) prefix = '+';
			if (arg < 0) prefix = '-';

			size_t arg_len = unsigned_to_str(num, arg < 0 ? -arg : arg, 10, false);
			size_t prefix_len = prefix == '\0' ? 0 : 1;
			size_t num_zeroes = 0;
			if (has_precision)
			{
				if (precision == 0 && arg == 0)
					arg_len = 0;
				else if (precision > arg_len)
					num_zeroes = (size_t)precision - arg_len;
			}
			size_t arg_width = arg_len + num_zeroes + prefix_len;

			if (prefix_len && left_pad_char == '0') write_c(prefix);
			write_p0();
			if (prefix_len && left_pad_char == ' ') write_c(prefix);
			write_p('0', num_zeroes);
			write_ns(num, arg_len);
			write_p1();
			break;
		}
		case 'u':
		case 'o':
		case 'x':
		case 'X':
		{
			uintmax_t arg;
			switch (size)
			{
			case PRINTF_SIZE_HH_CHAR: arg = (unsigned char)va_arg(args, int); break;
			case PRINTF_SIZE_H_SHORT: arg = (unsigned short)va_arg(args, int); break;
			case PRINTF_SIZE_L_LONG: arg = va_arg(args, unsigned long); break;
			case PRINTF_SIZE_LL_LONGLONG: arg = va_arg(args, unsigned long long); break;
			case PRINTF_SIZE_J_INTMAX: arg = va_arg(args, uintmax_t); break;
			case PRINTF_SIZE_Z_SIZE: arg = va_arg(args, size_t); break;
			case PRINTF_SIZE_T_PTRDIFF: arg = va_arg(args, ptrdiff_t); break;
			default: arg = va_arg(args, unsigned int); break;
			}

			int base = 10;
			bool uppercase = false;
			const char* prefix = "";
			size_t prefix_len = 0;
			switch (*(fmt - 1))
			{
			case 'o': base = 8; prefix = "0"; prefix_len = 1; break;
			case 'x': base = 16; prefix = "0x"; prefix_len = 2; break;
			case 'X': base = 16; uppercase = true; prefix = "0X"; prefix_len = 2; break;
			}

			bool is_alt = (flags & PRINTF_FLAG_ALT) != 0;
			bool has_prefix = arg != 0 && is_alt;
			size_t arg_len = unsigned_to_str(num, arg, base, uppercase);
			size_t num_zeroes = 0;
			if (has_precision)
			{
				if (precision == 0 && arg == 0 && !(base == 8 && is_alt))
					arg_len = 0;
				else if (precision > arg_len)
					num_zeroes = (size_t)precision - arg_len;
			}
			size_t arg_width = arg_len + num_zeroes + (has_prefix ? prefix_len : 0);

			if (has_prefix && left_pad_char == '0') write_s(prefix);
			write_p0();
			if (has_prefix && left_pad_char == ' ') write_s(prefix);
			write_p('0', num_zeroes);
			write_ns(num, arg_len);
			write_p1();
			break;
		}
		case 'n':
		{
			switch (size)
			{
			case PRINTF_SIZE_HH_CHAR: *va_arg(args, signed char*) = (signed char)len; break;
			case PRINTF_SIZE_H_SHORT: *va_arg(args, short*) = (short)len; break;
			case PRINTF_SIZE_L_LONG: *va_arg(args, long*) = len; break;
			case PRINTF_SIZE_LL_LONGLONG: *va_arg(args, long long*) = len; break;
			case PRINTF_SIZE_J_INTMAX: *va_arg(args, intmax_t*) = len; break;
			case PRINTF_SIZE_Z_SIZE: *va_arg(args, intptr_t*) = len; break;
			case PRINTF_SIZE_T_PTRDIFF: *va_arg(args, ptrdiff_t*) = len; break;
			default: *va_arg(args, int*) = len; break;
			}
			break;
		}
		case 'p':
		{
			void* arg = va_arg(args, void*);
			bool has_prefix = (flags & PRINTF_FLAG_ALT) == 0;

			size_t arg_len = unsigned_to_str(num, (uintptr_t)arg, 16, true);
			size_t min_zeroes = sizeof(void*) * 2;
			size_t num_zeroes = min_zeroes > arg_len ? min_zeroes - arg_len : 0;
			size_t arg_width = arg_len + num_zeroes + (has_prefix ? 2 : 0);

			left_pad_char = ' ';
			write_p0();
			if (has_prefix) write_s("0x");
			write_p('0', num_zeroes);
			write_ns(num, arg_len);
			write_p1();
			break;
		}
		default:
			write_ns(beg, fmt - beg);
			break;
		}
	}

	if (len > cap)
		buf[cap - 1] = '\0';

	return len - 1;
}

int snprintf(char* buf, size_t cap, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(buf, cap, fmt, args);
	va_end(args);
	return ret;
}
