#ifndef _FUTURA
#define _FUTURA
#endif

#include <acpi/acpi.h>
#include "pci.h"
#include "pit.h"
#include "debug.h"
#include "memory.h"
#include "lowlevel.h"
#include "textmode.h"
#include "scheduler.h"
#include "interrupts.h"

#ifdef ACPIOSL_DBG
#define DbgPrintf(...) TmPrintf(__VA_ARGS__)
#else
#define DbgPrintf(...)
#endif

/*
 * OSL Initialization and shutdown primitives
 */
ACPI_STATUS AcpiOsInitialize()
{
    DbgPrintf("AcpiOsInitialize()\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate()
{
    DbgPrintf("AcpiOsTerminate()\n");
    return AE_OK;
}

/*
 * ACPI Table interfaces
 */
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    ACPI_PHYSICAL_ADDRESS ret = 0;
	AcpiFindRootPointer(&ret);
    DbgPrintf("AcpiOsGetRootPointer() => %p\n", ret);
	return ret;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal, ACPI_STRING *NewVal)
{
    DbgPrintf("AcpiOsPredefinedOverride(%p, %p)\n", InitVal, NewVal);
    *NewVal = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
    DbgPrintf("AcpiOsTableOverride(%p, %p)\n", ExistingTable, NewTable);
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
    DbgPrintf("AcpiOsPhysicalTableOverride(%p, %p, %p)\n", ExistingTable, NewAddress, NewTableLength);
    *NewAddress = 0;
    *NewTableLength = 0;
    return AE_OK;
}

/*
 * Spinlock primitives
 */
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
    SchSpinlock* spinlock = SchCreateSpinlock();
    DbgPrintf("AcpiOsCreateLock(%p) => %p\n", OutHandle, spinlock);
    *OutHandle = spinlock;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    DbgPrintf("AcpiOsDeleteLock(%p)\n", Handle);
    SchDestroySpinlock((SchSpinlock*)Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    DbgPrintf("AcpiOsAcquireLock(%p)\n", Handle);
    SchSpinlockLock((SchSpinlock*)Handle);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    DbgPrintf("AcpiOsReleaseLock(%p, %u)\n", Handle, Flags);
    SchSpinlockUnlock((SchSpinlock*)Handle);
}

/*
 * Semaphore primitives
 */
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
    SchSemaphore* semaphore = SchCreateSemaphore(MaxUnits, InitialUnits);
    DbgPrintf("AcpiOsCreateSemaphore(%u, %u, %p) => %p\n", MaxUnits, InitialUnits, OutHandle, semaphore);
    *OutHandle = semaphore;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    DbgPrintf("AcpiOsDeleteSemaphore(%p)\n", Handle);
    SchDestroySemaphore((SchSemaphore*)Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    DbgPrintf("AcpiOsWaitSemaphore(%p, %u, %u)\n", Handle, Units, Timeout);
    if (!SchSemaphoreTryWait((SchSemaphore*)Handle, Timeout == UINT16_MAX ? SCH_INFINITE : Timeout))
        return AE_TIME;
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    DbgPrintf("AcpiOsSignalSemaphore(%p, %u)\n", Handle, Units);
    SchSemaphoreSignal((SchSemaphore*)Handle, Units);
    return AE_OK;
}

/*
 * Mutex primitives.
 */
ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle)
{
    SchMutex* mutex = SchCreateMutex();
    DbgPrintf("AcpiOsCreateMutex(%p) => %p\n", OutHandle, mutex);
    *OutHandle = mutex;
    return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle)
{
    DbgPrintf("AcpiOsDeleteMutex(%p)\n", Handle);
    SchDestroyMutex((SchMutex*)Handle);
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout)
{
    DbgPrintf("AcpiOsAcquireMutex(%p, %u)\n", Handle, Timeout);
    if (Handle != NULL)
        SchMutexTryLock((SchMutex*)Handle, Timeout == UINT16_MAX ? SCH_INFINITE : Timeout);
    return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle)
{
    DbgPrintf("AcpiOsReleaseMutex(%p)\n", Handle);
    if (Handle != NULL)
        SchMutexUnlock((SchMutex*)Handle);
}

/*
 * Memory allocation and mapping
 */
void* AcpiOsAllocate(ACPI_SIZE Size)
{
    void* ret = kalloc(Size);
    DbgPrintf("AcpiOsAllocate(%u) => %p\n", Size, ret);
    return ret;
}

