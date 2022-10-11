#include "TcpTransportAct.hpp"
#include "NetSettings.hpp"

#include "Shared/Common/MemPool.hpp"


struct TcpActContainer_t
{
    HOBJ hMemPoolWriteAct;
    std::mutex Mutex;
};


static TcpActContainer_t TcpActContainer;


void TcpActInitialize(void)
{
    if (!TcpActContainer.hMemPoolWriteAct)
    {
        TcpActContainer.hMemPoolWriteAct = MemPoolCreate(sizeof(TcpActWrite_t), NetSettings.TcpWriteActPoolSize);
        ASSERT(TcpActContainer.hMemPoolWriteAct != 0);
    };
};


void TcpActTerminate(void)
{
    if (TcpActContainer.hMemPoolWriteAct)
    {
        MemPoolDestroy(TcpActContainer.hMemPoolWriteAct);
        TcpActContainer.hMemPoolWriteAct = 0;        
    };
};


TcpActWrite_t* TcpActWriteAlloc(void)
{
    TcpActWrite_t* ActWrite = nullptr;

    {
        std::unique_lock<std::mutex> Lock(TcpActContainer.Mutex);
        ActWrite = (TcpActWrite_t*)MemPoolAlloc(TcpActContainer.hMemPoolWriteAct);
    }
    
    if (!ActWrite)
    {
#ifdef _DEBUG
        ASSERT(false, "out of write act pool");
#else
        DbgFatal("out of write act pool");
#endif        
    };

    return ActWrite;
};


void TcpActWriteFree(TcpActWrite_t* TcpActWrite)
{
    std::unique_lock<std::mutex> Lock(TcpActContainer.Mutex);
    MemPoolFree(TcpActContainer.hMemPoolWriteAct, TcpActWrite);
};