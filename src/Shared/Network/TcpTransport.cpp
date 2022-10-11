#include "TcpTransport.hpp"
#include "TcpTransportAct.hpp"
#include "TcpTransportTmout.hpp"
#include "TcpSockPool.hpp"
#include "TcpLayer.hpp"
#include "TcpSession.hpp"
#include "TcpIoSvc.hpp"
#include "TcpSession.hpp"
#include "NetTypedefs.hpp"
#include "NetAddr.hpp"
#include "NetSockUtils.hpp"
#include "NetSettings.hpp"

#include "Shared/Common/AsyncService.hpp"


enum TcpTransportFlag_t
{
    TcpTransportFlag_Default            = 0x0,
    TcpTransportFlag_Listen             = BIT(0),
    TcpTransportFlag_Remote             = BIT(1),
    TcpTransportFlag_DcPending          = BIT(2),
    TcpTransportFlag_DcRequest          = BIT(3),
    TcpTransportFlag_DcReuse            = BIT(4),
    TcpTransportFlag_Dispatch           = BIT(5),
    TcpTransportFlag_Connect            = BIT(6),
    TcpTransportFlag_ForceClose         = BIT(7),
};


struct TcpTransport_t
{
    TcpLayer_t Layer;
    std::atomic<int32> Generation;
    std::atomic<SOCKET> hSocket;
	std::atomic<int32> IoCount;
	std::atomic<int32> DisconnectOnCompleteCounter;
	std::recursive_mutex Mutex;
	CList<TcpActWrite_t> ListActWrite;
    std::atomic<int32> WriteCount;
	TcpActAccept_t* ActAcceptArray;
	int32 ActAcceptArraySize;
	TcpActRead_t ActRead;
	TcpActConnect_t ActConnect;
	TcpActDisconnect_t ActDisconnect;
    uint64 NetAddr;
    std::atomic<uint32> Flags;
    uint32 TimeoutRead;
};


static bool TcpTransportIoCompletionProc(void* Act, uint32 Bytes, uint32 ErrorCode);
static void TcpTransportSetFlag(TcpTransport_t* TcpTransport, uint32 Flag, bool FlagSet);
static bool TcpTransportTestFlag(TcpTransport_t* TcpTransport, uint32 Flag);
static bool TcpTransportIsOpen(TcpTransport_t* TcpTransport);
static void TcpTransportClose(TcpTransport_t* TcpTransport);
static void TcpTransportIoInc(TcpTransport_t* TcpTransport);
static void TcpTransportIoDec(TcpTransport_t* TcpTransport);
static void TcpTransportWriteInc(TcpTransport_t* TcpTransport);
static void TcpTransportWriteDec(TcpTransport_t* TcpTransport);
static bool TcpTransportDispatchEvent(TcpTransport_t* TcpTransport, int32 Id, const void* Param);
static bool TcpTransportBindSocketCompletion(TcpTransport_t* TcpTransport);
static bool TcpTransportCancelIo(TcpTransport_t* TcpTransport, TcpAct_t* Act);
static void TcpTransportStartupPerSession(TcpTransport_t* TcpTransport);
static void TcpTransportCleanupPerSession(TcpTransport_t* TcpTransport);
static void TcpTransportCleanupPerConnect(TcpTransport_t* TcpTransport);
static void TcpTransportSocketReuseProc(TcpTransport_t* TcpTransport);
static void TcpTransportCompleteAccept(TcpTransport_t* TcpTransport, TcpActAccept_t* Act, uint32 Bytes, uint32 ErrorCode);
static void TcpTransportCompleteConnect(TcpTransport_t* TcpTransport, TcpActConnect_t* Act, uint32 Bytes, uint32 ErrorCode);
static bool TcpTransportCompleteConnect(TcpTransport_t* TcpTransport, SOCKET hSocket);
static void TcpTransportCompleteRead(TcpTransport_t* TcpTransport, TcpActRead_t* Act, uint32 Bytes, uint32 ErrorCode);
static void TcpTransportCompleteWrite(TcpTransport_t* TcpTransport, TcpActWrite_t* Act, uint32 Bytes, uint32 ErrorCode);
static void TcpTransportCompleteDisconnect(TcpTransport_t* TcpTransport, TcpActDisconnect_t* Act, uint32 ErrorCode);
static bool TcpTransportStartAccept(TcpTransport_t* TcpTransport, TcpActAccept_t* Act);
static bool TcpTransportStartConnect(TcpTransport_t* TcpTransport, uint32 Timeout);
static void TcpTransportStartRead(TcpTransport_t* TcpTransport);
static void TcpTransportStartWrite(TcpTransport_t* TcpTransport);
static bool TcpTransportStartDisconnect(TcpTransport_t* TcpTransport, bool FlagReuse);
static bool TcpTransportCreateSendNode(TcpTransport_t* TcpTransport, const char* Data, int32 Size, bool FlagDisconnectOnComplete);
static bool TcpTransportConnect(TcpTransport_t* TcpTransport, uint32 Ip, uint16 Port, uint32 Timeout);
static bool TcpTransportCancelConnect(TcpTransport_t* TcpTransport);
static bool TcpTransportListen(TcpTransport_t* TcpTransport, uint32 Ip, uint16 Port);
static bool TcpTransportListenEnable(TcpTransport_t* TcpTransport, bool Flag);
static bool TcpTransportWrite(TcpTransport_t* TcpTransport, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete);
static bool TcpTransportDisconnect(TcpTransport_t* TcpTransport);
static bool TcpTransportCmdProc(TcpLayer_t* TcpLayer, int32 Id, void* Param);


#ifdef _DEBUG

