#include "AppMem.hpp"

#include "Shared/Common/Mem.hpp"
#include "Shared/Common/Thread.hpp"
#include "Shared/Common/Spinlock.hpp"


#define ALIGN_CHECK(v, a) 		(((v) & ((a) - 1u)) == 0u)
#define ALIGN_ADJUST(v, a) 		((a) - ((v) & ((a) - 1u)))
#define ALIGN_ROUND_DOWN(v, a) 	((v) & ~((a) - 1u))
#define ALIGN_ROUND_UP(v, a) 	(((v) + ((a) - 1u)) & ~((a) - 1u))
#define ALIGN(v, a) 			(ALIGN_ROUND_UP(v, a))


#define MEM_TAG         (0x3535353535353535)
#define MEM_LOCK_TYPE   CSpinlock


struct Heap_t : public CListNode<Heap_t>
{
    HANDLE              Handle;
    std::atomic<uint64> RefCount;
#ifdef _DEBUG
    uint32              ThreadId;
#endif    
};


struct MemBlk_t
{
#ifdef _DEBUG
    uint64      Tag;
#endif
    Heap_t*     Heap;
    std::size_t Size;
};


static_assert(ALIGN_CHECK(sizeof(MemBlk_t), MEM_ALIGN), "align check failed");


struct AppMemDiag_t
{
    uint64  AllocNum;
	uint64  TotalCrossThreadAlloc;
    uint64  TotalSelfThreadAlloc;
    uint32  AllocatedBytes;
    uint32  LargestAllocSize;
    uint32  LargestAllocatedSize;
    char    LargestAllocFile[256];
    int32   LargestAllocLine;
};


struct AppMem_t
{
    MEM_LOCK_TYPE   Mutex;
    Heap_t          HeapArray[512];
    CList<Heap_t>   HeapListFree;
    CList<Heap_t>   HeapListAlloc;
    AppMemDiag_t    Diag;
    bool            FlagMemZeroing;
    uint32          ReserveMemSize;
};


static AppMem_t         AppMem;
thread_local Heap_t*    THREAD_CURRENT_HEAP = nullptr;


static Heap_t* AppMemAllocHeap(void)
{
    std::unique_lock<MEM_LOCK_TYPE> Lock(AppMem.Mutex);

    if (AppMem.HeapListFree.empty())
        return nullptr;

    HANDLE hHeap = HeapCreate(0, AppMem.ReserveMemSize, 0);
    if (hHeap == NULL)
        return nullptr;

    Heap_t* Heap = AppMem.HeapListFree.front();
    AppMem.HeapListFree.erase(Heap);
    AppMem.HeapListAlloc.push_back(Heap);

    Heap->Handle    = hHeap;
    Heap->RefCount  = 0;
#ifdef _DEBUG
    Heap->ThreadId  = thread::current_id();
#endif

    return Heap;
};


static void AppMemFreeHeap(Heap_t* Heap)
{
#ifdef _DEBUG
    Heap->ThreadId = 0;
#endif    
    Heap->RefCount = 0;
    
    if (Heap->Handle)
    {
        HeapDestroy(Heap->Handle);
        Heap->Handle = 0;
    };
    
    {
        std::unique_lock<MEM_LOCK_TYPE> Lock(AppMem.Mutex);
        AppMem.HeapListAlloc.erase(Heap);
        AppMem.HeapListFree.push_back(Heap);
    }
};


static void AppMemHeapRefAdd(Heap_t* Heap)
{
    ++Heap->RefCount;
};


static void AppMemHeapRefDec(Heap_t* Heap)
{
    ASSERT(Heap->RefCount > 0);
    if (--Heap->RefCount == 0)
        AppMemFreeHeap(Heap);
};


