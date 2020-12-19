#include <stdint.h>

static inline uint32_t syscall(uint32_t func, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a" (ret)
        : "0"(func), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory"
    );
    return ret;
}

static void syscall_puts(const char* str)
{
    syscall(666, (uint32_t)str, 0, 0);
}

int main()
{
    syscall_puts("hello, world from userspace 1\n");
    syscall_puts("hello, world from userspace 2\n");
    syscall_puts("hello, world from userspace 3\n");
    syscall_puts("hello, world from userspace 4\n");
    return 0;
}