static bool TcpTransportDbgIsFatalError(uint32 ErrorCode)
{
    return ((ErrorCode != WSAEINTR) &&					// happens when call WSARecv while disconnecting socket
			(ErrorCode != ERROR_SUCCESS) &&             // no error
            (ErrorCode != WSAENOTSOCK) &&               // socket was closed while read or write
            (ErrorCode != WSA_OPERATION_ABORTED) &&     // io canceled
            (ErrorCode != WSAENOTCONN) &&               // socket was disconnected while read or write
            (ErrorCode != WSAECONNABORTED) &&           // connection aborted while read or write
            (ErrorCode != WSAECONNRESET) &&             // connection reset while reasd or write
            (ErrorCode != WSAESHUTDOWN));               // connection closed while trying write
};


#define TcpTransportErrOutputLn(code, format, ...)              \
    do                                                          \
    {                                                           \
        if (TcpTransportDbgIsFatalError(code))                  \
            OUTPUTLN(format, __VA_ARGS__);                      \
    } while (0)

#else

#define TcpTransportErrOutputLn(format, ...) ((void)(0))

#endif


static bool TcpTransportIoCompletionProc(void* Act, uint32 Bytes, uint32 ErrorCode)
{
    TcpAct_t* TcpAct = CONTAINING_RECORD(Act, TcpAct_t, TcpAct_t::Ovl);

    TcpTransport_t* TcpTransport = (TcpTransport_t*)TcpAct->Transport;
    ASSERT(TcpTransport);

    //
    //  Pre dispatch phase
    //
    if (ErrorCode == WSA_OPERATION_ABORTED && TcpAct->FlagTimeoutProc)
        ErrorCode = WSAETIMEDOUT;

    //
    //  Dispatch phase
    //
    switch (TcpAct->Type)
    {
    case TcpActType_Accept:
        TcpTransportCompleteAccept(TcpTransport, (TcpActAccept_t*)Act, Bytes, ErrorCode);
        break;

    case TcpActType_Connect:
        TcpTransportCompleteConnect(TcpTransport, (TcpActConnect_t*)Act, Bytes, ErrorCode);
        break;

    case TcpActType_Read:
        TcpTransportCompleteRead(TcpTransport, (TcpActRead_t*)Act, Bytes, ErrorCode);
        break;

    case TcpActType_Write:
        TcpTransportCompleteWrite(TcpTransport, (TcpActWrite_t*)Act, Bytes, ErrorCode);
        break;

    case TcpActType_Disconnect:
        TcpTransportCompleteDisconnect(TcpTransport, (TcpActDisconnect_t*)Act, ErrorCode);
        break;

    default:
        ASSERT(false);
        break;
    };

    //
    //  Post dispatch phase
    //
    if (!ErrorCode)
    {
        if ((TcpAct->Type == TcpActType_Connect) ||
            (TcpAct->Type == TcpActType_Read))
        {       
            if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcRequest))
            {
                TcpTransportStartDisconnect(TcpTransport, false);
                TcpTransportSetFlag(TcpTransport, TcpTransportFlag_DcRequest, false);
            };
        };
    };

    TcpTransportIoDec(TcpTransport);

    return true;
};


static void TcpTransportSetFlag(TcpTransport_t* TcpTransport, uint32 Flag, bool FlagSet)
{
    if (FlagSet)
        TcpTransport->Flags.fetch_or(Flag);
    else
        TcpTransport->Flags.fetch_and(~Flag);
};


static bool TcpTransportTestFlag(TcpTransport_t* TcpTransport, uint32 Flag)
{
    return IS_FLAG_SET(TcpTransport->Flags.load(), Flag);
};


static bool TcpTransportIsOpen(TcpTransport_t* TcpTransport)
{
    return (TcpTransport->hSocket != INVALID_SOCKET);
};


static void TcpTransportClose(TcpTransport_t* TcpTransport)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    if (TcpTransportIsOpen(TcpTransport))
    {
        NetSockClose(TcpTransport->hSocket);
        TcpTransport->hSocket = INVALID_SOCKET;    
    };
};


static void TcpTransportIoInc(TcpTransport_t* TcpTransport)
{
    TcpIoSvcRefInc();
    
    if (!TcpTransport->IoCount++)
        TcpLayerRefInc(&TcpTransport->Layer);
};


static void TcpTransportIoDec(TcpTransport_t* TcpTransport)
{    
    TcpIoSvcRefDec();
    
    ASSERT(TcpTransport->IoCount > 0);
    if (!--TcpTransport->IoCount)
        TcpLayerRefDec(&TcpTransport->Layer);
};


static void TcpTransportWriteInc(TcpTransport_t* TcpTransport)
{
    ++TcpTransport->WriteCount;
};


static void TcpTransportWriteDec(TcpTransport_t* TcpTransport)
{
    ASSERT(TcpTransport->WriteCount > 0);
    --TcpTransport->WriteCount;
};


static bool TcpTransportDispatchEvent(TcpTransport_t* TcpTransport, int32 Id, const void* Param)
{
    TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Dispatch, true);
    bool bResult = TcpLayerSendEvent(&TcpTransport->Layer, Id, Param);
    TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Dispatch, false);
    return bResult;
};


static bool TcpTransportBindSocketCompletion(TcpTransport_t* TcpTransport)
{
    ASSERT(TcpTransportIsOpen(TcpTransport));
    
    return TcpIoSvcRegist(HANDLE(TcpTransport->hSocket.load()), TcpTransportIoCompletionProc);
};


