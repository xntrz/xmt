#include "MemPool.hpp"


union MemPoolItem_t
{
    struct
    {
        MemPoolItem_t* Next;
    };
    double pad;
};


struct MemPool_t
{
    MemPoolItem_t* ListFree;
    uint32 ObjectSize;
    uint32 ObjectAllocated;
    int16 Padding;
    int16 Alignment;
};


HOBJ MemPoolCreate(uint32 ObjectSize, int32 ObjectNum)
{
    int16 Alignment = MEM_ALIGN;
	ObjectSize = ALIGN(ObjectSize, Alignment);

    char* Memory = new char[sizeof(MemPool_t) + Alignment + (sizeof(MemPoolItem_t) + ObjectSize) * ObjectNum];
    if (!Memory)
        return 0;

    uint32 Padding = 0;    
    if (!ALIGN_CHECK(Memory, Alignment))
        Padding = ALIGN_ADJUST(Memory, Alignment);

    MemPool_t* Pool = (MemPool_t*)((char*)Memory + Padding);
    ASSERT(ALIGN_CHECK(Pool, Alignment));

    Pool->ObjectSize = ObjectSize;
    Pool->ObjectAllocated = 0;
    Pool->Padding = Padding;
    Pool->Alignment = Alignment;

	MemPoolItem_t* Item = (MemPoolItem_t*)(Pool + 1);
    Pool->ListFree = Item;
    
    for (int32 i = 0; i < ObjectNum; ++i)
    {
		ASSERT(ALIGN_CHECK(Item, Alignment));
        Item->Next = (i + 1 >= ObjectNum ? nullptr : (MemPoolItem_t*)((char*)Item + sizeof(MemPoolItem_t) + ObjectSize));        
        Item = Item->Next;
    };

    return Pool;
};


void MemPoolDestroy(HOBJ hMemPool)
{
    MemPool_t* Pool = (MemPool_t*)hMemPool;
    
    ASSERT(Pool->ObjectAllocated == 0);

    char* Memory = (char*)Pool;
    delete[] Memory;
};


void* MemPoolAlloc(HOBJ hMemPool)
{
    MemPool_t* Pool = (MemPool_t*)hMemPool;
    if (!Pool->ListFree)
        return nullptr;

    ++Pool->ObjectAllocated;

    MemPoolItem_t* ItemFree = Pool->ListFree;
    Pool->ListFree = ItemFree->Next;

    ItemFree->Next = nullptr;

    void* Result = ((char*)ItemFree + sizeof(MemPoolItem_t));
	ASSERT(ALIGN_CHECK(Result, Pool->Alignment));
    
    return Result;
};


void MemPoolFree(HOBJ hMemPool, void* Memory)
{
    MemPool_t* Pool = (MemPool_t*)hMemPool;
    ASSERT(ALIGN_CHECK(Memory, Pool->Alignment));

	MemPoolItem_t* Item = (MemPoolItem_t*)((char*)Memory - sizeof(MemPoolItem_t));
    ASSERT(Item->Next == nullptr);

    Item->Next = Pool->ListFree;
    Pool->ListFree = Item;

    ASSERT(Pool->ObjectAllocated > 0);
    --Pool->ObjectAllocated;
};


uint32 MemPoolAllocatedSize(HOBJ hMemPool)
{
    MemPool_t* Pool = (MemPool_t*)hMemPool;
    return (Pool->ObjectAllocated * Pool->ObjectSize);
};


uint32 MemPoolAllocatedNum(HOBJ hMemPool)
{
    return ((MemPool_t*)hMemPool)->ObjectAllocated;
};