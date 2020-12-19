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
#include "drivers/virtio_gpu.h"

extern int __kernel_beg;
extern int __kernel_end;
extern int __kernel_stack_beg;
extern int __kernel_stack_end;

static uint32_t kmain(void* ctx);
static uint32_t kmonitor(void* ctx);

void* lodepng_malloc(size_t size)
{
    return kalloc(size);
}

void* lodepng_realloc(void* ptr, size_t new_size)
{
    return krealloc(ptr, new_size);
}

void lodepng_free(void* ptr)
{
    if (ptr)
        kfree(ptr);
}

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

static void k_TestVirtioBlk(const PciDeviceInfo* info, void* ctx)
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
    for (size_t i = 0; i < 256; i++)
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

static void k_TestVirtioGpu(const PciDeviceInfo* info, void* ctx)
{
    // Create device
    DrvVirtioGpu* gpu = DrvVirtioGpu_Create(info);
    if (!gpu)
        return;

    // Initialize device
    if (!DrvVirtioGpu_Start(gpu))
        return;
}

static uint32_t kmain(void* ctx)
{
    TmPrintfInf("\nTesting PCI stuff...\n");
    PciInitialize();
  //PciRegisterDiscoverCallback(k_TestAhci, NULL);
  //PciRegisterDiscoverCallback(k_TestVirtioBlk, NULL);
    PciRegisterDiscoverCallback(k_TestVirtioGpu, NULL);
    PciDiscoverDevices();

    TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_BLUE, "hello test\n");
    TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_RED, "hello hello hello\n");
    TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_YELLOW, "hello test\n");
    TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_GREEN, "hello\n");
    TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_DKGRAY, "hello hello\n");
    TmColorPrintf(TM_COLOR_BLACK, TM_COLOR_LTGRAY, "hello test hello test hello\n");
    TmColorPrintf(TM_COLOR_BLACK, TM_COLOR_CYAN, "hello !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    TmColorPrintf(TM_COLOR_MAGENTA, TM_COLOR_RED, "hello hello\n");
    TmColorPrintf(TM_COLOR_BLUE, TM_COLOR_GREEN, "hello hello can you see test hello\n");
    TmColorPrintf(TM_COLOR_RED, TM_COLOR_YELLOW, "hello hello god\n");
    TmColorPrintf(TM_COLOR_DKGRAY, TM_COLOR_CYAN, "hello test hello hello\n");
    TmColorPrintf(TM_COLOR_WHITE, TM_COLOR_MAGENTA, "hello hello hello test hello hello\n");

    /*
    TmPrintfInf("\nTesting IRQL stuff...\n");
    IrqlSetCurrent(IRQL_DEVICE_LO);
    for (int i = 0; i < 5; i++)
    {
        TmPrintf("IRQL 0x%02X      TPR %02Xh\n", IrqlGetCurrent(), ApicGetTPR());
        SchStall(1000 * 1000);
    }
    IrqlSetCurrent(IRQL_STANDARD);
    */

    TmPrintfInf("\nTesting scheduler queue primitive...\n");
    test_Queue = SchCreateQueue();
    SchCreateTask("producer", 64*1024, test_Producer, NULL);
    SchCreateTask("consumer1", 64*1024, test_Consumer, NULL);
    SchCreateTask("consumer2", 64*1024, test_Consumer, NULL);
    SchCreateTask("consumer3", 64*1024, test_Consumer, NULL);

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
