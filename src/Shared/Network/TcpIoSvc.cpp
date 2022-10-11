#include "TcpIoSvc.hpp"
#include "NetTypedefs.hpp"
#include "NetSSL.hpp"
#include "NetSettings.hpp"

#include "Shared/Common/Thread.hpp"
#include "Shared/Common/Event.hpp"
#include "Shared/Common/IoService.hpp"


static uint32 TcpIoSvcMorphError(uint32 ErrorCode)
{
    //
    //  Convert NT error to WSA error
    //
    switch (ErrorCode)
    {
    case ERROR_SUCCESS:
        return ERROR_SUCCESS;

    case ERROR_OPERATION_ABORTED:
        return WSA_OPERATION_ABORTED;

    case ERROR_IO_PENDING:
        return WSA_IO_PENDING;

    case ERROR_INVALID_HANDLE:
    case ERROR_CANNOT_MAKE:
    case ERROR_DUP_NAME:
        return WSAENOTSOCK;

    case ERROR_INSUFFICIENT_BUFFER:
    case ERROR_PAGEFILE_QUOTA:
    case ERROR_COMMITMENT_LIMIT:
    case ERROR_WORKING_SET_QUOTA:
        return WSAENOBUFS;

    case ERROR_SHARING_VIOLATION:
    case ERROR_ADDRESS_ALREADY_ASSOCIATED:
        return WSAEADDRINUSE;

    case ERROR_TIMEOUT:
    case ERROR_SEM_TIMEOUT:
        return WSAETIMEDOUT;

    case ERROR_GRACEFUL_DISCONNECT:
        return WSAEDISCON;

    case ERROR_PORT_UNREACHABLE:
    case ERROR_NETNAME_DELETED:
        return WSAECONNRESET;

    case ERROR_CONNECTION_ABORTED:
        return WSAECONNABORTED;

    case ERROR_BAD_NETPATH:
    case ERROR_INVALID_NETNAME:
    case ERROR_NETWORK_UNREACHABLE:
    case ERROR_PROTOCOL_UNREACHABLE:
        return WSAENETUNREACH;

    case ERROR_HOST_UNREACHABLE:
        return WSAEHOSTUNREACH;

    case ERROR_CANCELLED:
    case ERROR_REQUEST_ABORTED:
        return WSAEINTR;

    case ERROR_BUFFER_OVERFLOW:
        return WSAEMSGSIZE;

    case ERROR_NETWORK_BUSY:
        return WSAENETDOWN;

    case ERROR_CONNECTION_REFUSED:
        return WSAECONNREFUSED;

    case ERROR_INVALID_ADDRESS:
        return WSAEADDRNOTAVAIL;

    case ERROR_NOT_SUPPORTED:
        return WSAEOPNOTSUPP;

    case ERROR_ACCESS_DENIED:
        return WSAEACCES;

    default:
        OUTPUTLN("unmapped error code %u", ErrorCode);
        return WSAEINVAL;
    };

    return WSAEINVAL;
};


static void TcpIoSvcStopCallback(void* Param)
{
    NetSslThreadCleanup();
};


static bool TcpIoSvcCompletionCallback(uint32 ErrorCode, uint32 Bytes, void* CompletionKey, void* Act, void* Param)
{
    TcpIoSvcCompletionProc_t CompletionProc = TcpIoSvcCompletionProc_t(CompletionKey);
    ASSERT(CompletionProc);
    return CompletionProc(Act, Bytes, TcpIoSvcMorphError(ErrorCode));
};


struct TcpIoSvc_t
{
    HOBJ hIoSvc;
    int32 ThreadCount;
    std::atomic<int32> RefCount;
    HOBJ hEventRefEnd;
};


static TcpIoSvc_t TcpIoSvc;


void TcpIoSvcInitialize(void)
{
    IoSvcCallbacks_t Callbacks;
    Callbacks.IoThreadStart = nullptr;
    Callbacks.IoThreadStop = TcpIoSvcStopCallback;
    Callbacks.IoComplete = TcpIoSvcCompletionCallback;

    if (NetSettings.TcpIoThreads == -1)
        TcpIoSvc.ThreadCount = (ThreadGetCoreNum() * 2);
    else
        TcpIoSvc.ThreadCount = NetSettings.TcpIoThreads;
    
    TcpIoSvc.hIoSvc = IoSvcCreate("TcpIoSvc_", TcpIoSvc.ThreadCount, &Callbacks);
    ASSERT(TcpIoSvc.hIoSvc != 0);

    TcpIoSvc.hEventRefEnd = EventCreate();
    ASSERT(TcpIoSvc.hEventRefEnd != 0);
};


void TcpIoSvcTerminate(void)
{
    if (TcpIoSvc.hEventRefEnd != 0)
    {
        EventDestroy(TcpIoSvc.hEventRefEnd);
        TcpIoSvc.hEventRefEnd = 0;
    };

    if (TcpIoSvc.hIoSvc != 0)
    {
        IoSvcDestroy(TcpIoSvc.hIoSvc);
        TcpIoSvc.hIoSvc = 0;
    };
};


void TcpIoSvcWaitForActEnd(void)
{
    if (TcpIoSvc.hEventRefEnd != 0)
    {
        auto FnEventCallback = [](void* Param) -> bool
        {
            return (TcpIoSvc.RefCount == 0);
        };

        if (!EventWaitFor(TcpIoSvc.hEventRefEnd, 5000, FnEventCallback))
            ASSERT(false, "Wait for TcpIoSvc ref end failed, ref remains: %d", TcpIoSvc.RefCount);
    };
};


void TcpIoSvcRefInc(void)
{
    ++TcpIoSvc.RefCount;
};


void TcpIoSvcRefDec(void)
{
    ASSERT(TcpIoSvc.RefCount > 0);
	if (!--TcpIoSvc.RefCount)
		EventSignalAll(TcpIoSvc.hEventRefEnd);    
};


bool TcpIoSvcRegist(HANDLE hSocket, TcpIoSvcCompletionProc_t CompletionProc)
{
    return IoSvcBind(TcpIoSvc.hIoSvc, hSocket, CompletionProc);
};


void TcpIoSvcStats(int32* IoNum, int32* ThreadsNum, int32* ThreadsAwait)
{
    *IoNum = TcpIoSvc.RefCount;
    *ThreadsNum = TcpIoSvc.ThreadCount;
    *ThreadsAwait = IoSvcThreadsAvailable(TcpIoSvc.hIoSvc);
};