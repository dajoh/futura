#include <string.h>
#include "pit.h"
#include "tsc.h"
#include "debug.h"
#include "memory.h"
#include "textmode.h"
#include "scheduler.h"
#include "interrupts.h"

SchTask SchKernelTask = {0};
SchTask* SchCurrentTask = NULL;
SchTask* SchFirstSleepTask = NULL;
SListHead SchDeadTaskListHead;
static uint32_t SchNextTaskId = 1;

static void SchRunListInsert(SchTask* task)
{
    task->next = SchKernelTask.next;
    SchKernelTask.next = task;
}

static SchTask* SchRunListRemove(SchTask* task)
{
    SchTask* next = task->next;
    SchTask* prev = &SchKernelTask;
    while (prev->next != task)
        prev = prev->next;
    prev->next = next;
    task->next = NULL;
    return next;
}

static void SchSleepListInsert(SchTask* task, uint64_t sleepUntil)
{
    task->sleepUntil = sleepUntil;

    SchTask** prevNext = &SchFirstSleepTask;
    SchTask* entry = SchFirstSleepTask;
    while (entry)
    {
        if (task->sleepUntil < entry->sleepUntil)
        {
            *prevNext = task;
            task->sleepNext = entry;
            break;
        }

        prevNext = &entry->sleepNext;
        entry = entry->sleepNext;
    }

    if (!entry)
    {
        *prevNext = task;
        task->sleepNext = NULL;
    }
}

static void SchSleepListRemove(SchTask* task)
{
    SchTask** prevNext = &SchFirstSleepTask;
    while (*prevNext)
    {
        if (*prevNext == task)
        {
            *prevNext = task->sleepNext;
            break;
        }
        prevNext = &(*prevNext)->next;
    }
    task->sleepNext = NULL;
    task->sleepUntil = 0;
}

static SchTask* SchSleepListPop()
{
    SchTask* sleeper = SchFirstSleepTask;
    SchFirstSleepTask = sleeper->sleepNext;
    sleeper->sleepNext = NULL;
    sleeper->sleepUntil = 0;
    return sleeper;
}

static void SchWaitListAppend(SchWaitList* list, SchTask* task)
{
    task->waitList = list;
    SchTask** lastNext = &list->first;
    while (*lastNext != NULL)
        lastNext = &(*lastNext)->waitNext;
    *lastNext = task;
}

static void SchWaitListRemove(SchTask* task, bool timeout)
{
    DbgAssert(task->waitList && task->status == SCH_STATUS_WAITING);
    SchTask** prevNext = &task->waitList->first;
    while (*prevNext)
    {
        if (*prevNext == task)
        {
            *prevNext = task->waitNext;
            break;
        }
        prevNext = &(*prevNext)->next;
    }
    task->waitNext = NULL;
    task->waitList = NULL;
    task->waitTimeout = timeout;
}

static SchTask* SchWaitListPop(SchWaitList* list, bool timeout)
{
    SchTask* waiter = list->first;
    list->first = waiter->waitNext;
    waiter->waitNext = NULL;
    waiter->waitList = NULL;
    waiter->waitTimeout = timeout;
    return waiter;
}

SchTask* SchInitialize(const char* name)
{
    SListInitialize(&SchDeadTaskListHead);
    SchKernelTask.next = &SchKernelTask;
    SchKernelTask.id = SchNextTaskId++;
    SchKernelTask.name = name;
    SchKernelTask.esp = 0;
    SchKernelTask.status = SCH_STATUS_RUNNING;
    SchKernelTask.sleepNext = NULL;
    SchKernelTask.sleepUntil = 0;
    SchKernelTask.waitNext = NULL;
    SchKernelTask.waitList = NULL;
    SchKernelTask.waitTimeout = false;
    SchCurrentTask = &SchKernelTask;
    return &SchKernelTask;
}

static void SchTaskFnWrapper(SchTaskFn fn, void* ctx)
{
    TmPrintfVrb("Task #%d - %s started\n", SchCurrentTask->id, SchCurrentTask->name);
    uint32_t ret = fn(ctx);
    TmPrintfVrb("Task #%d - %s finished with return code: %u (0x%08X)\n", SchCurrentTask->id, SchCurrentTask->name, ret, ret);

    IntDisableIRQs();
    SchTask* next = SchRunListRemove(SchCurrentTask);
    SchCurrentTask->status = SCH_STATUS_DEAD;
    SListPushFront(&SchDeadTaskListHead, &SchCurrentTask->deadList);
    SchSwitchTask(next);
}

