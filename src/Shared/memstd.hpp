#pragma once

void* operator new(std::size_t size);
void* operator new[](std::size_t size);
void operator delete(void* ptr);
void operator delete[](void* ptr);
void* operator new(std::size_t size, const std::nothrow_t& nth);
void* operator new[](std::size_t size, const std::nothrow_t& nth);


extern DLLSHARED void MemSetAllocSource(const char* fname, int32 fline);

/* using __FILENAME__ macro from debug.hpp */
#define new (MemSetAllocSource(__FILENAME__, __LINE__), 0) ? NULL : new