static bool TcpTransportCancelIo(TcpTransport_t* TcpTransport, TcpAct_t* Act)
{
    ASSERT(Act);

    bool bResult = false;

    if (!HasOverlappedIoCompleted(&Act->Ovl))
    {
        bResult = bool(CancelIoEx(HANDLE(TcpTransport->hSocket.load()), &Act->Ovl) > 0);
        if (!bResult && (GetLastError() != ERROR_NOT_FOUND))
            ASSERT(bResult, "%u", GetLastError());
    };

    return bResult;
};


static void TcpTransportStartupPerSession(TcpTransport_t* TcpTransport)
{
    TcpTransport->Layer.data = &TcpTransport->Layer;
    TcpTransport->Layer.FnEventProc = nullptr;
    TcpTransport->Layer.FnCmdProc = TcpTransportCmdProc;
    TcpTransport->Generation = 0;
    TcpTransport->hSocket = INVALID_SOCKET;
    TcpTransport->IoCount = 0;
    TcpTransport->DisconnectOnCompleteCounter = 0;
    TcpTransport->ListActWrite.clear();
	TcpTransport->WriteCount = 0;
    TcpTransport->ActAcceptArray = nullptr;
    TcpTransport->ActAcceptArraySize = 0;
    std::memset(&TcpTransport->ActRead, 0x00, sizeof(TcpTransport->ActRead));
    std::memset(&TcpTransport->ActConnect, 0x00, sizeof(TcpTransport->ActConnect));
    std::memset(&TcpTransport->ActDisconnect, 0x00, sizeof(TcpTransport->ActDisconnect));
    NetAddrInit(&TcpTransport->NetAddr);
    TcpTransport->Flags = TcpTransportFlag_Default;
    TcpTransport->TimeoutRead = NetSettings.TcpReadTimeout;
};


static void TcpTransportCleanupPerSession(TcpTransport_t* TcpTransport)
{
    TcpTmoutRemove(&TcpTransport->ActRead);
    TcpTmoutRemove(&TcpTransport->ActConnect);
    TcpTmoutRemove(&TcpTransport->ActDisconnect);

    TcpTransportClose(TcpTransport);

    TcpTransport->IoCount = 0;
    TcpTransport->DisconnectOnCompleteCounter = 0;

    auto it = TcpTransport->ListActWrite.begin();
    while (it != TcpTransport->ListActWrite.end())
    {
        TcpActWrite_t* Act = &(*it);
        it = TcpTransport->ListActWrite.erase(it);

        TcpActWriteFree(Act);
        TcpTransportWriteDec(TcpTransport);
    };

    if (TcpTransport->ActAcceptArray)
    {
        delete[] TcpTransport->ActAcceptArray;
        TcpTransport->ActAcceptArray = nullptr;

        TcpTransport->ActAcceptArraySize = 0;
    };
};


static void TcpTransportCleanupPerConnect(TcpTransport_t* TcpTransport)
{
    NetAddrInit(&TcpTransport->NetAddr);
    TcpTransport->DisconnectOnCompleteCounter = 0;

    uint32 ClearFlag = (TcpTransportFlag_Listen |
                        TcpTransportFlag_Connect |
                        TcpTransportFlag_DcRequest |
                        TcpTransportFlag_DcPending |
                        TcpTransportFlag_DcReuse);

    TcpTransportSetFlag(TcpTransport, ClearFlag, false);
};


static void TcpTransportSocketReuseProc(TcpTransport_t* TcpTransport)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    auto it = TcpTransport->ListActWrite.begin();
    while (it != TcpTransport->ListActWrite.end())
    {
        TcpActWrite_t* Act = &(*it);

        switch (Act->Status)
        {
        case TcpActWriteStatus_Process:
            {
                ++it;
                TcpTransportCancelIo(TcpTransport, Act);
            }
            break;

        case TcpActWriteStatus_Await:
        case TcpActWriteStatus_Complete:
            {
                it = TcpTransport->ListActWrite.erase(it);
                TcpActWriteFree(Act);
                TcpTransportWriteDec(TcpTransport);
            }
            break;

        default:
            ASSERT(false);
            break;
        };
    };

    TcpTransportCancelIo(TcpTransport, &TcpTransport->ActRead);
};


static void TcpTransportCompleteAccept(TcpTransport_t* TcpTransport, TcpActAccept_t* Act, uint32 Bytes, uint32 ErrorCode)
{    
    if (!ErrorCode)
    {
        sockaddr_in* LocalAddr = nullptr;
        sockaddr_in* RemoteAddr = nullptr;
        int32 LocalAddrSize = 0;
        int32 RemoteAddrSize = 0;

        NetWsaFnGetAcceptExSockAddrs(
            Act->RemoteAddrBuffer,
            0,
            sizeof(Act->RemoteAddrBuffer) / 2,
            sizeof(Act->RemoteAddrBuffer) / 2,
            (sockaddr**)&LocalAddr,
            &LocalAddrSize,
            (sockaddr**)&RemoteAddr,
            &RemoteAddrSize
        );

        uint64 RemoteNetAddr = 0;
        NetAddrInit(&RemoteNetAddr, RemoteAddr->sin_addr.s_addr, RemoteAddr->sin_port);
        NetAddrNtoh(&RemoteNetAddr);

        if (NetSockUpdAcceptCtx(TcpTransport->hSocket, Act->hRemoteSocket))
        {
            HOBJ hTcpSessionHost = TcpLayerGetSession(&TcpTransport->Layer);
            ASSERT(hTcpSessionHost != 0);
            
            TcpSessionOpenRemote(hTcpSessionHost, RemoteNetAddr, (void*)Act->hRemoteSocket);
        };

        Act->hRemoteSocket = INVALID_SOCKET;

        if(TcpTransportTestFlag(TcpTransport, TcpTransportFlag_Listen))
            TcpTransportStartAccept(TcpTransport, Act);
    }
    else
    {
        TcpTransportErrOutputLn(ErrorCode, "complete Accept %u", ErrorCode);
        
        NetSockClose(Act->hRemoteSocket);
        Act->hRemoteSocket = INVALID_SOCKET;
    };
};