static void AppMemCheckHeap(void)
{
    if (THREAD_CURRENT_HEAP == nullptr)
    {
		THREAD_CURRENT_HEAP = AppMemAllocHeap();
		ASSERT(THREAD_CURRENT_HEAP != nullptr);

        struct AppMemThreadExit_t
        {
            AppMemThreadExit_t() {
                AppMemHeapRefAdd(THREAD_CURRENT_HEAP);
            };

            ~AppMemThreadExit_t() {
                AppMemHeapRefDec(THREAD_CURRENT_HEAP);
            };
        };

        static thread_local AppMemThreadExit_t on_exit;
    };
};


static void* AppMemAlloc(std::size_t size, const char* fname, int32 fline)
{
    AppMemCheckHeap();

    /* adjust alloc size & save original size */
    std::size_t OrgSize = size;
    size += sizeof(MemBlk_t);

    /* alloc mem */
    MemBlk_t* Result = (MemBlk_t*)HeapAlloc(THREAD_CURRENT_HEAP->Handle, 0, size);
    if (Result)
    {
        /* init mem blk control info */
        AppMemHeapRefAdd(THREAD_CURRENT_HEAP);
#ifdef _DEBUG
        Result->Tag = MEM_TAG;
#endif
        Result->Heap = THREAD_CURRENT_HEAP;
        Result->Size = OrgSize;
        Result++;

        /* zero memory if opt is set */
        if (AppMem.FlagMemZeroing)
            std::memset(Result, 0x00, OrgSize);

        /* update diag stats */
        {
            std::unique_lock<MEM_LOCK_TYPE> Lock(AppMem.Mutex);
            AppMem.Diag.AllocatedBytes += size;
#ifdef _DEBUG
            ++AppMem.Diag.AllocNum;

            if (size > AppMem.Diag.LargestAllocSize)
            {
                AppMem.Diag.LargestAllocSize = size;
                if (fname)
                    std::strcpy(AppMem.Diag.LargestAllocFile, fname);
                AppMem.Diag.LargestAllocLine = fline;
            };

            if (AppMem.Diag.AllocatedBytes > AppMem.Diag.LargestAllocatedSize)
                AppMem.Diag.LargestAllocatedSize = AppMem.Diag.AllocatedBytes;            
#endif   
        }
    }
    else
    {
        /* no memory case */
        std::string StrSize = std::to_string(size);
        std::string StrAlloc = std::to_string(AppMem.Diag.AllocatedBytes);
        
		strsplitgrp(StrSize, ',');
		strsplitgrp(StrAlloc, ',');
        
        DbgFatal(
            "Memory limit is reached!\n"
            "Requested: %s bytes\n"
            "Allocated: %s bytes",
            StrSize.c_str(),
            StrAlloc.c_str()
        );
    };

    ASSERT(ALIGN_CHECK(std::intptr_t(Result), MEM_ALIGN));

    return Result;
};


static void AppMemFree(void* mem)
{
    if (mem == nullptr)
		return;

	ASSERT(ALIGN_CHECK(std::intptr_t(mem), MEM_ALIGN));

    /* get real pointer from mem */
    MemBlk_t* MemBlk = reinterpret_cast<MemBlk_t*>(mem);
    --MemBlk;
#ifdef _DEBUG
    ASSERT(MemBlk->Tag == MEM_TAG);
#endif

    Heap_t* Heap = MemBlk->Heap;

    /* update diag stats */
    {
        std::unique_lock<MEM_LOCK_TYPE> Lock(AppMem.Mutex);
#ifdef _DEBUG   
        if (THREAD_CURRENT_HEAP != Heap)
            ++AppMem.Diag.TotalCrossThreadAlloc;
        else
            ++AppMem.Diag.TotalSelfThreadAlloc;           

        ASSERT(AppMem.Diag.AllocNum > 0);
        --AppMem.Diag.AllocNum;
#endif
        AppMem.Diag.AllocatedBytes -= (MemBlk->Size + sizeof(MemBlk_t));
    }

    /* free memory */
    HeapFree(Heap->Handle, 0, MemBlk);
    AppMemHeapRefDec(Heap);
};


