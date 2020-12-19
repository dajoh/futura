#include <stdint.h>
#include <acpi/acpi.h>
#include "isr.h"
#include "pit.h"
#include "pic.h"
#include "apic.h"
#include "ioapic.h"
#include "debug.h"
#include "memory.h"
#include "lowlevel.h"
#include "textmode.h"
#include "scheduler.h"
#include "interrupts.h"

#pragma pack(push, 1)
typedef struct
{
   uint32_t ds;
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
   uint32_t interrupt, errcode;
   uint32_t eip, cs, eflags, useresp, userss;
} InterruptContext;

typedef struct
{
    uint16_t OffsetLow;
    uint16_t Selector;
    uint8_t Zero;
    uint8_t Flags;
    uint16_t OffsetHigh;
} IdtEntry;
#pragma pack(pop)

typedef struct IntCallbackRecord_s
{
    SListEntry List;
    uint32_t Interrupt;
    IntCallbackFn Function;
    void* Context;
} IntCallbackRecord;

extern int __kernel_idt_beg;
static IdtEntry* IntIDT = (IdtEntry*)&__kernel_idt_beg;
static SListHead IntCallbackList;
static int IntPicMode = INT_PIC_MODE_8259;
static int IntPageFaultsDeferred = 0;
static bool IntDeferringPageFaults = false;

void IntCommonHandler(InterruptContext ctx)
{
    // This is only the case during early boot
    if (ctx.interrupt >= INT20_PIC_IRQ0 && ctx.interrupt <= INT2F_PIC_IRQ15)
    {
        PicSendEOI(ctx.interrupt - INT20_PIC_IRQ0); // TODO: don't send EOI for spurious interrupts
        if (ctx.interrupt == INT20_PIC_IRQ0 && IntPicMode == INT_PIC_MODE_8259)
            PitCurrentTick++;
        if (ctx.interrupt == INT21_PIC_IRQ1)
            inb(0x60);
        return;
    }

    // Don't send EOI for ISRFFh (spurious interrupt ISR)
    if (IntPicMode == INT_PIC_MODE_APIC && ctx.interrupt != 0xFF)
        ApicSendEOI(ctx.interrupt);

    bool handled = false;
    IntCallbackRecord* callback = CONTAINING_RECORD(IntCallbackList.Next, IntCallbackRecord, List);
    while (callback)
    {
        if (callback->Interrupt == ctx.interrupt)
        {
            callback->Function(callback->Context);
            handled = true;
        }
        callback = CONTAINING_RECORD(callback->List.Next, IntCallbackRecord, List);
    }

    if (ctx.interrupt == INT0D_CPU_GP_FAULT)
    {
        TmSetColor(TM_COLOR_LTRED, TM_COLOR_BLACK);
        TmPrintf("GENERAL PROTECTION FAULT\n");
        TmPrintf("eip=%p  eflags=%p task=%d\n", ctx.eip, ctx.eflags, SchCurrentTask ? SchCurrentTask->id : 0);
        TmPrintf("int=%p  err=%p  uesp=%p\n", ctx.interrupt, ctx.errcode, ctx.useresp);
        TmPrintf("cs=%p   ds=%p   uss=%p\n", ctx.cs, ctx.ds, ctx.userss);
        TmPrintf("eax=%p  ebx=%p  ecx=%p  edx=%p\n", ctx.eax, ctx.ebx, ctx.ecx, ctx.edx);
        TmPrintf("ebp=%p  esp=%p  edi=%p  esi=%p\n", ctx.ebp, ctx.esp, ctx.edi, ctx.esi);
        DbgPanic("GP fault");
    }
    else if (ctx.interrupt == INT0E_CPU_PAGE_FAULT)
    {
        int taskId = SchCurrentTask ? SchCurrentTask->id : 0;
        void* addr = rdcr2();
        void* page = (void*)KPAGE_ALIGN_DOWN(addr);
        if (page == NULL || !IntDeferringPageFaults)
        {
            TmPushColor(TM_COLOR_LTRED, TM_COLOR_BLACK);
            TmPrintf("PAGE FAULT: address %p\n", addr);
            TmPrintf("eip=%p  eflags=%p task=%d\n", ctx.eip, ctx.eflags, taskId);
            TmPrintf("int=%p  err=%p  uesp=%p\n", ctx.interrupt, ctx.errcode, ctx.useresp);
            TmPrintf("cs=%p   ds=%p   uss=%p\n", ctx.cs, ctx.ds, ctx.userss);
            TmPrintf("eax=%p  ebx=%p  ecx=%p  edx=%p\n", ctx.eax, ctx.ebx, ctx.ecx, ctx.edx);
            TmPrintf("ebp=%p  esp=%p  edi=%p  esi=%p\n", ctx.ebp, ctx.esp, ctx.edi, ctx.esi);
            TmPopColor();
        }
        if (page != NULL && IntDeferringPageFaults)
        {
            IntPageFaultsDeferred++;
            VirtMapMemory(KPHYS(page), KVIRT(page), 1, VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, "FAULT");
        }
        if (page == NULL)
            DbgPanic("NULL pointer accessed");
        if (!IntDeferringPageFaults)
            DbgPanic("page fault");
    }
    else if (ctx.interrupt == INTXX_APIC_IRQ0)
    {
        TmColorPrintf(TM_COLOR_YELLOW, TM_COLOR_BLACK, "[INT%02Xh] Spurious IRQ0 timer!\n", ctx.interrupt & 0xFF);
    }
    else if (ctx.interrupt == INTXX_APIC_TIMER)
    {
        PitCurrentTick++;
        if (SchCurrentTask)
            SchYield();
    }
    else if (ctx.interrupt == INTXX_APIC_IRQ1)
    {
        TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_BLACK, "[INT%02Xh] Got keyboard scancode: 0x%02X\n", ctx.interrupt & 0xFF, inb(0x60));
    }
    else if (ctx.interrupt == 0x80)
    {
        TmPutString((const char*)ctx.ebx);
    }
    else if (!handled)
    {
        TmColorPrintf(TM_COLOR_LTRED, TM_COLOR_BLACK, "[INT%02Xh] Unknown interrupt 0x%02X called (err=0x%08X)\n", ctx.interrupt & 0xFF, ctx.interrupt & 0xFF, ctx.errcode);
    }
}

