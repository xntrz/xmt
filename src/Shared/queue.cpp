#include "queue.hpp"


struct Queue_t
{
    void* ObjectArray;
    int32 PosCur;
    int32 PosCap;
    int32 Capacity;
    int32 Stride;
    int32 Size;
};


HOBJ QueueCreate(int32 Capacity, int32 ObjectSize)
{
    ASSERT(Capacity > 0);
    ASSERT(ObjectSize > 0);
    
    Queue_t* Queue = new Queue_t();
    if (!Queue)
        return 0;

    Queue->Capacity = Capacity;
    Queue->Stride = ObjectSize;
    Queue->ObjectArray = new char[Queue->Stride * Capacity];
    if (!Queue->ObjectArray)
    {
        delete Queue;
        Queue = nullptr;
        return 0;
    };

    Queue->PosCur = 0;
    Queue->PosCap = 0;

    return Queue;
};


void QueueDestroy(HOBJ hQueue)
{
    Queue_t* Queue = (Queue_t*)hQueue;

    if (Queue->ObjectArray)
    {
        delete[] Queue->ObjectArray;
        Queue->ObjectArray = nullptr;        
    };

    delete Queue;
};


bool QueueIsFull(HOBJ hQueue)
{
    Queue_t* Queue = (Queue_t*)hQueue;

	return (Queue->Size == Queue->Capacity);
};


bool QueueIsEmpty(HOBJ hQueue)
{
    Queue_t* Queue = (Queue_t*)hQueue;

    return (Queue->Size == 0);
};


void QueuePush(HOBJ hQueue, void* Object)
{
    Queue_t* Queue = (Queue_t*)hQueue;

    ASSERT(!QueueIsFull(Queue));

    std::memcpy(
        &((char*)Queue->ObjectArray)[Queue->PosCap * Queue->Stride],
        Object,
        Queue->Stride
    );

    Queue->PosCap = (++Queue->PosCap % Queue->Capacity);
    ++Queue->Size;
};


void QueuePop(HOBJ hQueue)
{
    Queue_t* Queue = (Queue_t*)hQueue;

    ASSERT(!QueueIsEmpty(Queue));

    Queue->PosCur = (++Queue->PosCur % Queue->Capacity);

    ASSERT(Queue->Size > 0);
    --Queue->Size;
};


void* QueueFront(HOBJ hQueue)
{
    Queue_t* Queue = (Queue_t*)hQueue;

    return &((char*)Queue->ObjectArray)[Queue->PosCur * Queue->Stride];
};