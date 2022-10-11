#include "TcpStats.hpp"
#include "TcpLayer.hpp"

#include "Shared/Common/AsyncService.hpp"


struct TcpStatsLayer_t
{
    TcpLayer_t Layer;
};


struct TcpStats_t
{
    HOBJ hAsyncSvc;
    std::atomic<uint64> LastSecBytesRecvAccum;
    std::atomic<uint64> LastSecBytesSentAccum;
    std::atomic<uint64> LastSecBytesRecv;
    std::atomic<uint64> LastSecBytesSent;
    std::atomic<uint64> TotalBytesRecv;
    std::atomic<uint64> TotalBytesSent;
};


static TcpStats_t TcpStats;


static bool TcpStatsEventProc(TcpLayer_t* TcpLayer, int32 Id, const void* Param)
{
    switch (Id)
    {
    case TcpLayerEvent_Read:
        {
            TcpLayerEventParamRead_t* EventParam = (TcpLayerEventParamRead_t*)Param;
            
            TcpStats.LastSecBytesRecvAccum += EventParam->DataSize;
            TcpStats.TotalBytesRecv += EventParam->DataSize;
        }
        break;

    case TcpLayerEvent_Write:
        {
            TcpLayerEventParamWrite_t* EventParam = (TcpLayerEventParamWrite_t*)Param;
            
            TcpStats.LastSecBytesSentAccum += EventParam->DataSize;
            TcpStats.TotalBytesSent += EventParam->DataSize;
        }
        break;
    };

    return TcpLayerSendEvent(TcpLayer, Id, Param);
};


static void TcpStatsAsyncSvcProc(void* Param)
{
    TcpStats.LastSecBytesRecv = TcpStats.LastSecBytesRecvAccum.load();
    TcpStats.LastSecBytesSent = TcpStats.LastSecBytesSentAccum.load();
    TcpStats.LastSecBytesRecvAccum = 0;
    TcpStats.LastSecBytesSentAccum = 0;
};


void TcpStatsInitialize(void)
{
    AsyncSvcCallbacks_t Callbacks = {};
    Callbacks.Run = TcpStatsAsyncSvcProc;
    
    TcpStats.hAsyncSvc = AsyncSvcCreate("TcpStatsPerSecSvc", &Callbacks, 1000);
    ASSERT(TcpStats.hAsyncSvc != 0);
    AsyncSvcStart(TcpStats.hAsyncSvc);
};


void TcpStatsTerminate(void)
{
    if (TcpStats.hAsyncSvc != 0)
    {
        AsyncSvcStop(TcpStats.hAsyncSvc);
        AsyncSvcDestroy(TcpStats.hAsyncSvc);
        TcpStats.hAsyncSvc = 0;
    };
};


struct TcpLayer_t* TcpStatsCreate(void* Param)
{
    TcpStatsLayer_t* TcpStatsLayer = new TcpStatsLayer_t();
    if (!TcpStatsLayer)
        return 0;

    TcpStatsLayer->Layer.data = &TcpStatsLayer->Layer;
    TcpStatsLayer->Layer.FnEventProc = TcpStatsEventProc;
    TcpStatsLayer->Layer.FnCmdProc = nullptr;

    return &TcpStatsLayer->Layer;
};


void TcpStatsDestroy(struct TcpLayer_t* TcpLayer)
{
    TcpStatsLayer_t* TcpStatsLayer = (TcpStatsLayer_t*)TcpLayer;

    delete TcpStatsLayer;
};


void TcpStatsClear(void)
{
    TcpStats.LastSecBytesRecvAccum = 0;
    TcpStats.LastSecBytesSentAccum = 0;
    TcpStats.LastSecBytesRecv = 0;
    TcpStats.LastSecBytesSent = 0;
    TcpStats.TotalBytesRecv = 0;
    TcpStats.TotalBytesSent = 0;
};


void TcpStatsGrab(
    uint64* LastSecBytesRecvAccum,
    uint64* LastSecBytesSentAccum,
    uint64* LastSecBytesRecv,
    uint64* LastSecBytesSent,
    uint64* TotalBytesRecv,
    uint64* TotalBytesSent
)
{
    *LastSecBytesRecvAccum = TcpStats.LastSecBytesRecvAccum;
    *LastSecBytesSentAccum = TcpStats.LastSecBytesSentAccum;
    *LastSecBytesRecv = TcpStats.LastSecBytesRecv;
    *LastSecBytesSent = TcpStats.LastSecBytesSent;
    *TotalBytesRecv = TcpStats.TotalBytesRecv;
    *TotalBytesSent = TcpStats.TotalBytesSent;
};