void IntSetPicMode(int picMode)
{
    IntPicMode = picMode;
}

int IntGetPicMode()
{
    return IntPicMode;
}

void IntSetAcpiPicMode()
{
    // Call ACPI \_PIC method to set APIC mode
    ACPI_OBJECT param;
    param.Integer.Type = ACPI_TYPE_INTEGER;
    param.Integer.Value = IntPicMode;
    ACPI_OBJECT_LIST params = {1, &param};
    ACPI_STATUS status = AcpiEvaluateObject(NULL, "\\_PIC", &params, NULL);
    DbgAssert(ACPI_SUCCESS(status) || status == AE_NOT_FOUND);

    if (status == AE_NOT_FOUND)
        TmPrintfWrn("ACPI method '\\_PIC' could not be found\n");
}

void IntRegisterCallback(uint32_t interrupt, IntCallbackFn fn, void* ctx)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        IntCallbackRecord* record = kalloc(sizeof(IntCallbackRecord));
        record->Interrupt = interrupt;
        record->Function = fn;
        record->Context = ctx;
        SListPushFront(&IntCallbackList, &record->List);
    }
    IntLeaveCriticalSection(irqLock);
}

void IntUnregisterCallback(uint32_t interrupt, IntCallbackFn fn)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        SListEntry** prevNext = &IntCallbackList.Next;
        while (*prevNext)
        {
            IntCallbackRecord* record = CONTAINING_RECORD(*prevNext, IntCallbackRecord, List);
            if (record->Interrupt == interrupt && record->Function == fn)
            {
                *prevNext = record->List.Next;
                kfree(record);
                break;
            }
            prevNext = &record->List.Next;
        }
    }
    IntLeaveCriticalSection(irqLock);
}