SchTask* SchCreateTask(const char* name, size_t stackSize, SchTaskFn fn, void* ctx)
{
    if (stackSize == 0)
        stackSize = 1024*1024;

    // alloc stack
    size_t stackPages = (stackSize + (KPAGE_SIZE - 1)) / KPAGE_SIZE;
    kphys_t stackPhys = PhysAlloc(stackPages, PHYS_REGION_TYPE_KERNEL_TASK_STACK, "TaskStack");
    uint8_t* stackVirt = VirtAlloc(stackPhys, stackPages, VIRT_PROT_READWRITE, VIRT_REGION_TYPE_TASK_STACK, "TaskStack");
    uint8_t* stack = stackVirt;
    k_memset(stack, 0, stackPages * KPAGE_SIZE);

    // fill stack
    stack += stackSize - 32;
    *(uint32_t*)(stack + 0)  = 0xDEAD0001;                 // task start ebp
    *(uint32_t*)(stack + 4)  = 0xDEAD0002;                 // task start edi
    *(uint32_t*)(stack + 8)  = 0xDEAD0003;                 // task start esi
    *(uint32_t*)(stack + 12) = 0xDEAD0004;                 // task start ebx
    *(uint32_t*)(stack + 16) = (uint32_t)SchTaskFnWrapper; // Task start address
    *(uint32_t*)(stack + 20)  = 0xDEAD0005;                // SchTaskFnWrapper return address
    *(uint32_t*)(stack + 24)  = (uint32_t)fn;
    *(uint32_t*)(stack + 28)  = (uint32_t)ctx;

    // alloc task
    SchTask* task = kalloc(sizeof(SchTask));
    task->id = SchNextTaskId++;
    task->name = name;
    task->esp = (uint32_t)stack;
    task->status = SCH_STATUS_RUNNING;
    task->sleepNext = NULL;
    task->sleepUntil = 0;
    task->waitNext = NULL;
    task->waitList = NULL;
    task->waitTimeout = false;
    task->deadList.Next = NULL;
    task->stackStart = stackVirt;
    task->stackPages = stackPages;

    // insert into run list
    uint32_t irqLock = IntEnterCriticalSection();
    SchRunListInsert(task);
    IntLeaveCriticalSection(irqLock);
    return task;
}

void SchYield()
{
    IntDisableIRQs();

    // Process dead list
    while (!SListIsEmpty(&SchDeadTaskListHead) && SchDeadTaskListHead.Next != &SchCurrentTask->deadList)
    {
        SchTask* dead = CONTAINING_RECORD(SListPopFront(&SchDeadTaskListHead), SchTask, deadList);
        TmPrintfVrb("Task #%d - %s deleted by task #%d (%uKiB stack memory freed)\n", dead->id, dead->name, SchCurrentTask->id, dead->stackPages * 4);

        PhysFree(VirtToPhys(dead->stackStart));
        VirtFree(dead->stackStart);
        kfree(dead);
    }

    // Process sleep list
    while (SchFirstSleepTask && SchFirstSleepTask->sleepUntil <= PitCurrentTick)
    {
        // Remove from sleep list
        SchTask* sleeper = SchSleepListPop();

        // Remove from wait list
        if (sleeper->waitList || sleeper->status == SCH_STATUS_WAITING)
            SchWaitListRemove(sleeper, true);

        // Add to run list
        SchRunListInsert(sleeper);
        sleeper->status = SCH_STATUS_RUNNING;
    }

    // Switch to next run task
    if (SchCurrentTask->next != SchCurrentTask)
        SchSwitchTask(SchCurrentTask->next);
}

void SchSleep(uint32_t ms)
{
    if (ms == 0)
        return SchYield();

    IntDisableIRQs();
    SchCurrentTask->status = SCH_STATUS_SLEEPING;
    SchSleepListInsert(SchCurrentTask, PitCurrentTick + PitMsToTicks(ms));
    SchTask* next = SchRunListRemove(SchCurrentTask);
    SchSwitchTask(next);
}

void SchStall(uint32_t microsecs)
{
    if (microsecs == 0)
        return;
    uint64_t ticksPerMicrosec = TscFrequency / 1000000;
    uint64_t stallUntil = rdtsc() + microsecs * ticksPerMicrosec;
    do
    {
        asm volatile("nop");
    } while (rdtsc() < stallUntil);
}