static void TcpTransportCompleteConnect(TcpTransport_t* TcpTransport, TcpActConnect_t* Act, uint32 Bytes, uint32 ErrorCode)
{
    ASSERT(!TcpLayerIsRemote(&TcpTransport->Layer));

    TcpTmoutRemove(Act);

    if ((!ErrorCode)                                    &&
        NetSockUpdConnectCtx(TcpTransport->hSocket)     &&
        NetSockSetNonblock(TcpTransport->hSocket)       &&
        NetSockSetBuffer(TcpTransport->hSocket, 0, 0)   &&
        NetSockSetNoDelay(TcpTransport->hSocket, true))
    {
        TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Connect, true);

        TcpLayerEventParamConnect_t Param = {};
        Param.Ip = NetAddrIp(&TcpTransport->NetAddr);
        Param.Port = NetAddrPort(&TcpTransport->NetAddr);
        Param.Elapsed = 0;
        Param.FlagRemote = false;

        if (TcpTransportDispatchEvent(TcpTransport, TcpLayerEvent_Connect, &Param))
            TcpTransportStartRead(TcpTransport);
        else
            TcpTransportStartDisconnect(TcpTransport, false);
    }
    else
    {
        TcpTransportClose(TcpTransport);

        TcpLayerEventParamConnectFail_t Param = {};
        Param.Ip = NetAddrIp(&TcpTransport->NetAddr);
        Param.Port = NetAddrPort(&TcpTransport->NetAddr);
        Param.ErrorCode = ErrorCode;
        Param.Label = TcpLayerGetLabel(&TcpTransport->Layer);

        TcpTransportDispatchEvent(TcpTransport, TcpLayerEvent_ConnectFail, &Param);
    };
};


static bool TcpTransportCompleteConnect(TcpTransport_t* TcpTransport, SOCKET hSocket)
{
    ASSERT(TcpLayerIsRemote(&TcpTransport->Layer));

    bool bResult = false;
    TcpLayerRefInc(&TcpTransport->Layer);

    TcpTransport->hSocket = hSocket;
    TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Remote, true);

    if (TcpTransportBindSocketCompletion(TcpTransport))
    {
label_SkipBindIo:
        if (NetSockSetNonblock(TcpTransport->hSocket) &&
            NetSockSetBuffer(TcpTransport->hSocket, 0, 0)   &&
            NetSockSetNoDelay(TcpTransport->hSocket, true))
        {
            TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Connect, true);
            
            TcpLayerEventParamConnect_t Param = {};
            Param.Ip = NetAddrIp(&TcpTransport->NetAddr);
            Param.Port = NetAddrPort(&TcpTransport->NetAddr);
            Param.Elapsed = 0;
            Param.FlagRemote = true;

            if (TcpTransportDispatchEvent(TcpTransport, TcpLayerEvent_Connect, &Param))
                TcpTransportStartRead(TcpTransport);
            else
                TcpTransportStartDisconnect(TcpTransport, false);

            bResult = true;
        }
		else
        {
            uint32 ErrorCode = WSAGetLastError();
            OUTPUTLN("setup socket prop while accept failed %u", ErrorCode);
            ASSERT(false);
            TcpSessionClose(TcpTransport->Layer.hSession);
        };
    }
	else
    {
        uint32 ErrorCode = WSAGetLastError();
        if (ErrorCode == ERROR_INVALID_PARAMETER)
        {
            goto label_SkipBindIo;
        }
        else
        {
            OUTPUTLN("bind socket io while accept failed %u", ErrorCode);
            ASSERT(false);
            TcpSessionClose(TcpTransport->Layer.hSession);
        };
	};

    TcpLayerRefDec(&TcpTransport->Layer);
    
    return bResult;
};


static void TcpTransportCompleteRead(TcpTransport_t* TcpTransport, TcpActRead_t* Act, uint32 Bytes, uint32 ErrorCode)
{
    ASSERT(Bytes == 0, "Not a zero recv = %u", Bytes);

    TcpTmoutRemove(Act);

    if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcPending) ||
        TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcRequest))
        return;

    bool FlagReuse = false;
    
    if (!ErrorCode)
    {
        char Buffer[4096];
        
        int32 iResult = recv(TcpTransport->hSocket, Buffer, sizeof(Buffer), 0);
        if (iResult > 0)
        {
            Bytes = uint32(iResult);

            TcpLayerEventParamRead_t Param = {};
            Param.Data = Buffer;
            Param.DataSize = Bytes;
            Param.ErrorCode = ErrorCode;
            Param.Received = Bytes;

            if (!TcpTransportDispatchEvent(TcpTransport, TcpLayerEvent_Read, &Param))
                Bytes = 0;
        }
        else if (iResult <= 0)
        {
            //
            //  Got EOF - passive close, we can reuse socket
            //
            FlagReuse = true;
        };
    }
    else
    {
        FlagReuse = true;
    };

    if (Bytes)
        TcpTransportStartRead(TcpTransport);
    else
        TcpTransportStartDisconnect(TcpTransport, FlagReuse);
};