void AcpiOsFree(void* Memory)
{
    DbgPrintf("AcpiOsFree(%p)\n", Memory);
    kfree(Memory);
}

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    DbgPrintf("AcpiOsMapMemory(%p, %u)\n", (uintptr_t)Where, Length);
    DbgAssert(Where < UINT32_MAX);

    if (Length < 1)
        Length = 1;

    kphys_t page = KPAGE_ALIGN_DOWN(Where);
    size_t offset = KPHYS(Where) - page;
    uint8_t* virt = VirtAlloc(page, KPAGE_COUNT(Length), VIRT_PROT_READWRITE | VIRT_PROT_NOCACHE, VIRT_REGION_TYPE_ACPI, "ACPI");
    DbgAssert(virt != NULL);
    return virt + offset;
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
    DbgPrintf("AcpiOsUnmapMemory(%p, %u)\n", LogicalAddress, Size);
    VirtFree(LogicalAddress);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    DbgPrintf("AcpiOsGetPhysicalAddress(%p)\n", LogicalAddress);
    *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)VirtToPhys(LogicalAddress);
    return AE_OK;
}

/*
 * Interrupt handlers
 */
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context)
{
    DbgPrintf("AcpiOsInstallInterruptHandler(%02Xh, %p, %p)\n", InterruptNumber, ServiceRoutine, Context);
    IntRegisterCallback(InterruptNumber, (IntCallbackFn)ServiceRoutine, Context);
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine)
{
    DbgPrintf("AcpiOsRemoveInterruptHandler(%02Xh, %p)\n", InterruptNumber, ServiceRoutine);
    IntUnregisterCallback(InterruptNumber, (IntCallbackFn)ServiceRoutine);
    return AE_OK;
}

/*
 * Threads and Scheduling
 */
ACPI_THREAD_ID AcpiOsGetThreadId()
{
    return SchCurrentTask ? SchCurrentTask->id : 0;
}

typedef struct AcpiTaskInfo_s
{
    ACPI_OSD_EXEC_CALLBACK Function;
    void* Context;
} AcpiTaskInfo;

static uint32_t AcpiNumActiveTasks = 0;
static uint32_t AcpiTaskWrapper(void* ctx)
{
    AcpiTaskInfo* info = (AcpiTaskInfo*)ctx;
    info->Function(info->Context);
    kfree(info);
    AcpiNumActiveTasks--;
    return 0;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    AcpiNumActiveTasks++;
    AcpiTaskInfo* info = kalloc(sizeof(AcpiTaskInfo));
    info->Function = Function;
    info->Context = Context;
    SchTask* task = SchCreateTask("AcpiExec", 64*1024, AcpiTaskWrapper, info);
    DbgPrintf("AcpiOsExecute(%u, %p, %p) => task %u\n", Type, Function, Context, task->id);
    return AE_OK;
}

void AcpiOsWaitEventsComplete()
{
    uint32_t tasks = AcpiNumActiveTasks;
    if (tasks != 0)
    {
        while (AcpiNumActiveTasks != 0)
            asm volatile("hlt");
        DbgPrintf("AcpiOsWaitEventsComplete() waited for %u tasks\n", tasks);
    }
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    DbgPrintf("AcpiOsSleep(%llu millisecs)\n", Milliseconds);
    SchSleep(Milliseconds);
}

void AcpiOsStall(UINT32 Microseconds)
{
    DbgPrintf("AcpiOsStall(%u microsecs)\n", Microseconds);
    SchStall(Microseconds);
}

/*
 * Platform and hardware-independent I/O interfaces
 */
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
    switch (Width)
    {
    case 8:  *Value = inb(Address); /*DbgPrintf("AcpiOsReadPort(%04X) => byte %02X\n", Address, *Value);*/ return AE_OK;
    case 16: *Value = inw(Address); /*DbgPrintf("AcpiOsReadPort(%04X) => word %04X\n", Address, *Value);*/ return AE_OK;
    case 32: *Value = inl(Address); /*DbgPrintf("AcpiOsReadPort(%04X) => long %08X\n", Address, *Value);*/ return AE_OK;
    default:
        return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    switch (Width)
    {
    case 8:  /*DbgPrintf("AcpiOsWritePort(%04X, byte %02X)\n", Address, Value);*/ outb(Address, Value); return AE_OK;
    case 16: /*DbgPrintf("AcpiOsWritePort(%04X, word %04X)\n", Address, Value);*/ outw(Address, Value); return AE_OK;
    case 32: /*DbgPrintf("AcpiOsWritePort(%04X, long %08X)\n", Address, Value);*/ outl(Address, Value); return AE_OK;
    default:
        return AE_BAD_PARAMETER;
    }
}