SchSemaphore* SchCreateSemaphore(int initial, int max)
{
    SchSemaphore* semaphore = kcalloc(sizeof(SchSemaphore));
    semaphore->max = max;
    semaphore->count = initial;
    return semaphore;
}

void SchDestroySemaphore(SchSemaphore* semaphore)
{
    DbgAssertMsg(semaphore->waiters.first == NULL, "semaphore destroyed with waiters");
    kfree(semaphore);
}

void SchSemaphoreWait(SchSemaphore* semaphore)
{
    SchSemaphoreTryWait(semaphore, SCH_INFINITE);
}

bool SchSemaphoreTryWait(SchSemaphore* semaphore, uint32_t timeoutMs)
{
    IntDisableIRQs();
    int result = --semaphore->count;
    if (result < 0)
    {
        if (timeoutMs == 0)
        {
            semaphore->count++;
            IntEnableIRQs();
            return false;
        }

        // set status
        SchTask* task = SchCurrentTask;
        task->status = SCH_STATUS_WAITING;

        // add to sleep list if we have a timeout
        if (timeoutMs != SCH_INFINITE)
            SchSleepListInsert(task, PitCurrentTick + PitMsToTicks(timeoutMs));

        // append to wait list
        SchWaitListAppend(&semaphore->waiters, task);

        // remove from run list
        SchTask* next = SchRunListRemove(task);

        // switch to next task in run list
        SchSwitchTask(next);

        // when the above function returns we have either been woken by a signal or due to timeout
        bool result = !task->waitTimeout;
        task->waitTimeout = false;
        return result;
    }
    IntEnableIRQs();
    return true;
}

void SchSemaphoreSignal(SchSemaphore* semaphore, int count)
{
    IntDisableIRQs();
    while (count-- && semaphore->count != semaphore->max)
    {
        int result = semaphore->count++;
        if (result < 0)
        {
            // remove first waiter from wait list
            SchTask* waiter = SchWaitListPop(&semaphore->waiters, false);
            waiter->status = SCH_STATUS_RUNNING;

            // remove from sleep list
            if (waiter->sleepUntil != 0)
                SchSleepListRemove(waiter);

            // add to run list
            SchRunListInsert(waiter);
        }
    }
    IntEnableIRQs();
}

SchMutex* SchCreateMutex()
{
    return kcalloc(sizeof(SchMutex));
}

void SchDestroyMutex(SchMutex* mutex)
{
    DbgAssertMsg(mutex->waiters.first == NULL, "mutex destroyed with waiters");
    kfree(mutex);
}

void SchMutexLock(SchMutex* mutex)
{
    SchMutexTryLock(mutex, SCH_INFINITE);
}

bool SchMutexTryLock(SchMutex* mutex, uint32_t timeoutMs)
{
    IntDisableIRQs();
    if (mutex->held)
    {
        if (timeoutMs == 0)
        {
            IntEnableIRQs();
            return false;
        }

        // set status
        SchTask* task = SchCurrentTask;
        task->status = SCH_STATUS_WAITING;

        // add to sleep list if we have a timeout
        if (timeoutMs != SCH_INFINITE)
            SchSleepListInsert(task, PitCurrentTick + PitMsToTicks(timeoutMs));

        // append to wait list
        SchWaitListAppend(&mutex->waiters, task);

        // remove from run list
        SchTask* next = SchRunListRemove(task);

        // switch to next task in run list
        SchSwitchTask(next);

        // when the above function returns we have either been woken by an unlock or due to timeout
        bool result = !task->waitTimeout;
        task->waitTimeout = false;
        return result;
    }
    mutex->held = true;
    IntEnableIRQs();
    return true;
}

void SchMutexUnlock(SchMutex* mutex)
{
    IntDisableIRQs();
    DbgAssertMsg(mutex->held, "mutex unlocked when not held");
    if (mutex->waiters.first)
    {
        // remove first waiter from wait list
        SchTask* waiter = SchWaitListPop(&mutex->waiters, false);
        waiter->status = SCH_STATUS_RUNNING;

        // remove from sleep list
        if (waiter->sleepUntil != 0)
            SchSleepListRemove(waiter);

        // add to run list
        SchRunListInsert(waiter);

        // switch to waiter
        SchSwitchTask(waiter);
        return;
    }
    mutex->held = false;
    IntEnableIRQs();
}

SchEvent* SchCreateEvent()
{
    return kcalloc(sizeof(SchEvent));
}

void SchDestroyEvent(SchEvent* event)
{
    DbgAssertMsg(event->waiters.first == NULL, "event destroyed with waiters");
    kfree(event);
}