static void TcpTransportCompleteWrite(TcpTransport_t* TcpTransport, TcpActWrite_t* Act, uint32 Bytes, uint32 ErrorCode)
{    
    if (!ErrorCode)
    {
		{
			TcpLayerEventParamWrite_t Param = {};
			Param.Data = Act->Buffer;
			Param.DataSize = Bytes;
			Param.ErrorCode = ErrorCode;

			TcpTransportDispatchEvent(TcpTransport, TcpLayerEvent_Write, &Param);
		}

        ASSERT(Bytes == Act->Size);

        {
            std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

            Act->Status = TcpActWriteStatus_Complete;
            
            if ((Act->Generation == TcpTransport->Generation) &&
                Act->FlagDisconnectOnComplete)
            {
                ASSERT(TcpTransport->DisconnectOnCompleteCounter > 0);
                if (!--TcpTransport->DisconnectOnCompleteCounter)
                {
                    TcpTransportStartDisconnect(TcpTransport, false);
                    return;
                };
            };

            TcpTransportStartWrite(TcpTransport);
        }
    }
    else
    {
        TcpTransportStartDisconnect(TcpTransport, false);
    };
};


static void TcpTransportCompleteDisconnect(TcpTransport_t* TcpTransport, TcpActDisconnect_t* Act, uint32 ErrorCode)
{
    TcpTmoutRemove(Act);

    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    ASSERT(TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcPending));

    //
    //  Socket may be closed before dc act completion by user end ref so check it out
    //
    if (TcpTransportIsOpen(TcpTransport))
    {
        if (!ErrorCode)
        {
            if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcReuse))
            {
                TcpTransportSocketReuseProc(TcpTransport);

                if (TcpLayerIsRemote(&TcpTransport->Layer))
                    TcpSockPoolFreeUnbound(TcpTransport->hSocket);
                else
                    TcpSockPoolFreeBound(TcpTransport->hSocket);

                TcpTransport->hSocket = INVALID_SOCKET;
            }
            else
            {
                TcpTransportClose(TcpTransport);
            };
        }
        else
        {
            TcpTransportClose(TcpTransport);
        };
    };

    TcpTransportCleanupPerConnect(TcpTransport);
    ++TcpTransport->Generation;

    TcpLayerEventParamDisconnect_t Param = {};
    Param.ErrorCode = ErrorCode;
    Param.FlagRemote = TcpTransportTestFlag(TcpTransport, TcpTransportFlag_Remote);
    
    TcpTransportDispatchEvent(TcpTransport, TcpLayerEvent_Disconnect, &Param);
};


static bool TcpTransportStartAccept(TcpTransport_t* TcpTransport, TcpActAccept_t* Act)
{
    ASSERT(TcpTransportIsOpen(TcpTransport));
    ASSERT(Act->hRemoteSocket == INVALID_SOCKET);

    bool bResult = false;    

    std::memset(Act, 0x00, sizeof(*Act));
    Act->Type = TcpActType_Accept;
    Act->Transport = TcpTransport;
    Act->hRemoteSocket = TcpSockPoolAllocUnbound();

    if (Act->hRemoteSocket != INVALID_SOCKET)
    {
        const int32 AddrLen = sizeof(Act->RemoteAddrBuffer) / 2;

        TcpTransportIoInc(TcpTransport);
        if (!NetWsaFnAcceptEx(
            TcpTransport->hSocket,
            Act->hRemoteSocket,
            Act->RemoteAddrBuffer,
            0,
            AddrLen,
            AddrLen,
            0,
            &Act->Ovl))
        {
            uint32 ErrorCode = WSAGetLastError();
            if (ErrorCode != WSA_IO_PENDING)
            {
                TcpTransportErrOutputLn(ErrorCode, "call Accept %u", ErrorCode);

                NetSockClose(Act->hRemoteSocket);
                Act->hRemoteSocket = INVALID_SOCKET;

                TcpTransportIoDec(TcpTransport);
            }
            else
            {
                bResult = true;
            };
        }
        else
        {
            bResult = true;
        };
    }
    else
    {
        uint32 ErrorCode = WSAGetLastError();
        TcpTransportErrOutputLn(ErrorCode, "start Accept %u", ErrorCode);
    };

    return bResult;
};


static bool TcpTransportStartConnect(TcpTransport_t* TcpTransport, uint32 Timeout)
{
    ASSERT(NetAddrIsValid(&TcpTransport->NetAddr));

    bool bResult = false;
    
    Timeout = Max(NetSettings.TcpConnDefTimeout, Timeout);

    uint64 Endpoint = TcpTransport->NetAddr;
    NetAddrHton(&Endpoint);

    sockaddr_in Addr = {};
    Addr.sin_addr.s_addr= NetAddrIp(&Endpoint);
    Addr.sin_port       = NetAddrPort(&Endpoint);
    Addr.sin_family     = AF_INET;

    std::memset(&TcpTransport->ActConnect, 0x00, sizeof(TcpTransport->ActConnect));
    TcpTransport->ActConnect.Type = TcpActType_Connect;
    TcpTransport->ActConnect.Transport = TcpTransport;
    TcpTransport->ActConnect.hSocket = TcpTransport->hSocket;
    TcpTransport->ActConnect.Timeout = Timeout;
    TcpTransport->ActConnect.TimeStart = 0;
    TcpTransport->ActConnect.TimeoutFlags = 0;
    TcpTransport->ActConnect.FlagTimeoutProc = false;
    TcpTmoutRegist(&TcpTransport->ActConnect);
    
    TcpTransportIoInc(TcpTransport);
    if (NetWsaFnConnectEx(
        TcpTransport->hSocket,
        LPSOCKADDR(&Addr),
        sizeof(Addr),
        nullptr,
        0,
        0,
        &TcpTransport->ActConnect.Ovl))
    {
        bResult = true;
    }
    else
    {
        uint32 ErrorCode = WSAGetLastError();
        if (ErrorCode == WSA_IO_PENDING)
        {
            bResult = true;            
        }
        else
        {
            TcpTransportErrOutputLn(ErrorCode, "call ConnectEx %u", ErrorCode);
            TcpTmoutRemove(&TcpTransport->ActConnect);
            TcpTransportIoDec(TcpTransport);
        };
    };

    return bResult;
};


