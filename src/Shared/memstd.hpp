#pragma once

#define MEM_ALIGN (sizeof(long double))


extern DLLSHARED void* MemAlloc(std::size_t size, const char* fname = nullptr, int32 fline = 0);
extern DLLSHARED void MemFree(void* ptr, const char* fname = nullptr, int32 fline = 0);
extern DLLSHARED void* MemRealloc(void* ptr, std::size_t size, const char* fname = nullptr, int32 fline = 0);


inline void* operator new(std::size_t size)
{
    return MemAlloc(size);
};


inline void* operator new[](std::size_t size)
{
    return MemAlloc(size);
};


inline void* operator new(std::size_t size, const char* fname, int32 fline)
{
    return MemAlloc(size, fname, fline);
};


inline void* operator new[](std::size_t size, const char* fname, int32 fline)
{
    return MemAlloc(size, fname, fline);
};


inline void operator delete(void* ptr)
{
    MemFree(ptr);
};


inline void operator delete[](void* ptr)
{
    MemFree(ptr);
};


inline void operator delete(void* ptr, const char* fname, int32 fline)
{
    MemFree(ptr, fname, fline);
};


inline void operator delete[](void* ptr, const char* fname, int32 fline)
{
    MemFree(ptr, fname, fline);
};


inline void* operator new(std::size_t size, const std::nothrow_t& nth)
{
    return MemAlloc(size);
};


inline void* operator new[](std::size_t size, const std::nothrow_t& nth)
{
    return MemAlloc(size);
};


#ifdef _DEBUG
#define new new(__FILE__, __LINE__)
#endif