#include <string.h>
#include <acpi/acpi.h>
#include "pci.h"
#include "pit.h"
#include "tsc.h"
#include "pic.h"
#include "irql.h"
#include "apic.h"
#include "ioapic.h"
#include "ahci.h"
#include "debug.h"
#include "memory.h"
#include "comport.h"
#include "lowlevel.h"
#include "textmode.h"
#include "scheduler.h"
#include "interrupts.h"
#include "drivers/virtio_blk.h"

#include <elf.h>
#include "embedded.h"

extern int __kernel_beg;
extern int __kernel_end;
extern int __kernel_stack_beg;
extern int __kernel_stack_end;

static uint32_t kmain(void* ctx);
static uint32_t kmonitor(void* ctx);

typedef int (*umain_t)();

void kinit(uint32_t magic, multiboot_info_t* info)
{
    // Initialize text mode
    ComInitialize();
    TmInitialize();
    TmClear();
    TmPushColor(TM_COLOR_WHITE, TM_COLOR_RED);
    TmPrintf("  __       _                   \n");
    TmPrintf(" / _|     | |                  \n");
    TmPrintf("| |_ _   _| |_ _   _ _ __ __ _ \n");
    TmPrintf("|  _| | | | __| | | | '__/ _` |\n");
    TmPrintf("| | | |_| | |_| |_| | | | (_| |\n");
    TmPrintf("|_|  \\__,_|\\__|\\__,_|_|  \\__,_|\n\n");
    TmPopColor();

    // Check magic
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        TmPrintfErr("Invalid multiboot magic!\n");
        return;
    }

    // Check for memory map
    if ((info->flags & MULTIBOOT_INFO_MEM_MAP) == 0)
    {
        TmPrintfErr("No multiboot memory map!\n");
        return;
    }

    // Print kernel/multiboot info
    TmPrintf("Kernel boot parameters:\n");
    TmPrintf("* Kernel start:              %p\n", &__kernel_beg);
    TmPrintf("* Kernel end:                %p\n", &__kernel_end);
    TmPrintf("* Kernel stack start:        %p\n", &__kernel_stack_beg);
    TmPrintf("* Kernel stack end:          %p\n", &__kernel_stack_end);
    TmPrintf("* Multiboot info at:         %p\n", info);
    TmPrintf("* Multiboot info flags:      0x%08X\n", info->flags);
    TmPrintf("* Multiboot memory map at:   0x%08X\n", info->mmap_addr);
    TmPrintf("* Multiboot memory map size: %u bytes\n", info->mmap_length);

    TmPrintfInf("\nInitializing interrupt handling...\n");
    IntInitialize();

    TmPrintfInf("\nInitializing the PIC...\n");
    PicInitialize();

    TmPrintfInf("\nInitializing the PIT and TSC...\n");
    PitInitialize(100);
    TscInitialize();

    TmPrintfInf("\nInitializing memory manager...\n");
    MemInitialize(info);

    TmPrintfInf("\nInitializing ACPI tables...\n");
    DbgAssert(ACPI_SUCCESS(AcpiInitializeTables(NULL, 16, FALSE)));

    TmPrintfInf("\nInitializing local APIC...\n");
    ApicInitialize();

    TmPrintfInf("\nInitializing I/O APIC...\n");
    IoApicInitialize();

    TmPrintfInf("\nInitializing scheduler...\n");
    SchTask* kidleTask = SchInitialize("kidle");

    TmPrintfDbg("\nEnabling interrupts!\n");
    IntEnableIRQs();

    TmPrintfInf("\nInitializing ACPI...\n");
    DbgAssert(ACPI_SUCCESS(AcpiInitializeSubsystem()));
    DbgAssert(ACPI_SUCCESS(AcpiLoadTables()));
    DbgAssert(ACPI_SUCCESS(AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION)));
    DbgAssert(ACPI_SUCCESS(AcpiInitializeObjects(ACPI_FULL_INITIALIZATION)));
    IntSetAcpiPicMode();

    TmPrintfInf("\nStarting kernel tasks...\n");
    SchTask* kmainTask = SchCreateTask("kmain", 1024*1024, kmain, NULL);
    SchTask* kmonitorTask = SchCreateTask("kmonitor", 32*1024, kmonitor, NULL);

    // Hang/idle
    while (true)
        asm volatile("hlt");
}

static void k_TestAhci(const PciDeviceInfo* info, void* ctx)
{
    if (info->BaseClass != 0x01 || info->SubClass != 0x06)
        return;

    uint32_t bus = info->Bus;
    uint32_t device = info->Device;
    uint32_t function = info->Function;

    TmPrintf("[!] SATA AHCI controller found:\n");
    TmPrintf("  BAR0: %08X\n", PciReadLong(bus, device, function, PCI_OFFSET_BAR0));
    TmPrintf("  BAR1: %08X\n", PciReadLong(bus, device, function, PCI_OFFSET_BAR1));
    TmPrintf("  BAR2: %08X\n", PciReadLong(bus, device, function, PCI_OFFSET_BAR2));
    TmPrintf("  BAR3: %08X\n", PciReadLong(bus, device, function, PCI_OFFSET_BAR3));
    TmPrintf("  BAR4: %08X\n", PciReadLong(bus, device, function, PCI_OFFSET_BAR4));
    TmPrintf("  BAR5: %08X (ABAR)\n", PciReadLong(bus, device, function, PCI_OFFSET_BAR5));

    /*AhciCtrl* ahci = (AhciCtrl*)PciReadLong(bus, device, function, PCI_OFFSET_BAR5);
    VmmMapMemory((void*)ahci, (void*)ahci, 2, VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, "AHCI");*/
}