static void TcpTransportStartRead(TcpTransport_t* TcpTransport)
{
    if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcPending) ||
        TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcRequest))
        return;

    std::memset(&TcpTransport->ActRead, 0x00, sizeof(TcpTransport->ActRead));
    TcpTransport->ActRead.Type = TcpActType_Read;
    TcpTransport->ActRead.Transport = TcpTransport;
    TcpTransport->ActRead.hSocket = TcpTransport->hSocket;
    TcpTransport->ActRead.Timeout = Max(TcpTransport->TimeoutRead, 1000u);
    TcpTransport->ActRead.TimeStart = 0;
    TcpTransport->ActRead.TimeoutFlags = 0;
    TcpTransport->ActRead.FlagTimeoutProc = false;
    TcpTmoutRegist(&TcpTransport->ActRead);

    WSABUF WsaBuff = {};
    uint32 Bytes = 0;
    uint32 Flags = 0;

    TcpTransportIoInc(TcpTransport);
    int32 iResult = WSARecv(TcpTransport->hSocket, &WsaBuff, 1, LPDWORD(&Bytes), LPDWORD(&Flags), &TcpTransport->ActRead.Ovl, NULL);    
    if (iResult == 0)
    {
        //
        //  Success
        //
    }
    else if (iResult == SOCKET_ERROR)
    {
        uint32 ErrorCode = GetLastError();
        if (ErrorCode == WSA_IO_PENDING)
        {
            ;
        }
        else
        {
            TcpTransportErrOutputLn(ErrorCode, "call WSARecv %u", ErrorCode);
            TcpTmoutRemove(&TcpTransport->ActRead);
            TcpTransportStartDisconnect(TcpTransport, false);
            TcpTransportIoDec(TcpTransport);
        };        
    };
};


static void TcpTransportStartWrite(TcpTransport_t* TcpTransport)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    int32 WriteNum = 0;
    bool bFailure = false;
    
    auto it = TcpTransport->ListActWrite.begin();
    while (it != TcpTransport->ListActWrite.end())
    {
        if (bFailure)
            break;

        TcpActWrite_t* Act = &(*it);
		if (Act->Generation < TcpTransport->Generation)
			Act->Status = TcpActWriteStatus_Complete;

        switch (Act->Status)
        {
        case TcpActWriteStatus_Complete:
            {
                it = TcpTransport->ListActWrite.erase(it);
                TcpActWriteFree(Act);
                TcpTransportWriteDec(TcpTransport);
            }
            break;

        case TcpActWriteStatus_Process:
            {
                ++it;
            }
            break;

        case TcpActWriteStatus_Await:
            {
                WSABUF WsaBuf = {};
                WsaBuf.buf = PCHAR(Act->Buffer);
                WsaBuf.len = ULONG(Act->Size);

                uint32 Bytes = 0;
                uint32 Flags = 0;

                TcpTransportIoInc(TcpTransport);
                int32 iResult = WSASend(TcpTransport->hSocket, &WsaBuf, 1, LPDWORD(&Bytes), Flags, &Act->Ovl, NULL);
                if (iResult == 0)
                {
                    ASSERT(Bytes == Act->Size);
                    Act->Status = TcpActWriteStatus_Complete;
                    ++it;
                }
                else if (iResult == SOCKET_ERROR)
                {
                    uint32 ErrorCode = WSAGetLastError();
                    if (ErrorCode == WSA_IO_PENDING)
                    {
                        Act->Status = TcpActWriteStatus_Process;
                        ++it;
                    }
                    else
                    {
                        it = TcpTransport->ListActWrite.erase(it);
                        TcpActWriteFree(Act);
                        TcpTransportWriteDec(TcpTransport);
                        TcpTransportErrOutputLn(ErrorCode, "call WSASend %u", ErrorCode);
                        TcpTransportStartDisconnect(TcpTransport, false);
                        TcpTransportIoDec(TcpTransport);
                        bFailure = true;
                    };
                };
            }
            break;

        default:
            ASSERT(false);
            break;
        };
    };
};


static bool TcpTransportStartDisconnect(TcpTransport_t* TcpTransport, bool FlagReuse)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);
    
    bool bResult = false;

    if (TcpTransport->hSocket == INVALID_SOCKET)
        return bResult;

    if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_DcPending))
        return bResult;

    TcpTransportSetFlag(TcpTransport, TcpTransportFlag_DcPending, true);
    TcpTransportSetFlag(TcpTransport, TcpTransportFlag_DcReuse, FlagReuse);

    std::memset(&TcpTransport->ActDisconnect, 0x00, sizeof(TcpTransport->ActDisconnect));
    TcpTransport->ActDisconnect.Type = TcpActType_Disconnect;
    TcpTransport->ActDisconnect.Transport = TcpTransport;
    TcpTransport->ActDisconnect.hSocket = TcpTransport->hSocket;
    TcpTransport->ActDisconnect.Timeout = NetSettings.TcpDcTimeout;
    TcpTransport->ActDisconnect.TimeStart = 0;
    TcpTransport->ActDisconnect.TimeoutFlags = 0;
    TcpTransport->ActDisconnect.FlagTimeoutProc = false;
    TcpTmoutRegist(&TcpTransport->ActDisconnect);
    
    TcpTransportIoInc(TcpTransport);
    if (NetWsaFnDisconnectEx(TcpTransport->hSocket, &TcpTransport->ActDisconnect.Ovl, (FlagReuse ? TF_REUSE_SOCKET : 0), 0))
    {
        bResult = true;
    }
    else
    {
        uint32 ErrorCode = WSAGetLastError();
        if (ErrorCode == WSA_IO_PENDING)
        {
            bResult = true;
        }
        else
        {
            TcpTransportErrOutputLn(ErrorCode, "call DisconnectEx %u", ErrorCode);
            TcpTransportSetFlag(TcpTransport, TcpTransportFlag_DcPending | TcpTransportFlag_DcReuse, false);
            TcpTmoutRemove(&TcpTransport->ActDisconnect);
            TcpTransportClose(TcpTransport);
            TcpTransportIoDec(TcpTransport);
        };
    };

    return bResult;
};


