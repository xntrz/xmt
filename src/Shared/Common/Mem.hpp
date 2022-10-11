#pragma once

struct MemFunctions_t
{
    void* (*Alloc)(std::size_t size, const char* fname, int32 fline);
    void  (*Free)(void* ptr, const char* fname, int32 fline);
    void* (*Realloc)(void* ptr, std::size_t size, const char* fname, int32 fline);
};


DLLSHARED void MemInitialize(void);
DLLSHARED void MemTerminate(void);
DLLSHARED void MemPush(const MemFunctions_t* MemFunctions);
DLLSHARED void MemPop(void);
DLLSHARED void* MemAlloc(std::size_t size, const char* fname, int32 fline);
DLLSHARED void MemFree(void* ptr, const char* fname, int32 fline);
DLLSHARED void* MemRealloc(void* ptr, std::size_t size, const char* fname, int32 fline);