/*
 * Platform and hardware-independent physical memory interfaces
 */
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width)
{
    void* virt = AcpiOsMapMemory(Address, Width / 8);
    if (virt == NULL)
        return AE_NOT_EXIST;

    ACPI_STATUS ret = AE_OK;
    switch (Width) {
    case 8: *Value = *(volatile uint8_t*)virt; break;
    case 16: *Value = *(volatile uint16_t*)virt; break;
    case 32: *Value = *(volatile uint32_t*)virt; break;
    case 64: *Value = *(volatile uint64_t*)virt; break;
    default: ret = AE_BAD_PARAMETER;
    }
    AcpiOsUnmapMemory(virt, Width / 8);
    return ret;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
	void* virt = AcpiOsMapMemory(Address, Width / 8);
	if (virt == NULL)
		return AE_NOT_FOUND;

	ACPI_STATUS ret = AE_OK;
	switch (Width) {
	case 8: *(volatile uint8_t*)virt = Value; break;
	case 16: *(volatile uint16_t*)virt = Value; break;
	case 32: *(volatile uint32_t*)virt = Value; break;
	case 64: *(volatile uint64_t*)virt = Value; break;
	default: ret = AE_BAD_PARAMETER;
	}
	AcpiOsUnmapMemory(virt, Width / 8);
	return ret;
}

/*
 * Platform and hardware-independent PCI configuration space access
 */
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width)
{
    uint32_t addr = 0x80000000 | (PciId->Bus << 16) | (PciId->Device << 11) | (PciId->Function << 8) | (Reg & 0xFC);
    switch (Width)
    {
    case 8:  *Value = PciReadByte(PciId->Bus, PciId->Device, PciId->Function, Reg); DbgPrintf("AcpiReadPci(%08X) => byte %02X\n", addr, *Value); return AE_OK;
    case 16: *Value = PciReadWord(PciId->Bus, PciId->Device, PciId->Function, Reg); DbgPrintf("AcpiReadPci(%08X) => word %04X\n", addr, *Value); return AE_OK;
    case 32: *Value = PciReadLong(PciId->Bus, PciId->Device, PciId->Function, Reg); DbgPrintf("AcpiReadPci(%08X) => long %08X\n", addr, *Value); return AE_OK;
    default:
        DbgUnreachableCase(Width);
        return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
    uint32_t addr = 0x80000000 | (PciId->Bus << 16) | (PciId->Device << 11) | (PciId->Function << 8) | (Reg & 0xFC);
    switch (Width)
    {
    case 8:  PciWriteByte(PciId->Bus, PciId->Device, PciId->Function, Reg, Value); DbgPrintf("AcpiWritePci(%08X, byte %02X)\n", addr, Value); return AE_OK;
    case 16: PciWriteWord(PciId->Bus, PciId->Device, PciId->Function, Reg, Value); DbgPrintf("AcpiWritePci(%08X, word %04X)\n", addr, Value); return AE_OK;
    case 32: PciWriteLong(PciId->Bus, PciId->Device, PciId->Function, Reg, Value); DbgPrintf("AcpiWritePci(%08X, long %08X)\n", addr, Value); return AE_OK;
    default:
        DbgUnreachableCase(Width);
        return AE_BAD_PARAMETER;
    }
}

/*
 * Miscellaneous
 */
UINT64 AcpiOsGetTimer()
{
    return PitTicksToMs(PitCurrentTick) * 10000; // number of 100 nanosecs
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info)
{
    DbgPanic("AcpiOsSignal not implemented");
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
    return AE_OK;
}

/*
 * Debug print routines
 */
void AcpiOsPrintf(const char *Format, ...)
{
    va_list args;
    va_start(args, Format);
    TmVPrintf(Format, args);
    va_end(args);
}

void AcpiOsVprintf(const char *Format, va_list Args)
{
    TmVPrintf(Format, Args);
}
