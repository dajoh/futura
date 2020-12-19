#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "list.h"

#define SCH_INFINITE ((uint32_t)-1)

#define SCH_STATUS_RUNNING  0
#define SCH_STATUS_DEAD     1
#define SCH_STATUS_SLEEPING 2
#define SCH_STATUS_WAITING  3

#pragma pack(push, 1)
typedef struct SchTask_s SchTask;
typedef struct SchWaitList_s SchWaitList;

// NOTE: if you change this also change in asm
typedef struct SchTask_s
{
    SchTask* next;
    uint32_t id;
    const char* name;
    uint32_t esp;
    uint32_t status;
    SchTask* sleepNext;
    uint64_t sleepUntil;
    SchTask* waitNext;
    SchWaitList* waitList;
    bool waitTimeout;
    void* waitReturn;
    SListEntry deadList;
    void* stackStart;
    size_t stackPages;
} SchTask;

typedef struct SchWaitList_s
{
    SchTask* first;
} SchWaitList;

typedef struct SchSemaphore_s
{
    SchWaitList waiters;
    int max;
    int count;
} SchSemaphore;

typedef struct SchMutex_s
{
    SchWaitList waiters;
    bool held;
} SchMutex;

typedef struct SchEvent_s
{
    SchWaitList waiters;
    bool signaled;
} SchEvent;

typedef struct SchQueue_s
{
    SchWaitList waiters;
    ListHead entries;
} SchQueue;

typedef struct SchSpinlock_s
{
    bool held;
} SchSpinlock;
#pragma pack(pop)

typedef uint32_t SchTaskFn(void* ctx);

extern SchTask* SchCurrentTask;

SchTask* SchInitialize();
SchTask* SchCreateTask(const char* name, size_t stackSize, SchTaskFn fn, void* ctx);
void SchSwitchTask(SchTask* target);
void SchYield();
void SchSleep(uint32_t ms);
void SchStall(uint32_t microsecs);

SchSemaphore* SchCreateSemaphore(int initial, int max);
void SchDestroySemaphore(SchSemaphore* semaphore);
void SchSemaphoreWait(SchSemaphore* semaphore);
bool SchSemaphoreTryWait(SchSemaphore* semaphore, uint32_t timeoutMs);
void SchSemaphoreSignal(SchSemaphore* semaphore, int count);

SchMutex* SchCreateMutex();
void SchDestroyMutex(SchMutex* mutex);
void SchMutexLock(SchMutex* mutex);
bool SchMutexTryLock(SchMutex* mutex, uint32_t timeoutMs);
void SchMutexUnlock(SchMutex* mutex);

SchEvent* SchCreateEvent();
void SchDestroyEvent(SchEvent* event);
void SchEventWait(SchEvent* event);
bool SchEventTryWait(SchEvent* event, uint32_t timeoutMs);
void SchEventSignal(SchEvent* event);

SchQueue* SchCreateQueue();
void SchDestroyQueue(SchQueue* queue);
void SchQueuePush(SchQueue* queue, ListEntry* entry);
ListEntry* SchQueuePop(SchQueue* queue);
ListEntry* SchQueueTryPop(SchQueue* queue, uint32_t timeoutMs);

SchSpinlock* SchCreateSpinlock();
void SchDestroySpinlock(SchSpinlock* spinlock);
void SchSpinlockLock(SchSpinlock* spinlock);
void SchSpinlockUnlock(SchSpinlock* spinlock);

#endif