static void* AppMemRealloc(void* mem, std::size_t size, const char* fname, int32 fline)
{
    void* Result = nullptr;
    
    if (size == 0)
    {
		AppMemFree(mem);
        Result = nullptr;
    }
    else if (mem == nullptr)
    {
        Result = AppMemAlloc(size, fname, fline);
    }
    else
    {
        Result = AppMemAlloc(size, fname, fline);
        if (Result)
        {
            std::size_t OrgSize = (reinterpret_cast<MemBlk_t*>(mem) - 1)->Size;
            std::memcpy(Result, mem, std::min(OrgSize, size));
        };

        AppMemFree(mem);
    };

    return Result;
};


void AppMemInitialize(uint32 ReserveMemSize)
{
    /* mark all heaps as free */
    for (int32 i = 0; i < COUNT_OF(AppMem.HeapArray); ++i)
        AppMemFreeHeap(&AppMem.HeapArray[i]);

    /* override global mem functions */
    MemFunctions_t AppMemFunctions = { AppMemAlloc, AppMemRealloc, AppMemFree };
    MemPush(&AppMemFunctions);

    /* init mem settings */    
    AppMem.FlagMemZeroing = false;
    AppMem.ReserveMemSize = ReserveMemSize;

    /* init heap for main thread */
    THREAD_CURRENT_HEAP = AppMemAllocHeap();
    AppMemHeapRefAdd(THREAD_CURRENT_HEAP);
    //AppMemCheckHeap();
};


void AppMemTerminate(void)
{
    /* allocs checkout */
    ASSERT(AppMem.Diag.AllocNum == 0, "alloc remains: %" PRIu64, AppMem.Diag.AllocNum);

    /* terminate heap for main thread */
    AppMemHeapRefDec(THREAD_CURRENT_HEAP);
};


void AppMemTerminate2(void)
{
    /* restore prev global mem fn's */
    MemPop();
#ifdef _DEBUG
    OUTPUT(
        "\n"
        "----------App memory diag----------\n"
        "Allocs:                 %" PRIu64 "\n"
        "Cross thread allocs:    %" PRIu64 "\n"
        "Self thread allocs:     %" PRIu64 "\n"
        "Allocated bytes:        %" PRIu32 "\n"
        "Largest alloc size:     %" PRIu32 "\n"
        "Largest allocated size: %" PRIu32 "\n"
        "Largest alloc fname:    %s\n"
        "Largest alloc fline:    %" PRIi32 "\n"
        "-----------------------------------\n"
        "\n",
        AppMem.Diag.AllocNum,
        AppMem.Diag.TotalCrossThreadAlloc,
        AppMem.Diag.TotalSelfThreadAlloc,
        AppMem.Diag.AllocatedBytes,
        AppMem.Diag.LargestAllocSize,
        AppMem.Diag.LargestAllocatedSize,
        AppMem.Diag.LargestAllocFile,
        AppMem.Diag.LargestAllocLine
    );
#endif   
};


void AppMemGrabDiag(
    uint64* AllocNum,
    uint64* CrossThreadAlloc,
    uint64* SelfThreadAlloc,
    uint32* AllocatedBytes,
    uint32* LargestAllocSize,
    uint32* LargestAllocatedSize
)
{
#ifdef _DEBUG    
    std::unique_lock<MEM_LOCK_TYPE> Lock(AppMem.Mutex);

    *AllocNum               = AppMem.Diag.AllocNum;
    *CrossThreadAlloc       = AppMem.Diag.TotalCrossThreadAlloc;
    *SelfThreadAlloc        = AppMem.Diag.TotalSelfThreadAlloc;
    *AllocatedBytes         = AppMem.Diag.AllocatedBytes;
    *LargestAllocSize       = AppMem.Diag.LargestAllocSize;
    *LargestAllocatedSize   = AppMem.Diag.LargestAllocatedSize;
#endif    
};