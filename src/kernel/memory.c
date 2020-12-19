#include "memory.h"
#include "textmode.h"

void MemInitialize(multiboot_info_t* info)
{
    TmPrintfInf("\nInitializing physical memory manager (early)...\n");
    PhysInitializeEarly(info);

    TmPrintfInf("\nInitializing virtual memory manager (early)...\n");
    VirtInitializeEarly();

    TmPrintfInf("\nInitializing kernel heap...\n");
    KHeapInitialize();

    TmPrintfInf("\nInitializing physical memory manager (full)...\n");
    PhysInitializeFull();

    TmPrintfInf("\nInitializing virtual memory manager (full)...\n");
    VirtInitializeFull();
}

void MemDebugDump()
{
    PhysDebugDump();
    VirtDebugDump();
}