static void k_TestVirtio(const PciDeviceInfo* info, void* ctx)
{
    // Create device
    DrvVirtioBlk* drv = DrvVirtioBlk_Create(info);
    if (!drv)
        return;

    // Initialize device
    if (!DrvVirtioBlk_Start(drv))
        return;

    TmPrintf("\n\n\n\n\n\n\n\n");

    // ---------------------------------------------
    // Read a couple of sectors synchronously
    // ---------------------------------------------
    {
        uint8_t buf[512];
        DbgAssert(DrvVirtioBlk_Read(drv, 0, buf, sizeof(buf)) == sizeof(buf)); DbgHexdump(buf, 32); TmPutChar('\n');
        DbgAssert(DrvVirtioBlk_Read(drv, 1, buf, sizeof(buf)) == sizeof(buf)); DbgHexdump(buf, 32); TmPutChar('\n');
    }

    // ---------------------------------------------
    // Read a sector asynchronously
    // ---------------------------------------------
    {
        // Prepare async call structure   
        AsyncCall asyncCall;
        memset(&asyncCall, 0, sizeof(AsyncCall));
        asyncCall.Event = SchCreateEvent();

        // Submit async operation
        uint8_t buf[512];
        DrvVirtioBlk_ReadAsync(drv, 0, buf, sizeof(buf), &asyncCall, NULL);

        // Wait operation to complete
        SchEventWait(asyncCall.Event);

        // Inspect result
        DbgAssert(asyncCall.Success && asyncCall.Transferred == sizeof(buf));
        DbgHexdump(buf, 32); TmPutChar('\n');
    }
}

typedef struct TestEntry
{
    ListEntry Queue;
    int Number;
} TestEntry;

static SchQueue* test_Queue = NULL;

static uint32_t test_Producer(void* ctx)
{
    for (size_t i = 0; i < 30; i++)
    {
        for (size_t j = 0; j < 2; j++)
        {
            int number = (i + 1) * 10 + j;

            TestEntry* entry = kalloc(sizeof(TestEntry));
            entry->Number = number;
            SchQueuePush(test_Queue, &entry->Queue);
            //TmPrintf("[T%u] Producer pushed entry: %d\n", SchCurrentTask->id, number);
        }

        SchSleep(1000);
    }
    return 0;
}

static uint32_t test_Consumer(void* ctx)
{
    for (size_t i = 0; i < 30; i++)
    {
        TestEntry* entry = CONTAINING_RECORD(SchQueuePop(test_Queue), TestEntry, Queue);
        TmPrintf("[T%u] Consumer popped entry: %d\n", SchCurrentTask->id, entry->Number);
        kfree(entry);
        SchSleep(250);
    }
    return 0;
}

#pragma pack(push, 1)
typedef struct tss
{
   uint32_t prev_tss;
   uint32_t esp0; // The stack pointer to load when we change to kernel mode.
   uint32_t ss0; // The stack segment to load when we change to kernel mode.
   uint32_t esp1;
   uint32_t ss1;
   uint32_t esp2;
   uint32_t ss2;
   uint32_t cr3;
   uint32_t eip;
   uint32_t eflags;
   uint32_t eax;
   uint32_t ecx;
   uint32_t edx;
   uint32_t ebx;
   uint32_t esp;
   uint32_t ebp;
   uint32_t esi;
   uint32_t edi;
   uint32_t es;         
   uint32_t cs;        
   uint32_t ss;        
   uint32_t ds;        
   uint32_t fs;       
   uint32_t gs;         
   uint32_t ldt;      
   uint16_t trap;
   uint16_t iomap_base;
} tss_t;
#pragma pack(pop)

extern int __kernel_gdt_beg;
uint64_t* __kernel_gdt_tss = ((uint64_t*)&__kernel_gdt_beg) + 5;
uint64_t create_tss_gdt_entry(uint32_t base, uint32_t limit, uint16_t flag)
{
    uint64_t descriptor;
    descriptor  =  limit       & 0x000F0000; // set limit bits 19:16
    descriptor |= (flag <<  8) & 0x00F0FF00; // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (base >> 16) & 0x000000FF; // set base bits 23:16
    descriptor |=  base        & 0xFF000000; // set base bits 31:24
    descriptor <<= 32;
    descriptor |= base  << 16;               // set base bits 15:0
    descriptor |= limit  & 0x0000FFFF;       // set limit bits 15:0
    return descriptor;
}

