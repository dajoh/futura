#ifndef KERNEL_LOWLEVEL_H
#define KERNEL_LOWLEVEL_H

#include <stdint.h>

static inline void cpuid(uint32_t fn, uint32_t* eax, uint32_t* edx)
{
    asm volatile("cpuid": "=a"(*eax), "=d"(*edx): "0"(fn): "ebx", "ecx");
}

static inline void cpuid2(uint32_t fn, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx)
{
    asm volatile("cpuid": "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx): "0"(fn));
}

static inline void pg_setdir(uintptr_t addr)
{
    asm volatile("mov %0, %%cr3":: "r"(addr));
}

static inline void pg_flushtlb(uintptr_t addr)
{
    asm volatile("invlpg (%0)":: "r"(addr): "memory");
}

static inline uint64_t rdtsc()
{
    uint64_t ret;
    asm volatile("rdtsc": "=A"(ret));
    return ret;
}

static inline uint64_t rdmsr(uint32_t msrId)
{
    uint64_t msrValue;
    asm volatile("rdmsr": "=A"(msrValue): "c"(msrId));
    return msrValue;
}

static inline void wrmsr(uint32_t msrId, uint64_t msrValue)
{
    asm volatile("wrmsr":: "c"(msrId), "A"(msrValue));
}

static inline void* rdcr2()
{
    void* cr2Value;
    asm volatile("mov %%cr2, %0": "=r"(cr2Value));
    return cr2Value;
}

static inline uint8_t inb(uint16_t __port)
{
    uint8_t _v;
    asm volatile("inb %w1,%0":"=a" (_v):"Nd" (__port));
    return _v;
}

static inline uint16_t inw(uint16_t __port)
{
    uint16_t _v;
    asm volatile("inw %w1,%0":"=a" (_v):"Nd" (__port));
    return _v;
}

static inline uint32_t inl(uint16_t __port)
{
    uint32_t _v;
    asm volatile("inl %w1,%0":"=a" (_v):"Nd" (__port));
    return _v;
}

static inline void outb(uint16_t __port, uint8_t __value)
{
    asm volatile("outb %b0,%w1": :"a" (__value), "Nd" (__port));
}

static inline void outw(uint16_t __port, uint16_t __value)
{
    asm volatile("outw %w0,%w1": :"a" (__value), "Nd" (__port));
}

static inline void outl(uint16_t __port, uint32_t __value)
{
    asm volatile("outl %0,%w1": :"a" (__value), "Nd" (__port));
}

static inline void io_wait()
{
    asm volatile("outb %%al, $0x80": :"a"(0));
}

#endif
