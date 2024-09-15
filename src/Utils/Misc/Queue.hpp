#pragma once


HOBJ QueueCreate(int32 Capacity, int32 ObjectSize);
void QueueDestroy(HOBJ hQueue);
bool QueueIsFull(HOBJ hQueue);
bool QueueIsEmpty(HOBJ hQueue);
void QueuePush(HOBJ hQueue, void* Object);
void QueuePop(HOBJ hQueue);
void* QueueFront(HOBJ hQueue);


template<class TData>
class CQueue
{
public:
    inline CQueue(int32 Capacity)
        : m_hQueue(QueueCreate(Capacity, sizeof(TData)))
    {
        ASSERT(m_hQueue != 0);
    };

    inline ~CQueue(void)
    {
        if (m_hQueue)
        {
            QueueDestroy(m_hQueue);
            m_hQueue = 0;
        };
    };

    inline void Push(TData Data)
    {
        ASSERT(m_hQueue);
        QueuePush(m_hQueue, (void*)&Data);
    };

    inline void Pop(void)
    {
        ASSERT(m_hQueue);
        QueuePop(m_hQueue);
    };

    inline TData Front(void) const
    {
        ASSERT(m_hQueue);
        return *(TData*)QueueFront(m_hQueue);
    };

    inline bool IsEmpty(void) const
    {
        ASSERT(m_hQueue);
        return QueueIsEmpty(m_hQueue);
    };

    inline bool IsFull(void) const
    {
        ASSERT(m_hQueue);
        return QueueIsFull(m_hQueue);
    };

private:
    HOBJ m_hQueue;
};