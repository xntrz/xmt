#include "AppMem.hpp"

#include "Shared/Common/Mem.hpp"
#include "Shared/Common/Thread.hpp"


struct Heap_t : public CListNode<Heap_t>
{
	HANDLE Handle;
	int32 ThreadId;
};


struct MemBlk_t
{
    Heap_t* Heap;
    uint32 Size;
};


struct AppMemDiag_t
{
    std::atomic<int64> AllocNum;
	std::atomic<int64> TotalCrossThreadAlloc;
    std::atomic<int64> TotalSelfThreadAlloc;
    std::atomic<uint32> AllocatedBytes;
    std::atomic<uint32> LargestAllocSize;
    std::atomic<uint32> LargestAllocatedSize;
};


struct AppMem_t
{
    std::mutex HeapListMutex;
    Heap_t HeapArray[256];
    CList<Heap_t> HeapListFree;
    CList<Heap_t> HeapListAlloc;
    AppMemDiag_t Diag;
    bool FlagMemZeroing;
    uint32 ReserveMemSize;
};


static AppMem_t AppMem;


thread_local static Heap_t* THREAD_CURRENT_HEAP = nullptr;


static Heap_t* AppMemAllocHeap(void)
{
    std::unique_lock<std::mutex> Lock(AppMem.HeapListMutex);
    if (AppMem.HeapListFree.empty())
        return nullptr;

    Heap_t* Heap = AppMem.HeapListFree.front();
    AppMem.HeapListFree.erase(Heap);
    AppMem.HeapListAlloc.push_back(Heap);

    Heap->Handle = HeapCreate(0, AppMem.ReserveMemSize, 0);
	ASSERT(Heap->Handle);
    Heap->ThreadId = ThreadGetId();
    
    return Heap;
};


static void AppMemFreeHeap(Heap_t* Heap)
{
    Heap->ThreadId = -1;

    if (Heap->Handle)
    {
        HeapDestroy(Heap->Handle);
        Heap->Handle = 0;
    };
    
    {
        std::unique_lock<std::mutex> Lock(AppMem.HeapListMutex);
        AppMem.HeapListAlloc.erase(Heap);
        AppMem.HeapListFree.push_back(Heap);
    }
};


static uint32 AppMemGetBlockSize(void* mem)
{
    void* Ptr = ((char*)mem - ALIGN(sizeof(MemBlk_t), MEM_ALIGN));
    return ((MemBlk_t*)Ptr)->Size;    
};


static void AppMemCheckHeap(void)
{
    if (THREAD_CURRENT_HEAP == nullptr)
    {
		ASSERT(THREAD_CURRENT_HEAP == nullptr);
		THREAD_CURRENT_HEAP = AppMemAllocHeap();
		ASSERT(THREAD_CURRENT_HEAP != nullptr);
    };
};


static void* AppMemAlloc(uint32 size, const char* fname, int32 fline)
{
    AppMemCheckHeap();

    uint32 OrgSize = size;
    size += ALIGN(sizeof(MemBlk_t), MEM_ALIGN);

    void* Result = HeapAlloc(THREAD_CURRENT_HEAP->Handle, 0, size);
    if (Result)
    {
        ((MemBlk_t*)Result)->Heap = THREAD_CURRENT_HEAP;
        ((MemBlk_t*)Result)->Size = OrgSize;

        Result = ((char*)Result + ALIGN(sizeof(MemBlk_t), MEM_ALIGN));

        if (AppMem.FlagMemZeroing)
            std::memset(Result, 0x00, OrgSize);

        AppMem.Diag.AllocatedBytes += (OrgSize + ALIGN(sizeof(MemBlk_t), MEM_ALIGN));
#ifdef _DEBUG        
        ++AppMem.Diag.AllocNum;

        if (size > AppMem.Diag.LargestAllocSize)
            AppMem.Diag.LargestAllocSize.store(size);

        if (AppMem.Diag.AllocatedBytes > AppMem.Diag.LargestAllocatedSize)
            AppMem.Diag.LargestAllocatedSize.store(AppMem.Diag.AllocatedBytes);

        ASSERT(ALIGN_CHECK(uint32(Result), MEM_ALIGN));
#endif        
    }
    else
    {
        std::string StrSize = std::to_string(size);
        std::string StrAlloc = std::to_string(AppMem.Diag.AllocatedBytes);
        
        Str_SplitGrp(StrSize, ',');
        Str_SplitGrp(StrAlloc, ',');
        
        DbgFatal("Memory limit is reached!\nRequested: %s bytes\nAllocated: %s bytes", StrSize.c_str(), StrAlloc.c_str());
    };

    return Result;
};


