#include "TcpTransportTmout.hpp"
#include "TcpTransportAct.hpp"

#include "Shared/Common/AsyncService.hpp"
#include "Shared/Common/Time.hpp"


enum TcpTmoutFlag_t
{
    TcpTmoutFlag_Inpool     = BIT(0),
    TcpTmoutFlag_Complete   = BIT(1),
    TcpTmoutFlag_Timeout    = BIT(2),
};


struct TcpTmoutSvc_t
{
    HOBJ hAsyncSvc;
    CList<TcpAct_t> ListAct;
    std::recursive_mutex Mutex;
    std::atomic<int32> RefCount;
};


static TcpTmoutSvc_t TcpTmoutSvc;


static void TcpTmoutSvcProc(void* Param)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTmoutSvc.Mutex);
    
    auto it = TcpTmoutSvc.ListAct.begin();
    while (it != TcpTmoutSvc.ListAct.end())
    {
        TcpAct_t* Act = &(*it);

        if ((Act->hSocket == INVALID_SOCKET) || (Act->hSocket == 0))
        {
            ++it;
            TcpTmoutRemove(Act);
            continue;
        };

        if (IS_FLAG_SET_ANY(Act->TimeoutFlags, TcpTmoutFlag_Complete | TcpTmoutFlag_Timeout))
        {
            ++it;
            continue;
        };

        uint32 Bytes = 0;
        uint32 Flags = 0;

        bool bIsTimeout = ((TimeCurrentTick() - Act->TimeStart) > Act->Timeout);
        bool bIsComplete = bool(WSAGetOverlappedResult(Act->hSocket, &Act->Ovl, LPDWORD(&Bytes), FALSE, LPDWORD(&Flags)) > 0);

        if (bIsComplete)
        {
            FLAG_SET(Act->TimeoutFlags, TcpTmoutFlag_Complete);
        }
        else if (bIsTimeout)
        {
            if (!IS_FLAG_SET(Act->TimeoutFlags, TcpTmoutFlag_Timeout))
            {
                FLAG_SET(Act->TimeoutFlags, TcpTmoutFlag_Timeout);
                Act->FlagTimeoutProc = true;
                CancelIoEx(HANDLE(Act->hSocket), &Act->Ovl);
            };
        }
        else
        {
            ++it;
        };
    };
};


void TcpTmoutInitialize(void)
{
    AsyncSvcCallbacks_t Callbacks = {};
    Callbacks.Run = TcpTmoutSvcProc;

    TcpTmoutSvc.hAsyncSvc = AsyncSvcCreate("TcpTmoutSvc", &Callbacks, 100);
    ASSERT(TcpTmoutSvc.hAsyncSvc != 0);
    AsyncSvcStart(TcpTmoutSvc.hAsyncSvc);
};


void TcpTmoutTerminate(void)
{
    ASSERT(TcpTmoutSvc.RefCount == 0);
    
    AsyncSvcStop(TcpTmoutSvc.hAsyncSvc);
    AsyncSvcDestroy(TcpTmoutSvc.hAsyncSvc);
    TcpTmoutSvc.hAsyncSvc = 0;
};


void TcpTmoutRegist(TcpAct_t* Act)
{
    ASSERT(Act);

    std::lock_guard<std::recursive_mutex> Lock(TcpTmoutSvc.Mutex);
    
    if (!IS_FLAG_SET(Act->TimeoutFlags, TcpTmoutFlag_Inpool))
    {
        FLAG_SET(Act->TimeoutFlags, TcpTmoutFlag_Inpool);
        FLAG_CLEAR(Act->TimeoutFlags, TcpTmoutFlag_Complete | TcpTmoutFlag_Timeout);

        Act->TimeStart = TimeCurrentTick();
        Act->TimeoutNode.data = Act;
    
        TcpTmoutSvc.ListAct.push_back(&Act->TimeoutNode);
        ++TcpTmoutSvc.RefCount;
    };
};


void TcpTmoutRemove(TcpAct_t* Act)
{
    ASSERT(Act);

    std::lock_guard<std::recursive_mutex> lock(TcpTmoutSvc.Mutex);
    
    if (IS_FLAG_SET(Act->TimeoutFlags, TcpTmoutFlag_Inpool))
    {
        ASSERT(TcpTmoutSvc.RefCount);
        --TcpTmoutSvc.RefCount;
        
        FLAG_CLEAR(Act->TimeoutFlags, TcpTmoutFlag_Inpool);
        TcpTmoutSvc.ListAct.erase(&Act->TimeoutNode);
    };
};