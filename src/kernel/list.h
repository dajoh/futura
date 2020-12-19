#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H

#include <stddef.h>
#include <stdbool.h>

#define CONTAINING_RECORD(address, type, field) ((type*)((uint8_t*)(address) - (uint8_t*)(&((type*)0)->field)))

typedef struct SListEntry
{
    struct SListEntry* Next;
} SListEntry;
typedef SListEntry SListHead;

static inline void SListInitialize(SListHead* head)
{
    head->Next = NULL;
}

static inline bool SListIsEmpty(SListHead* head)
{
    return head->Next == NULL;
}

static inline void SListPushFront(SListHead* head, SListEntry* entry)
{
    entry->Next = head->Next;
    head->Next = entry;
}

static inline SListEntry* SListPopFront(SListHead* head)
{
    SListEntry* first = head->Next;
    if (first != NULL)
        head->Next = first->Next;
    return first;
}

typedef struct ListEntry
{
    struct ListEntry* Next;
    struct ListEntry* Prev;
} ListEntry;
typedef ListEntry ListHead;

static inline void ListInitialize(ListHead* head)
{
    head->Next = head;
    head->Prev = head;
}

static inline bool ListIsEmpty(ListHead* head)
{
    return head->Next == head;
}

static inline void ListPushFront(ListHead* head, ListEntry* entry)
{
    ListEntry* first = head->Next;
    entry->Next = first;
    entry->Prev = head;
    first->Prev = entry;
    head->Next = entry;
}

static inline void ListPushBack(ListHead* head, ListEntry* entry)
{
    ListEntry* last = head->Prev;
    entry->Next = head;
    entry->Prev = last;
    last->Next = entry;
    head->Prev = entry;
}

static inline void ListRemove(ListEntry* entry)
{
    ListEntry* next = entry->Next;
    ListEntry* prev = entry->Prev;
    prev->Next = next;
    next->Prev = prev;
}

static inline void ListInsertAfter(ListEntry* after, ListEntry* entry)
{
    ListEntry* next = after->Next;
    ListEntry* prev = after;
    entry->Next = next;
    entry->Prev = prev;
    next->Prev = entry;
    prev->Next = entry;
}

static inline void ListInsertBefore(ListEntry* before, ListEntry* entry)
{
    ListEntry* next = before;
    ListEntry* prev = before->Prev;
    entry->Next = next;
    entry->Prev = prev;
    next->Prev = entry;
    prev->Next = entry;
}

static inline ListEntry* ListPopFront(ListHead* head)
{
    ListEntry* first = head->Next;
    if (first == head)
        return NULL;
    ListRemove(first);
    return first;
}

static inline ListEntry* ListPopBack(ListHead* head)
{
    ListEntry* last = head->Prev;
    if (last == head)
        return NULL;
    ListRemove(last);
    return last;
}

#endif
