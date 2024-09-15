#include "Mem.hpp"


#ifdef _DEBUG
#define MEM_FNAME_UNKNOWN ("Unknown file")
#define MEM_FLINE_UNKNOWN (0)
#else
#define MEM_FNAME_UNKNOWN (nullptr)
#define MEM_FLINE_UNKNOWN (-1)
#endif


static void* DefMemAlloc(std::size_t size, const char* fname, int32 fline)
{
    return _aligned_malloc(size, MEM_ALIGN);
};


static void* DefMemRealloc(void* ptr, std::size_t size, const char* fname, int32 fline)
{
    return _aligned_realloc(ptr, size, MEM_ALIGN);
};


static void DefMemFree(void* ptr)
{
    _aligned_free(ptr);
};


struct Mem_t
{
    MemFunctions_t Stack[16];
    int32 StackSize;
    MemFunctions_t* StackPtr;
};


static Mem_t Mem;
thread_local const char* MemAllocFName = MEM_FNAME_UNKNOWN;
thread_local int32 MemAllocFLine = MEM_FLINE_UNKNOWN;


void MemInitialize(void)
{
    MemFunctions_t DefMemFunctions = { DefMemAlloc, DefMemRealloc, DefMemFree };
    MemPush(&DefMemFunctions);
};


void MemTerminate(void)
{    
    MemPop();
};


/*DLLSHARED*/ void MemPush(const MemFunctions_t* MemFunctions)
{
    ASSERT(Mem.StackSize < COUNT_OF(Mem.Stack));
    Mem.Stack[Mem.StackSize++] = *MemFunctions;
    Mem.StackPtr = &Mem.Stack[Mem.StackSize - 1];
};


/*DLLSHARED*/ void MemPop(void)
{
    ASSERT(Mem.StackSize > 0);
    Mem.Stack[--Mem.StackSize] = { nullptr, nullptr, nullptr };
    Mem.StackPtr = (Mem.StackSize ? &Mem.Stack[Mem.StackSize - 1] : nullptr);
};


/*DLLSHARED*/ void MemSetAllocSource(const char* fname, int32 fline)
{
#ifdef _DEBUG    
    MemAllocFName = fname;
    MemAllocFLine = fline;
#endif
};


/*DLLSHARED*/ void* MemAlloc(std::size_t size)
{
    void* ptr = nullptr;

    if (Mem.StackSize == 0)
        ptr = std::malloc(size);
    else
        ptr = Mem.StackPtr->Alloc(size, MemAllocFName, MemAllocFLine);

#ifdef _DEBUG
    MemAllocFName = MEM_FNAME_UNKNOWN;
    MemAllocFLine = MEM_FLINE_UNKNOWN;
#endif    

    return ptr;
};


/*DLLSHARED*/ void* MemRealloc(void* ptr, std::size_t size)
{
    if (Mem.StackSize == 0)
        ptr = std::realloc(ptr, size);
    else
        ptr = Mem.StackPtr->Realloc(ptr, size, MemAllocFName, MemAllocFLine);

#ifdef _DEBUG
    MemAllocFName = MEM_FNAME_UNKNOWN;
    MemAllocFLine = MEM_FLINE_UNKNOWN;
#endif    

    return ptr;
};


/*DLLSHARED*/ void MemFree(void* ptr)
{
    if (Mem.StackSize == 0)
        std::free(ptr);
    else
        Mem.StackPtr->Free(ptr);
};