static bool TcpTransportCreateSendNode(TcpTransport_t* TcpTransport, const char* Data, int32 Size, bool FlagDisconnectOnComplete)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    if (TcpTransport->DisconnectOnCompleteCounter > 0)
        return false;

    if (!Data || !Size)
    {
        TcpTransportStartDisconnect(TcpTransport, false);
        return false;
    };

    if (TcpTransport->DisconnectOnCompleteCounter > 0)
    {
        OUTPUTLN("Attempt to send data after requested disconnect on complete!");
        ASSERT(false);
        return false;
    };

    int32 Count = 0;
    int32 Written = 0;
    bool FlagIsListWasEmpty = TcpTransport->ListActWrite.empty();
    
    do
    {
        TcpActWrite_t* Act = TcpActWriteAlloc();
        ASSERT(Act);
        std::memset(Act, 0x00, sizeof(*Act));

        ASSERT(Written < Size);
        int32 Remain = (Size - Written);
        int32 Write = Min(int32(sizeof(Act->Buffer)), Remain);

        std::memcpy(Act->Buffer, &Data[Written], Write);

        Written += Write;
        ++Count;

        Act->Type = TcpActType_Write;
        Act->Transport = TcpTransport;
        Act->Status = TcpActWriteStatus_Await;
		Act->Size = Write;
        Act->FlagDisconnectOnComplete = FlagDisconnectOnComplete;
        Act->WriteNode.data = Act;
        Act->Generation = TcpTransport->Generation;

        TcpTransport->ListActWrite.push_back(&Act->WriteNode);
        TcpTransportWriteInc(TcpTransport);
    } while (Written != Size);

    if ((!TcpTransport->DisconnectOnCompleteCounter) && FlagDisconnectOnComplete)
        TcpTransport->DisconnectOnCompleteCounter = Count;

    if (FlagIsListWasEmpty)
        TcpTransportStartWrite(TcpTransport);

    return true;
};


static bool TcpTransportConnect(TcpTransport_t* TcpTransport, uint32 Ip, uint16 Port, uint32 Timeout)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);
    
    bool bResult = false;

    if (TcpTransportIsOpen(TcpTransport))
        return bResult;
    
	TcpTransport->hSocket = TcpSockPoolAllocBound();
    if (TcpTransport->hSocket != INVALID_SOCKET)
    {
        if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_ForceClose))
            NetSockSetLinger(TcpTransport->hSocket, true, 0);
        
        if (TcpTransportBindSocketCompletion(TcpTransport))
        {
        label_SkipBindIo:
            NetAddrInit(&TcpTransport->NetAddr, Ip, Port);
            bResult = TcpTransportStartConnect(TcpTransport, Timeout);
        }
        else
        {
            uint32 ErrorCode = WSAGetLastError();
            if (ErrorCode == ERROR_INVALID_PARAMETER)
            {
                goto label_SkipBindIo;
            }
            else
            {
                OUTPUTLN("bind socket io while connect failed %u", ErrorCode);
            };
        };
    };

    if (!bResult)
        TcpTransportClose(TcpTransport);

    return bResult;
};


static bool TcpTransportCancelConnect(TcpTransport_t* TcpTransport)
{
    if (!TcpTransportIsOpen(TcpTransport))
        return false;

    return TcpTransportCancelIo(TcpTransport, &TcpTransport->ActConnect);
};


static bool TcpTransportListen(TcpTransport_t* TcpTransport, uint32 Ip, uint16 Port)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    bool bResult = false;
    
    if (TcpTransportIsOpen(TcpTransport))
        return bResult;

    NetAddrInit(&TcpTransport->NetAddr, Ip, Port);
    uint64 ListenAddr = TcpTransport->NetAddr;
    NetAddrHton(&ListenAddr);
    
    TcpTransport->hSocket = NetSockNewListen(ListenAddr);
    if (TcpTransport->hSocket != INVALID_SOCKET)
    {
        if (TcpTransportBindSocketCompletion(TcpTransport))
        {
            TcpTransport->ActAcceptArraySize = NetSettings.TcpAcceptActPoolSize;
            TcpTransport->ActAcceptArray = new TcpActAccept_t[TcpTransport->ActAcceptArraySize];
            ASSERT(TcpTransport->ActAcceptArray);

            for (int32 i = 0; i < TcpTransport->ActAcceptArraySize; ++i)
            {
                TcpActAccept_t* TcpActAccept = &TcpTransport->ActAcceptArray[i];

                std::memset(TcpActAccept, 0x00, sizeof(*TcpActAccept));
                TcpActAccept->Type = TcpActType_Accept;
                TcpActAccept->Transport = TcpTransport;
                TcpActAccept->hRemoteSocket = INVALID_SOCKET;
            };

            bResult = TcpTransportListenEnable(TcpTransport, true);
        };
    };

    if (!bResult)
    {
        TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Listen, false);

        if (TcpTransport->ActAcceptArray)
        {
            delete[] TcpTransport->ActAcceptArray;
            TcpTransport->ActAcceptArray = nullptr;

            TcpTransport->ActAcceptArraySize = 0;
        };

        TcpTransportClose(TcpTransport);
    };

    return bResult;
};