void IntUnregisterCallback2(uint32_t interrupt, IntCallbackFn fn, void* ctx)
{
    uint32_t irqLock = IntEnterCriticalSection();
    {
        SListEntry** prevNext = &IntCallbackList.Next;
        while (*prevNext)
        {
            IntCallbackRecord* record = CONTAINING_RECORD(*prevNext, IntCallbackRecord, List);
            if (record->Interrupt == interrupt && record->Function == fn && record->Context == ctx)
            {
                *prevNext = record->List.Next;
                kfree(record);
                break;
            }
            prevNext = &record->List.Next;
        }
    }
    IntLeaveCriticalSection(irqLock);
}

void IntBeginDeferPageFaults()
{
    DbgAssert(!IntDeferringPageFaults);
    IntDeferringPageFaults = true;
}

void IntFinishDeferPageFaults()
{
    DbgAssert(IntDeferringPageFaults);
    IntDeferringPageFaults = false;
    if (IntPageFaultsDeferred != 0)
        TmPrintfWrn("%u page faults occurred\n", IntPageFaultsDeferred);
    IntPageFaultsDeferred = 0;
}

static void IntSetHandler(uint8_t index, void* offset, uint16_t selector, uint8_t flags)
{
    uint32_t off = (uint32_t)offset;
    IdtEntry entry;
    entry.OffsetLow = off & 0xFFFF;
    entry.Selector = selector;
    entry.Zero = 0;
    entry.Flags = flags;
    entry.OffsetHigh = off >> 16;
    IntIDT[index] = entry;
}

