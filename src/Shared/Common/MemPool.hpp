#pragma once


DLLSHARED HOBJ MemPoolCreate(uint32 ObjectSize, int32 ObjectNum);
DLLSHARED void MemPoolDestroy(HOBJ hMemPool);
DLLSHARED void* MemPoolAlloc(HOBJ hMemPool);
DLLSHARED void MemPoolFree(HOBJ hMemPool, void* Memory);
DLLSHARED uint32 MemPoolAllocatedSize(HOBJ hMemPool);
DLLSHARED uint32 MemPoolAllocatedNum(HOBJ hMemPool);