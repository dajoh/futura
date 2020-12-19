#include "irql.h"
#include "apic.h"
#include "debug.h"
#include "textmode.h"
#include "interrupts.h"

irql_t IrqlCurrent = IRQL_STANDARD;

static const char* IrqlName(irql_t irql)
{
    switch (irql)
    {
    case IRQL_STANDARD:  return "IRQL_STANDARD";
    case IRQL_SCHEDULER: return "IRQL_SCHEDULER";
    case IRQL_DEVICE_LO: return "IRQL_DEVICE_LO";
    case IRQL_DEVICE_HI: return "IRQL_DEVICE_HI";
    case IRQL_EXCLUSIVE: return "IRQL_EXCLUSIVE";
    default:
        DbgUnreachableCase(irql);
        return NULL;
    }
}

irql_t IrqlSetCurrent(irql_t level)
{
    DbgAssert(level >= IRQL_STANDARD && level <= IRQL_EXCLUSIVE);
    uint32_t lock = IntEnterCriticalSection();
    irql_t oldLevel = IrqlCurrent;
    if (oldLevel != level)
    {
        TmPrintfDbg("Switching to %s from %s\n", IrqlName(level), IrqlName(oldLevel));
        ApicSetTPR(level << 4);
        IrqlCurrent = level;
    }
    IntLeaveCriticalSection(lock);
    return oldLevel;
}
