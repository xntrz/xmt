#pragma once

#define MEM_ALIGN (sizeof(long double))


struct MemFunctions_t
{
    void* (*Alloc)(std::size_t size, const char* fname, int32 fline);
    void* (*Realloc)(void* ptr, std::size_t size, const char* fname, int32 fline);
    void  (*Free)(void* ptr);
};


void MemInitialize(void);
void MemTerminate(void);
DLLSHARED void MemPush(const MemFunctions_t* MemFunctions);
DLLSHARED void MemPop(void);
DLLSHARED void MemSetAllocSource(const char* fname, int32 fline);
DLLSHARED void* MemAlloc(std::size_t size);
DLLSHARED void* MemRealloc(void* ptr, std::size_t size);
DLLSHARED void MemFree(void* ptr);