static void ktest_userspace()
{
    // Create new virtual address space for usermode process
    VirtSpace* userspace = VirtSpaceCreate();

    // Load executable ELF image into user address space
    Elf32_Ehdr* elfHdr = (Elf32_Ehdr*)bin_app_elf;
    Elf32_Phdr* progHdr = (Elf32_Phdr*)(bin_app_elf + elfHdr->e_phoff);
    for (size_t i = 0; i < elfHdr->e_phnum; i++)
    {
        if (progHdr->p_type == PT_LOAD)
        {
            TmPrintf("OFF:%8X VIRT:%8X PHYS:%8X SIZE:%u MSIZE:%u ALIGN:%u\n", progHdr->p_offset, progHdr->p_vaddr, progHdr->p_paddr, progHdr->p_filesz, progHdr->p_memsz, progHdr->p_align);

            size_t pages = KPAGE_COUNT(progHdr->p_memsz);
            kphys_t phys = PhysAlloc(pages, PHYS_REGION_TYPE_USER_IMAGE, "uimage");
            VirtSpaceMap(userspace, phys, progHdr->p_vaddr, pages, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_USER_IMAGE, "uimage");

            void* kvirt = VirtAlloc(phys, pages, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_USER_IMAGE, "uimage-tmp");
            memset(kvirt, 0, pages * KPAGE_SIZE);
            memcpy(kvirt, bin_app_elf + progHdr->p_offset, progHdr->p_filesz);
            VirtFree(kvirt);
        }

        progHdr = (Elf32_Phdr*)(bin_app_elf + elfHdr->e_phoff + i * elfHdr->e_phentsize);
    }

    // Create kernel/user stack
    size_t kuStackSize = 64*1024;
    size_t kuStackPages = kuStackSize / KPAGE_SIZE;
    kphys_t kuStackPhys = PhysAlloc(kuStackPages, PHYS_REGION_TYPE_KERNEL_USER_STACK, "ku-stack");
    uint8_t* kuStack = VirtAlloc(kuStackPhys, kuStackPages, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_KERNEL_USER_STACK, "ku-stack");
    uint8_t* kuStackEnd = kuStack + kuStackSize;

    // Create TSS
    extern PageDirectory* VirtPageDirectory;
    tss_t* tss = kcalloc(sizeof(tss_t));
    tss->esp0 = KVIRT(kuStackEnd);
    tss->ss0 = 0x10;
    tss->cr3 = KEARLY_VIRT_TO_PHYS(VirtPageDirectory);

    // Load TSS
    *__kernel_gdt_tss = create_tss_gdt_entry((uintptr_t)tss, sizeof(tss_t) - 1, 0xE9);
    extern void kflushtss();
    kflushtss();
    
    // Jump to usermode
    TmPrintfDbg("Starting userspace program bin/app.elf!\n");
    VirtSpaceActivate(userspace);
    {
        extern void kenterusermode(uintptr_t entrypoint);
        kenterusermode(elfHdr->e_entry);
    }
    VirtSpaceActivate(NULL);

    // TODO: We haven't left usermode though...
    TmPrintfWrn("Back in kernel!\n");
}

static uint32_t kmain(void* ctx)
{
    ktest_userspace();
    DbgPanic("ktest_userspace() finished");

    /*
    // Test PCI stuff
    TmPrintfInf("\nTesting PCI stuff...\n");
    PciInitialize();
  //PciRegisterDiscoverCallback(k_TestAhci, NULL);
  //PciRegisterDiscoverCallback(k_TestVirtio, NULL);
    PciDiscoverDevices();
    SchSleep(5000);

    TmPrintfInf("\nTesting IRQL stuff...\n");
    IrqlSetCurrent(IRQL_DEVICE_LO);
    for (int i = 0; i < 5; i++)
    {
        TmPrintf("IRQL 0x%02X      TPR %02Xh\n", IrqlGetCurrent(), ApicGetTPR());
        SchStall(1000 * 1000);
    }
    IrqlSetCurrent(IRQL_STANDARD);

    TmPrintfInf("\nTesting scheduler queue primitive...\n");
    test_Queue = SchCreateQueue();
    SchCreateTask("producer", 64*1024, test_Producer, NULL);
    SchCreateTask("consumer1", 64*1024, test_Consumer, NULL);
    SchCreateTask("consumer2", 64*1024, test_Consumer, NULL);
    SchCreateTask("consumer3", 64*1024, test_Consumer, NULL);
    */

    return 0;
}

static uint32_t kmonitor(void* ctx)
{
    SchSleep(10 * 1000);
    while (true)
    {
        uint32_t irqLock = IntEnterCriticalSection();
        TmPushColor(TM_COLOR_LTBLUE, TM_COLOR_BLACK);
        {
            TmPrintf("    T%llu    ACTIVE TASKS:    ", PitCurrentTick);
            SchTask* task = SchCurrentTask;
            do
            {
                TmPrintf(task->next == SchCurrentTask ? "%s\n" : "%s, ", task->name);
                task = task->next;
            } while (task != SchCurrentTask);
        }
        TmPopColor();
        IntLeaveCriticalSection(irqLock);
        SchSleep(2500);
    }

    return 0;
}