static bool TcpTransportListenEnable(TcpTransport_t* TcpTransport, bool Flag)
{
    std::lock_guard<std::recursive_mutex> Lock(TcpTransport->Mutex);

    if (!TcpTransportIsOpen(TcpTransport))
        return false;

    if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_Listen) == Flag)
        return false;

    TcpTransportSetFlag(TcpTransport, TcpTransportFlag_Listen, Flag);

    for (int32 i = 0; i < TcpTransport->ActAcceptArraySize; ++i)
    {
        if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_Listen))
            TcpTransportStartAccept(TcpTransport, &TcpTransport->ActAcceptArray[i]);
        else
            TcpTransportCancelIo(TcpTransport, &TcpTransport->ActAcceptArray[i]);
    };

    return true;
};


static bool TcpTransportWrite(TcpTransport_t* TcpTransport, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete)
{
    if (!TcpTransportIsOpen(TcpTransport))
        return false;

    return TcpTransportCreateSendNode(TcpTransport, Data, DataSize, FlagDisconnectOnComplete);
};


static bool TcpTransportDisconnect(TcpTransport_t* TcpTransport)
{
    if (!TcpTransportIsOpen(TcpTransport))
        return false;

    if (!TcpTransportTestFlag(TcpTransport, TcpTransportFlag_Connect))
        return false;

    if (TcpTransportTestFlag(TcpTransport, TcpTransportFlag_Dispatch))
    {
        //
        //  Morph command to request if we are dispatching some event
        //  Disconnect request will be handled in event proc in post dispatch phase
        //     
        TcpTransportSetFlag(TcpTransport, TcpTransportFlag_DcRequest, true);
        return true;
    }
    else
    {
        return TcpTransportStartDisconnect(TcpTransport, false);
    };
};


static bool TcpTransportCmdProc(TcpLayer_t* TcpLayer, int32 Id, void* Param)
{
    bool bResult = false;
    TcpTransport_t* TcpTransport = (TcpTransport_t*)TcpLayer;        
    
    switch (Id)
    {
    case TcpLayerCmd_Connect:
        {
            TcpLayerCmdParamConnect_t* CmdParam = (TcpLayerCmdParamConnect_t*)Param;
            if (!CmdParam->FlagCancel)
                bResult = TcpTransportConnect(TcpTransport, CmdParam->Ip, CmdParam->Port, CmdParam->Timeout);
            else
                bResult = TcpTransportCancelConnect(TcpTransport);
        }
        break;

    case TcpLayerCmd_Listen:
        {
            TcpLayerCmdParamListen_t* CmdParam = (TcpLayerCmdParamListen_t*)Param;
            if (CmdParam->FlagStart)
                bResult = TcpTransportListen(TcpTransport, CmdParam->Ip, CmdParam->Port);
            else
                bResult = TcpTransportListenEnable(TcpTransport, CmdParam->FlagEnable);
        }
        break;

    case TcpLayerCmd_Write:
        {
            TcpLayerCmdParamWrite_t* CmdParam = (TcpLayerCmdParamWrite_t*)Param;
            bResult = TcpTransportWrite(TcpTransport, CmdParam->Data, CmdParam->DataSize, CmdParam->FlagDisconnectOnComplete);
        }
        break;

    case TcpLayerCmd_Disconnect:
        {
            TcpLayerCmdParamDisconnect_t* CmdParam = (TcpLayerCmdParamDisconnect_t*)Param;
            bResult = TcpTransportDisconnect(TcpTransport);
        }
        break;

    case TcpTransportCmd_CheckOpen:
        {
            *(bool*)Param = TcpTransportIsOpen(TcpTransport);
            bResult = true;
        }
        break;

    case TcpTransportCmd_CompleteRemoteConn:
        {
            bResult = TcpTransportCompleteConnect(TcpTransport, SOCKET(Param));
        }
        break;

    case TcpTransportCmd_UserClose:
        {
            TcpTransportClose(TcpTransport);
            bResult = true;
        }
        break;

    case TcpTransportCmd_SetTimeoutRead:
        {
            uint32 TimeoutValue = uint32(Param);
            
            TcpTransport->TimeoutRead = (TimeoutValue ? TimeoutValue : NetSettings.TcpReadTimeout);
            TcpTransport->ActRead.Timeout = TimeoutValue;
            
            bResult = true;
        }
        break;

    case TcpTransportCmd_SetForceClose:
        {
            TcpTransportSetFlag(TcpTransport, TcpTransportFlag_ForceClose, bool(Param != nullptr));

            bResult = true;
        }
        break;
    };

    return bResult;
};


struct TcpLayer_t* TcpTransportCreate(void* Param)
{
    TcpTransport_t* TcpTransport = new TcpTransport_t();
    if (!TcpTransport)
        return 0;

    TcpTransportStartupPerSession(TcpTransport);

    return &TcpTransport->Layer;
};


void TcpTransportDestroy(struct TcpLayer_t* TcpLayer)
{
    TcpTransport_t* TcpTransport = (TcpTransport_t*)TcpLayer;

    TcpTransportCleanupPerSession(TcpTransport);
	
    delete TcpTransport;
};