void SchEventWait(SchEvent* event)
{
    SchEventTryWait(event, SCH_INFINITE);
}

bool SchEventTryWait(SchEvent* event, uint32_t timeoutMs)
{
    IntDisableIRQs();
    if (!event->signaled)
    {
        if (timeoutMs == 0)
        {
            IntEnableIRQs();
            return false;
        }

        // set status
        SchTask* task = SchCurrentTask;
        task->status = SCH_STATUS_WAITING;

        // add to sleep list if we have a timeout
        if (timeoutMs != SCH_INFINITE)
            SchSleepListInsert(task, PitCurrentTick + PitMsToTicks(timeoutMs));

        // append to wait list
        SchWaitListAppend(&event->waiters, task);

        // remove from run list
        SchTask* next = SchRunListRemove(task);

        // switch to next task in run list
        SchSwitchTask(next);

        // when the above function returns we have either been woken by a signal or due to timeout
        bool result = !task->waitTimeout;
        task->waitTimeout = false;
        return result;
    }
    IntEnableIRQs();
    return true;
}

void SchEventSignal(SchEvent* event)
{
    IntDisableIRQs();
    event->signaled = true;
    while (event->waiters.first)
    {
        // remove first waiter from wait list
        SchTask* waiter = SchWaitListPop(&event->waiters, false);
        waiter->status = SCH_STATUS_RUNNING;

        // remove from sleep list
        if (waiter->sleepUntil != 0)
            SchSleepListRemove(waiter);

        // add to run list
        SchRunListInsert(waiter);
    }
    IntEnableIRQs();
}

SchQueue* SchCreateQueue()
{
    SchQueue* queue = kcalloc(sizeof(SchQueue));
    ListInitialize(&queue->entries);
    return queue;
}

void SchDestroyQueue(SchQueue* queue)
{
    DbgAssertMsg(ListIsEmpty(&queue->entries), "queue destroyed with entries");
    DbgAssertMsg(queue->waiters.first == NULL, "queue destroyed with waiters");
    kfree(queue);
}

void SchQueuePush(SchQueue* queue, ListEntry* entry)
{
    IntDisableIRQs();
    if (queue->waiters.first == NULL)
    {
        ListPushBack(&queue->entries, entry);
    }
    else
    {
        // remove first waiter from wait list
        SchTask* waiter = SchWaitListPop(&queue->waiters, false);
        waiter->waitReturn = entry;
        waiter->status = SCH_STATUS_RUNNING;

        // remove from sleep list
        if (waiter->sleepUntil != 0)
            SchSleepListRemove(waiter);

        // add to run list
        SchRunListInsert(waiter);
    }
    IntEnableIRQs();
}

ListEntry* SchQueuePop(SchQueue* queue)
{
    return SchQueueTryPop(queue, SCH_INFINITE);
}

ListEntry* SchQueueTryPop(SchQueue* queue, uint32_t timeoutMs)
{
    IntDisableIRQs();
    ListEntry* entry = ListPopFront(&queue->entries);
    if (entry == NULL && timeoutMs != 0)
    {
        // set status
        SchTask* task = SchCurrentTask;
        task->status = SCH_STATUS_WAITING;

        // add to sleep list if we have a timeout
        if (timeoutMs != SCH_INFINITE)
            SchSleepListInsert(task, PitCurrentTick + PitMsToTicks(timeoutMs));

        // append to wait list
        SchWaitListAppend(&queue->waiters, task);

        // remove from run list
        SchTask* next = SchRunListRemove(task);

        // switch to next task in run list
        SchSwitchTask(next);

        // when the above function returns we have either been woken by a signal or due to timeout
        bool success = !task->waitTimeout;
        task->waitTimeout = false;
        return success ? task->waitReturn : NULL;
    }
    IntEnableIRQs();
    return entry;
}

SchSpinlock* SchCreateSpinlock()
{
    return kcalloc(sizeof(SchSpinlock));
}

void SchDestroySpinlock(SchSpinlock* spinlock)
{
    DbgAssertMsg(!spinlock->held, "spinlock destroyed while held");
    kfree(spinlock);
}

void SchSpinlockLock(SchSpinlock* spinlock)
{
    while (spinlock->held)
        asm volatile("nop");
    spinlock->held = true;
}

void SchSpinlockUnlock(SchSpinlock* spinlock)
{
    DbgAssertMsg(spinlock->held, "spinlock unlocked while not held");
    spinlock->held = false;
}