static void AppMemFree(void* mem, const char* fname, int32 fline)
{
	if (mem == nullptr)
		return;

	ASSERT(ALIGN_CHECK(uint32(mem), MEM_ALIGN));
    
    void* Ptr = ((char*)mem - ALIGN(sizeof(MemBlk_t), MEM_ALIGN));

    MemBlk_t* MemBlk = (MemBlk_t*)Ptr;
    Heap_t* Heap = MemBlk->Heap;

#ifdef _DEBUG    
    ASSERT(AppMem.Diag.AllocNum > 0);
    --AppMem.Diag.AllocNum;
#endif
    AppMem.Diag.AllocatedBytes -= (MemBlk->Size + ALIGN(sizeof(MemBlk_t), MEM_ALIGN));

#ifdef _DEBUG
    static const uint32 FREE_TAG = 0xB44DF00D;
    std::memcpy(Ptr, &FREE_TAG, Min(sizeof(FREE_TAG), MemBlk->Size));
#endif    

    if (THREAD_CURRENT_HEAP != Heap)
    {
#ifdef _DEBUG
        ++AppMem.Diag.TotalCrossThreadAlloc;
#endif
        HeapFree(Heap->Handle, 0, Ptr);
    }
    else
    {
#ifdef _DEBUG
        ++AppMem.Diag.TotalSelfThreadAlloc;
#endif        
        HeapFree(Heap->Handle, 0, Ptr);
    };
};


static void* AppMemRealloc(void* mem, uint32 size, const char* fname, int32 fline)
{
    void* Result = nullptr;
    
    if (size == 0)
    {
        MemFree(mem, fname, fline);
        Result = nullptr;
    }
    else if (mem == nullptr)
    {
        Result = MemAlloc(size, fname, fline);
    }
    else
    {
        uint32 OrgSize = AppMemGetBlockSize(mem);
        Result = AppMemAlloc(size, fname, fline);
        if (Result)
            std::memcpy(Result, mem, Min(OrgSize, size));

        AppMemFree(mem, fname, fline);
    };

    return Result;
};


void AppMemInitialize(uint32 ReserveMemSize)
{
    for (int32 i = 0; i < COUNT_OF(AppMem.HeapArray); ++i)
        AppMemFreeHeap(&AppMem.HeapArray[i]);

    MemFunctions_t AppMemFunctions = { AppMemAlloc, AppMemFree, AppMemRealloc };
    MemPush(&AppMemFunctions);

    AppMem.FlagMemZeroing = false;
    AppMem.ReserveMemSize = ReserveMemSize;
};


void AppMemTerminate(void)
{
    ASSERT(AppMem.Diag.AllocNum == 0);
    
    MemPop();
    
    auto it = AppMem.HeapListAlloc.begin();
    while (it != AppMem.HeapListAlloc.end())
    {
        Heap_t* Heap = &*it;
        ++it;

        AppMemFreeHeap(Heap);
    };
};


void AppMemGrabDiag(
    int64* AllocNum,
    int64* CrossThreadAlloc,
    int64* SelfThreadAlloc,
    uint32* AllocatedBytes,
    uint32* LargestAllocSize,
    uint32* LargestAllocatedSize
)
{
#ifdef _DEBUG    
    *AllocNum = AppMem.Diag.AllocNum;
    *CrossThreadAlloc = AppMem.Diag.TotalCrossThreadAlloc;
    *SelfThreadAlloc = AppMem.Diag.TotalSelfThreadAlloc;
    *AllocatedBytes = AppMem.Diag.AllocatedBytes;
    *LargestAllocSize = AppMem.Diag.LargestAllocSize;
    *LargestAllocatedSize = AppMem.Diag.LargestAllocatedSize;
#endif    
};