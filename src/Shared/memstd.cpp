#include "memstd.hpp"

#include "Shared/Common/Mem.hpp"

#ifdef new
#undef new
#endif

#ifdef delete
#undef delete
#endif

void* operator new(std::size_t size)    { return MemAlloc(size); };
void* operator new[](std::size_t size)  { return MemAlloc(size); };

void operator delete(void* ptr)     { MemFree(ptr); };
void operator delete[](void* ptr)   { MemFree(ptr); };

void* operator new(std::size_t size, const std::nothrow_t& nth)     { return MemAlloc(size); };
void* operator new[](std::size_t size, const std::nothrow_t& nth)   { return MemAlloc(size); };
