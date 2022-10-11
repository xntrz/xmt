#include "mem.hpp"


static void* DefMemAlloc(std::size_t size, const char* fname, int32 fline)
{
    return _aligned_malloc(size, MEM_ALIGN);
};


static void DefMemFree(void* ptr, const char* fname, int32 fline)
{
    _aligned_free(ptr);
};


static void* DefMemRealloc(void* ptr, std::size_t size, const char* fname, int32 fline)
{
    return _aligned_realloc(ptr, size, MEM_ALIGN);
};


struct Mem_t
{
    MemFunctions_t Stack[16];
    int32 StackSize;
    MemFunctions_t* StackPtr;
};


static Mem_t Mem;


void MemInitialize(void)
{
    MemFunctions_t DefMemFunctions = { DefMemAlloc, DefMemFree, DefMemRealloc };
    MemPush(&DefMemFunctions);
};


void MemTerminate(void)
{    
    MemPop();
};


void MemPush(const MemFunctions_t* MemFunctions)
{
    ASSERT(Mem.StackSize < COUNT_OF(Mem.Stack));
    Mem.Stack[Mem.StackSize++] = *MemFunctions;
    Mem.StackPtr = &Mem.Stack[Mem.StackSize - 1];
};


void MemPop(void)
{
    ASSERT(Mem.StackSize > 0);
    Mem.Stack[--Mem.StackSize] = { nullptr, nullptr, nullptr };
    Mem.StackPtr = (Mem.StackSize ? &Mem.Stack[Mem.StackSize - 1] : nullptr);
};


void* MemAlloc(std::size_t size, const char* fname, int32 fline)
{
    return Mem.StackPtr->Alloc(size, fname, fline);
};


void MemFree(void* ptr, const char* fname, int32 fline)
{
    return Mem.StackPtr->Free(ptr, fname, fline);
};


void* MemRealloc(void* ptr, std::size_t size, const char* fname, int32 fline)
{
    return Mem.StackPtr->Realloc(ptr, size, fname, fline);
};