void IntInitialize()
{
    SListInitialize(&IntCallbackList);

    IntSetHandler(0, Isr00, 0x08, 0x8E);
    IntSetHandler(1, Isr01, 0x08, 0x8E);
    IntSetHandler(2, Isr02, 0x08, 0x8E);
    IntSetHandler(3, Isr03, 0x08, 0x8E);
    IntSetHandler(4, Isr04, 0x08, 0x8E);
    IntSetHandler(5, Isr05, 0x08, 0x8E);
    IntSetHandler(6, Isr06, 0x08, 0x8E);
    IntSetHandler(7, Isr07, 0x08, 0x8E);
    IntSetHandler(8, Isr08, 0x08, 0x8E);
    IntSetHandler(9, Isr09, 0x08, 0x8E);
    IntSetHandler(10, Isr0A, 0x08, 0x8E);
    IntSetHandler(11, Isr0B, 0x08, 0x8E);
    IntSetHandler(12, Isr0C, 0x08, 0x8E);
    IntSetHandler(13, Isr0D, 0x08, 0x8E);
    IntSetHandler(14, Isr0E, 0x08, 0x8E);
    IntSetHandler(15, Isr0F, 0x08, 0x8E);
    IntSetHandler(16, Isr10, 0x08, 0x8E);
    IntSetHandler(17, Isr11, 0x08, 0x8E);
    IntSetHandler(18, Isr12, 0x08, 0x8E);
    IntSetHandler(19, Isr13, 0x08, 0x8E);
    IntSetHandler(20, Isr14, 0x08, 0x8E);
    IntSetHandler(21, Isr15, 0x08, 0x8E);
    IntSetHandler(22, Isr16, 0x08, 0x8E);
    IntSetHandler(23, Isr17, 0x08, 0x8E);
    IntSetHandler(24, Isr18, 0x08, 0x8E);
    IntSetHandler(25, Isr19, 0x08, 0x8E);
    IntSetHandler(26, Isr1A, 0x08, 0x8E);
    IntSetHandler(27, Isr1B, 0x08, 0x8E);
    IntSetHandler(28, Isr1C, 0x08, 0x8E);
    IntSetHandler(29, Isr1D, 0x08, 0x8E);
    IntSetHandler(30, Isr1E, 0x08, 0x8E);
    IntSetHandler(31, Isr1F, 0x08, 0x8E);
    IntSetHandler(32, Isr20, 0x08, 0x8E);
    IntSetHandler(33, Isr21, 0x08, 0x8E);
    IntSetHandler(34, Isr22, 0x08, 0x8E);
    IntSetHandler(35, Isr23, 0x08, 0x8E);
    IntSetHandler(36, Isr24, 0x08, 0x8E);
    IntSetHandler(37, Isr25, 0x08, 0x8E);
    IntSetHandler(38, Isr26, 0x08, 0x8E);
    IntSetHandler(39, Isr27, 0x08, 0x8E);
    IntSetHandler(40, Isr28, 0x08, 0x8E);
    IntSetHandler(41, Isr29, 0x08, 0x8E);
    IntSetHandler(42, Isr2A, 0x08, 0x8E);
    IntSetHandler(43, Isr2B, 0x08, 0x8E);
    IntSetHandler(44, Isr2C, 0x08, 0x8E);
    IntSetHandler(45, Isr2D, 0x08, 0x8E);
    IntSetHandler(46, Isr2E, 0x08, 0x8E);
    IntSetHandler(47, Isr2F, 0x08, 0x8E);
    IntSetHandler(48, Isr30, 0x08, 0x8E);
    IntSetHandler(49, Isr31, 0x08, 0x8E);
    IntSetHandler(50, Isr32, 0x08, 0x8E);
    IntSetHandler(51, Isr33, 0x08, 0x8E);
    IntSetHandler(52, Isr34, 0x08, 0x8E);
    IntSetHandler(53, Isr35, 0x08, 0x8E);
    IntSetHandler(54, Isr36, 0x08, 0x8E);
    IntSetHandler(55, Isr37, 0x08, 0x8E);
    IntSetHandler(56, Isr38, 0x08, 0x8E);
    IntSetHandler(57, Isr39, 0x08, 0x8E);
    IntSetHandler(58, Isr3A, 0x08, 0x8E);
    IntSetHandler(59, Isr3B, 0x08, 0x8E);
    IntSetHandler(60, Isr3C, 0x08, 0x8E);
    IntSetHandler(61, Isr3D, 0x08, 0x8E);
    IntSetHandler(62, Isr3E, 0x08, 0x8E);
    IntSetHandler(63, Isr3F, 0x08, 0x8E);
    IntSetHandler(64, Isr40, 0x08, 0x8E);
    IntSetHandler(65, Isr41, 0x08, 0x8E);
    IntSetHandler(66, Isr42, 0x08, 0x8E);
    IntSetHandler(67, Isr43, 0x08, 0x8E);
    IntSetHandler(68, Isr44, 0x08, 0x8E);
    IntSetHandler(69, Isr45, 0x08, 0x8E);
    IntSetHandler(70, Isr46, 0x08, 0x8E);
    IntSetHandler(71, Isr47, 0x08, 0x8E);
    IntSetHandler(72, Isr48, 0x08, 0x8E);
    IntSetHandler(73, Isr49, 0x08, 0x8E);
    IntSetHandler(74, Isr4A, 0x08, 0x8E);
    IntSetHandler(75, Isr4B, 0x08, 0x8E);
    IntSetHandler(76, Isr4C, 0x08, 0x8E);
    IntSetHandler(77, Isr4D, 0x08, 0x8E);
    IntSetHandler(78, Isr4E, 0x08, 0x8E);
    IntSetHandler(79, Isr4F, 0x08, 0x8E);
    IntSetHandler(80, Isr50, 0x08, 0x8E);
    IntSetHandler(81, Isr51, 0x08, 0x8E);
    IntSetHandler(82, Isr52, 0x08, 0x8E);
    IntSetHandler(83, Isr53, 0x08, 0x8E);
    IntSetHandler(84, Isr54, 0x08, 0x8E);
    IntSetHandler(85, Isr55, 0x08, 0x8E);
    IntSetHandler(86, Isr56, 0x08, 0x8E);
    IntSetHandler(87, Isr57, 0x08, 0x8E);
    IntSetHandler(88, Isr58, 0x08, 0x8E);
    IntSetHandler(89, Isr59, 0x08, 0x8E);
    IntSetHandler(90, Isr5A, 0x08, 0x8E);
    IntSetHandler(91, Isr5B, 0x08, 0x8E);
    IntSetHandler(92, Isr5C, 0x08, 0x8E);
    IntSetHandler(93, Isr5D, 0x08, 0x8E);
    IntSetHandler(94, Isr5E, 0x08, 0x8E);
    IntSetHandler(95, Isr5F, 0x08, 0x8E);
    IntSetHandler(96, Isr60, 0x08, 0x8E);
    IntSetHandler(97, Isr61, 0x08, 0x8E);
    IntSetHandler(98, Isr62, 0x08, 0x8E);
    IntSetHandler(99, Isr63, 0x08, 0x8E);
    IntSetHandler(100, Isr64, 0x08, 0x8E);
    IntSetHandler(101, Isr65, 0x08, 0x8E);
    IntSetHandler(102, Isr66, 0x08, 0x8E);
    IntSetHandler(103, Isr67, 0x08, 0x8E);
    IntSetHandler(104, Isr68, 0x08, 0x8E);
    IntSetHandler(105, Isr69, 0x08, 0x8E);
    IntSetHandler(106, Isr6A, 0x08, 0x8E);
    IntSetHandler(107, Isr6B, 0x08, 0x8E);
    IntSetHandler(108, Isr6C, 0x08, 0x8E);
    IntSetHandler(109, Isr6D, 0x08, 0x8E);
    IntSetHandler(110, Isr6E, 0x08, 0x8E);
    IntSetHandler(111, Isr6F, 0x08, 0x8E);
    IntSetHandler(112, Isr70, 0x08, 0x8E);
    IntSetHandler(113, Isr71, 0x08, 0x8E);
    IntSetHandler(114, Isr72, 0x08, 0x8E);
    IntSetHandler(115, Isr73, 0x08, 0x8E);
    IntSetHandler(116, Isr74, 0x08, 0x8E);
    IntSetHandler(117, Isr75, 0x08, 0x8E);
    IntSetHandler(118, Isr76, 0x08, 0x8E);
    IntSetHandler(119, Isr77, 0x08, 0x8E);
    IntSetHandler(120, Isr78, 0x08, 0x8E);
    IntSetHandler(121, Isr79, 0x08, 0x8E);
    IntSetHandler(122, Isr7A, 0x08, 0x8E);
    IntSetHandler(123, Isr7B, 0x08, 0x8E);
    IntSetHandler(124, Isr7C, 0x08, 0x8E);
    IntSetHandler(125, Isr7D, 0x08, 0x8E);
    IntSetHandler(126, Isr7E, 0x08, 0x8E);
    IntSetHandler(127, Isr7F, 0x08, 0x8E);
    IntSetHandler(128, Isr80, 0x08, 0xEE);
    IntSetHandler(129, Isr81, 0x08, 0x8E);
    IntSetHandler(130, Isr82, 0x08, 0x8E);
    IntSetHandler(131, Isr83, 0x08, 0x8E);
    IntSetHandler(132, Isr84, 0x08, 0x8E);
    IntSetHandler(133, Isr85, 0x08, 0x8E);
    IntSetHandler(134, Isr86, 0x08, 0x8E);
    IntSetHandler(135, Isr87, 0x08, 0x8E);
    IntSetHandler(136, Isr88, 0x08, 0x8E);
    IntSetHandler(137, Isr89, 0x08, 0x8E);
    IntSetHandler(138, Isr8A, 0x08, 0x8E);
    IntSetHandler(139, Isr8B, 0x08, 0x8E);
    IntSetHandler(140, Isr8C, 0x08, 0x8E);
    IntSetHandler(141, Isr8D, 0x08, 0x8E);
    IntSetHandler(142, Isr8E, 0x08, 0x8E);
    IntSetHandler(143, Isr8F, 0x08, 0x8E);
    IntSetHandler(144, Isr90, 0x08, 0x8E);
    IntSetHandler(145, Isr91, 0x08, 0x8E);
    IntSetHandler(146, Isr92, 0x08, 0x8E);
    IntSetHandler(147, Isr93, 0x08, 0x8E);
    IntSetHandler(148, Isr94, 0x08, 0x8E);
    IntSetHandler(149, Isr95, 0x08, 0x8E);
    IntSetHandler(150, Isr96, 0x08, 0x8E);
    IntSetHandler(151, Isr97, 0x08, 0x8E);
    IntSetHandler(152, Isr98, 0x08, 0x8E);
    IntSetHandler(153, Isr99, 0x08, 0x8E);
    IntSetHandler(154, Isr9A, 0x08, 0x8E);
    IntSetHandler(155, Isr9B, 0x08, 0x8E);
    IntSetHandler(156, Isr9C, 0x08, 0x8E);
    IntSetHandler(157, Isr9D, 0x08, 0x8E);
    IntSetHandler(158, Isr9E, 0x08, 0x8E);
    IntSetHandler(159, Isr9F, 0x08, 0x8E);
    IntSetHandler(160, IsrA0, 0x08, 0x8E);
    IntSetHandler(161, IsrA1, 0x08, 0x8E);
    IntSetHandler(162, IsrA2, 0x08, 0x8E);
    IntSetHandler(163, IsrA3, 0x08, 0x8E);
    IntSetHandler(164, IsrA4, 0x08, 0x8E);
    IntSetHandler(165, IsrA5, 0x08, 0x8E);
    IntSetHandler(166, IsrA6, 0x08, 0x8E);
    IntSetHandler(167, IsrA7, 0x08, 0x8E);
    IntSetHandler(168, IsrA8, 0x08, 0x8E);
    IntSetHandler(169, IsrA9, 0x08, 0x8E);
    IntSetHandler(170, IsrAA, 0x08, 0x8E);
    IntSetHandler(171, IsrAB, 0x08, 0x8E);
    IntSetHandler(172, IsrAC, 0x08, 0x8E);
    IntSetHandler(173, IsrAD, 0x08, 0x8E);
    IntSetHandler(174, IsrAE, 0x08, 0x8E);
    IntSetHandler(175, IsrAF, 0x08, 0x8E);
    IntSetHandler(176, IsrB0, 0x08, 0x8E);
    IntSetHandler(177, IsrB1, 0x08, 0x8E);
    IntSetHandler(178, IsrB2, 0x08, 0x8E);
    IntSetHandler(179, IsrB3, 0x08, 0x8E);
    IntSetHandler(180, IsrB4, 0x08, 0x8E);
    IntSetHandler(181, IsrB5, 0x08, 0x8E);
    IntSetHandler(182, IsrB6, 0x08, 0x8E);
    IntSetHandler(183, IsrB7, 0x08, 0x8E);
    IntSetHandler(184, IsrB8, 0x08, 0x8E);
    IntSetHandler(185, IsrB9, 0x08, 0x8E);
    IntSetHandler(186, IsrBA, 0x08, 0x8E);
    IntSetHandler(187, IsrBB, 0x08, 0x8E);
    IntSetHandler(188, IsrBC, 0x08, 0x8E);
    IntSetHandler(189, IsrBD, 0x08, 0x8E);
    IntSetHandler(190, IsrBE, 0x08, 0x8E);
    IntSetHandler(191, IsrBF, 0x08, 0x8E);
    IntSetHandler(192, IsrC0, 0x08, 0x8E);
    IntSetHandler(193, IsrC1, 0x08, 0x8E);
    IntSetHandler(194, IsrC2, 0x08, 0x8E);
    IntSetHandler(195, IsrC3, 0x08, 0x8E);
    IntSetHandler(196, IsrC4, 0x08, 0x8E);
    IntSetHandler(197, IsrC5, 0x08, 0x8E);
    IntSetHandler(198, IsrC6, 0x08, 0x8E);
    IntSetHandler(199, IsrC7, 0x08, 0x8E);
    IntSetHandler(200, IsrC8, 0x08, 0x8E);
    IntSetHandler(201, IsrC9, 0x08, 0x8E);
    IntSetHandler(202, IsrCA, 0x08, 0x8E);
    IntSetHandler(203, IsrCB, 0x08, 0x8E);
    IntSetHandler(204, IsrCC, 0x08, 0x8E);
    IntSetHandler(205, IsrCD, 0x08, 0x8E);
    IntSetHandler(206, IsrCE, 0x08, 0x8E);
    IntSetHandler(207, IsrCF, 0x08, 0x8E);
    IntSetHandler(208, IsrD0, 0x08, 0x8E);
    IntSetHandler(209, IsrD1, 0x08, 0x8E);
    IntSetHandler(210, IsrD2, 0x08, 0x8E);
    IntSetHandler(211, IsrD3, 0x08, 0x8E);
    IntSetHandler(212, IsrD4, 0x08, 0x8E);
    IntSetHandler(213, IsrD5, 0x08, 0x8E);
    IntSetHandler(214, IsrD6, 0x08, 0x8E);
    IntSetHandler(215, IsrD7, 0x08, 0x8E);
    IntSetHandler(216, IsrD8, 0x08, 0x8E);
    IntSetHandler(217, IsrD9, 0x08, 0x8E);
    IntSetHandler(218, IsrDA, 0x08, 0x8E);
    IntSetHandler(219, IsrDB, 0x08, 0x8E);
    IntSetHandler(220, IsrDC, 0x08, 0x8E);
    IntSetHandler(221, IsrDD, 0x08, 0x8E);
    IntSetHandler(222, IsrDE, 0x08, 0x8E);
    IntSetHandler(223, IsrDF, 0x08, 0x8E);
    IntSetHandler(224, IsrE0, 0x08, 0x8E);
    IntSetHandler(225, IsrE1, 0x08, 0x8E);
    IntSetHandler(226, IsrE2, 0x08, 0x8E);
    IntSetHandler(227, IsrE3, 0x08, 0x8E);
    IntSetHandler(228, IsrE4, 0x08, 0x8E);
    IntSetHandler(229, IsrE5, 0x08, 0x8E);
    IntSetHandler(230, IsrE6, 0x08, 0x8E);
    IntSetHandler(231, IsrE7, 0x08, 0x8E);
    IntSetHandler(232, IsrE8, 0x08, 0x8E);
    IntSetHandler(233, IsrE9, 0x08, 0x8E);
    IntSetHandler(234, IsrEA, 0x08, 0x8E);
    IntSetHandler(235, IsrEB, 0x08, 0x8E);
    IntSetHandler(236, IsrEC, 0x08, 0x8E);
    IntSetHandler(237, IsrED, 0x08, 0x8E);
    IntSetHandler(238, IsrEE, 0x08, 0x8E);
    IntSetHandler(239, IsrEF, 0x08, 0x8E);
    IntSetHandler(240, IsrF0, 0x08, 0x8E);
    IntSetHandler(241, IsrF1, 0x08, 0x8E);
    IntSetHandler(242, IsrF2, 0x08, 0x8E);
    IntSetHandler(243, IsrF3, 0x08, 0x8E);
    IntSetHandler(244, IsrF4, 0x08, 0x8E);
    IntSetHandler(245, IsrF5, 0x08, 0x8E);
    IntSetHandler(246, IsrF6, 0x08, 0x8E);
    IntSetHandler(247, IsrF7, 0x08, 0x8E);
    IntSetHandler(248, IsrF8, 0x08, 0x8E);
    IntSetHandler(249, IsrF9, 0x08, 0x8E);
    IntSetHandler(250, IsrFA, 0x08, 0x8E);
    IntSetHandler(251, IsrFB, 0x08, 0x8E);
    IntSetHandler(252, IsrFC, 0x08, 0x8E);
    IntSetHandler(253, IsrFD, 0x08, 0x8E);
    IntSetHandler(254, IsrFE, 0x08, 0x8E);
    IntSetHandler(255, IsrFF, 0x08, 0x8E);
}
