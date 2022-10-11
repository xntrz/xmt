#include "TcpSession.hpp"
#include "TcpLayer.hpp"
#include "TcpTransport.hpp"
#include "TcpProxy.hpp"
#include "TcpSsl.hpp"
#include "TcpStats.hpp"
#include "NetAddr.hpp"

#include "Shared/Common/Event.hpp"


static const int32 DISPATCH_DEPTH_MAX = 12;
static const int32 COMMAND_RECEIVER_ALL = -1;


enum TcpSessionDispatch_t
{
    TcpSessionDispatch_Event = 0,
    TcpSessionDispatch_Cmd,
};


struct TcpSession_t
{
    CList<TcpLayer_t> ListLayer;
    std::atomic<int32> RefUser;
    std::atomic<int32> RefLayer;
    std::atomic<int32> RefTotal;
    std::atomic<uint64> NetAddr;
    std::atomic<void*> UserParam;
    std::atomic<int16> DepthEvent;
    std::atomic<int16> DepthCmd;
    std::atomic<int32> ProxyLabel;
    std::recursive_mutex EventProcMutex;
    NetEventProc_t EventProc;
    void* EventProcParam;
    bool FlagRemote;
    bool FlagListen;
    bool FlagNotifySent;
};


//
//  Since event/cmd order is important then layer label enum order too
//  Bottom layer is first, top - last
//
//  Note: only one proxy layer can be attached at once for session
// 
enum TcpLayerLabel_t
{
    TcpLayerLabel_Invalid = -1,
    TcpLayerLabel_Transport = 0,
    TcpLayerLabel_Stats,
    TcpLayerLabel_PrxHttp,
    TcpLayerLabel_PrxSocks4,
	TcpLayerLabel_PrxSocks4a,
    TcpLayerLabel_PrxSocks5,
    TcpLayerLabel_Ssl,
};


struct TcpLayerInfo_t
{
    TcpLayer_t*(*FnCreate)(void* Param);
    void(*FnDestroy)(TcpLayer_t* TcpLayer);
};


//
//  Proxy layer share same create/destroy callback, accept proxy type as create Param
//
static const TcpLayerInfo_t TcpLayerInfoTable[] =
{
    { TcpTransportCreate,   TcpTransportDestroy },  // transport
    { TcpStatsCreate,       TcpStatsDestroy     },  // stats
    { TcpProxyCreate,       TcpProxyDestroy     },  // http
    { TcpProxyCreate,       TcpProxyDestroy     },  // socks4
	{ TcpProxyCreate,       TcpProxyDestroy		},  // socks4a
    { TcpProxyCreate,       TcpProxyDestroy     },  // socks5
    { TcpSslCreate,         TcpSslDestroy       },  // ssl/tls
};


static inline TcpLayer_t* TcpSessionLayerCreate(int32 Label, void* Param)
{
    ASSERT(Label != TcpLayerLabel_Invalid);
    
    if (Label >= 0 && Label < COUNT_OF(TcpLayerInfoTable))
    {
        if (TcpLayerInfoTable[Label].FnCreate)
            return TcpLayerInfoTable[Label].FnCreate(Param);
    };

    return nullptr;
};


static inline void TcpSessionLayerDestroy(int32 Label, TcpLayer_t* TcpLayer)
{
    ASSERT(Label != TcpLayerLabel_Invalid);
    
    if (Label >= 0 && Label < COUNT_OF(TcpLayerInfoTable))
    {
        if (TcpLayerInfoTable[Label].FnDestroy)
            TcpLayerInfoTable[Label].FnDestroy(TcpLayer);
    };
};


static inline TcpLayer_t* TcpSessionLayerSearch(TcpSession_t* TcpSession, int32 Label)
{
    ASSERT(Label != TcpLayerLabel_Invalid);
    
    for (auto& it : TcpSession->ListLayer)
    {
        if (it.Label == Label)
            return &it;
    };

    return nullptr;
};


static bool TcpSessionLayerRegist(TcpSession_t* TcpSession, int32 Label, void* Param = nullptr)
{
    ASSERT(Label != TcpLayerLabel_Invalid);
    
    bool bResult = false;
    TcpLayer_t* TcpLayer = TcpSessionLayerCreate(Label, Param);
    if (TcpLayer)
    {
		TcpLayer->hSession = TcpSession;
		TcpLayer->Label = Label;

        //
        //  Insert layer by label order or just push back if list empty
        //
        if (TcpSession->ListLayer.empty())
        {
            TcpSession->ListLayer.push_front(TcpLayer);
        }
        else
        {
            auto it = TcpSession->ListLayer.begin();
            while (it != TcpSession->ListLayer.end())
            {
                if (TcpLayerGetLabel(&*it) < TcpLayerGetLabel(TcpLayer))
                    break;
                
                ++it;
            };

            TcpSession->ListLayer.insert(it, TcpLayer);
        };

        bResult = true;
    }
    else
    {
        OUTPUTLN("Attempt regist not existing layer with label %d", Label);
    };

    return bResult;
};


static void TcpSessionLayerRemove(TcpSession_t* TcpSession, int32 Label)
{
    ASSERT(Label != TcpLayerLabel_Invalid);
    
    TcpLayer_t* TcpLayer = TcpSessionLayerSearch(TcpSession, Label);
    if (TcpLayer)
    {
        TcpSession->ListLayer.erase(TcpLayer);
        TcpSessionLayerDestroy(Label, TcpLayer);
    }
    else
    {
        OUTPUTLN("Attempt remove not existing layer with label %d", Label);
    };
};


static inline TcpLayer_t* TcpSessionTopLayer(TcpSession_t* TcpSession)
{
    return TcpSession->ListLayer.front();
};


static inline void TcpSessionDispatchBegin(TcpSession_t* TcpSession, TcpSessionDispatch_t Type)
{
    if (Type == TcpSessionDispatch_Event)
    {
        ASSERT(TcpSession->DepthEvent < DISPATCH_DEPTH_MAX);
        ++TcpSession->DepthEvent;
    }
    else if (Type == TcpSessionDispatch_Cmd)
    {
        ASSERT(TcpSession->DepthCmd < DISPATCH_DEPTH_MAX);
        ++TcpSession->DepthCmd;
    }
    else
    {
        ASSERT(false);
    };
};


static inline void TcpSessionDispatchEnd(TcpSession_t* TcpSession, TcpSessionDispatch_t Type)
{
    if (Type == TcpSessionDispatch_Event)
    {
        ASSERT(TcpSession->DepthEvent > 0);
        --TcpSession->DepthEvent;        
    }
    else if (Type == TcpSessionDispatch_Cmd)
    {
        ASSERT(TcpSession->DepthCmd > 0);
        --TcpSession->DepthCmd;
    }
    else
    {
        ASSERT(false);
    };
};


static inline TcpLayerLabel_t TcpSessionProxyToLabel(NetProxy_t Proxytype)
{
    switch (Proxytype)
    {
    case NetProxy_Http:
        return TcpLayerLabel_PrxHttp;

    case NetProxy_Socks4:
        return TcpLayerLabel_PrxSocks4;

	case NetProxy_Socks4a:
		return TcpLayerLabel_PrxSocks4a;

    case NetProxy_Socks5:
        return TcpLayerLabel_PrxSocks5;

    default:
        ASSERT(false);
        return TcpLayerLabel_Invalid;
    };
};


static inline void TcpSessionTotalRefInc(TcpSession_t* TcpSession)
{
    ++TcpSession->RefTotal;
};


static inline void TcpSessionTotalRefDec(TcpSession_t* TcpSession)
{
	ASSERT(TcpSession->RefTotal > 0);
    if (!--TcpSession->RefTotal)
    {
        TcpSessionSetEventProc(TcpSession, nullptr);
        TcpSession->UserParam = nullptr;
        
        ASSERT(TcpSession->DepthCmd == 0, "%u", TcpSession->DepthCmd);
        ASSERT(TcpSession->DepthEvent == 0, "%u", TcpSession->DepthEvent);

        auto it = TcpSession->ListLayer.begin();
        while (it != TcpSession->ListLayer.end())
        {
            TcpLayer_t* TcpLayer = &(*it);
            it = TcpSession->ListLayer.erase(it);
            TcpSessionLayerDestroy(TcpLayer->Label, TcpLayer);
        };

        delete TcpSession;
    };
};


static inline void TcpSessionUserRefInc(TcpSession_t* TcpSession)
{
    ++TcpSession->RefUser;
};


static inline void TcpSessionUserRefDec(TcpSession_t* TcpSession)
{
    if (TcpSession->RefUser)
    {
        ASSERT(TcpSession->RefUser > 0);
        if (!--TcpSession->RefUser)
        {
            //
            //  By using eventproc locking we can guaranteed there is no worker in user proc when ref ends
            //
            TcpSessionSetEventProc(TcpSession, nullptr);
            TcpSession->UserParam = nullptr;

            TcpSessionSendCmd(TcpSession, nullptr, TcpLayerLabel_Transport, TcpTransportCmd_UserClose);

            TcpSessionTotalRefDec(TcpSession);
        };
    };
};


HOBJ TcpSessionOpenRemote(HOBJ hTcpSessionHost, uint64 NetAddr, void* Param)
{
    TcpSession_t* TcpSessionHost = (TcpSession_t*)hTcpSessionHost;
    ASSERT(TcpSessionHost);
    
    TcpSession_t* TcpSessionRemote = new TcpSession_t();
    if (!TcpSessionRemote)
        return 0;

    TcpSessionRemote->NetAddr = NetAddr;
    TcpSessionSetEventProc(TcpSessionRemote, TcpSessionHost->EventProc, TcpSessionHost->EventProcParam);
    TcpSessionRemote->FlagRemote = true;
    TcpSessionRemote->FlagNotifySent = TcpSessionHost->FlagNotifySent;
    TcpSessionRemote->RefUser = 1;
    TcpSessionRemote->RefLayer = 0;
    TcpSessionRemote->RefTotal = 0;
    TcpSessionRemote->ProxyLabel = TcpLayerLabel_Invalid;

    TcpSessionLayerRegist(TcpSessionRemote, TcpLayerLabel_Transport);
#ifdef _DEBUG
    TcpSessionLayerRegist(TcpSessionRemote, TcpLayerLabel_Stats);
#endif    

    if (TcpSessionLayerSearch(TcpSessionHost, TcpLayerLabel_Ssl))
        TcpSessionLayerRegist(TcpSessionRemote, TcpLayerLabel_Ssl);

    TcpSessionSendCmd(
        TcpSessionRemote,
        nullptr,
        TcpLayerLabel_Transport,
        TcpTransportCmd_CompleteRemoteConn,
        Param
    );

    TcpSessionTotalRefInc(TcpSessionRemote);

    return TcpSessionRemote;
};


HOBJ TcpSessionOpen(NetEventProc_t EventProc, void* EventProcParam)
{
    TcpSession_t* TcpSession = new TcpSession_t();
    if (!TcpSession)
        return 0;

    TcpSession->EventProc = EventProc;
    TcpSession->EventProcParam = EventProcParam;
    TcpSession->FlagRemote = false;
    TcpSession->FlagNotifySent = false;
    TcpSession->RefUser = 1;
    TcpSession->RefLayer = 0;
    TcpSession->RefTotal = 0;
    TcpSession->ProxyLabel = TcpLayerLabel_Invalid;
    TcpSessionLayerRegist(TcpSession, TcpLayerLabel_Transport);
#ifdef _DEBUG
    TcpSessionLayerRegist(TcpSession, TcpLayerLabel_Stats);
#endif    

    TcpSessionTotalRefInc(TcpSession);
    
    return TcpSession;
};


void TcpSessionClose(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;
    
    TcpSessionUserRefDec(TcpSession);
};


void TcpSessionCopy(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    TcpSessionUserRefInc(TcpSession);
};


void TcpSessionRefInc(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    if (!TcpSession->RefLayer++)
        TcpSessionTotalRefInc(TcpSession);
};


void TcpSessionRefDec(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    ASSERT(TcpSession->RefLayer > 0);
    if (!--TcpSession->RefLayer)
        TcpSessionTotalRefDec(TcpSession);    
};


bool TcpSessionConnect(HOBJ hTcpSession, uint32 Ip, uint16 Port, uint32 Timeout)
{
    TcpLayerCmdParamConnect_t Param = {};
    Param.Ip = Ip;
    Param.Port = Port;
    Param.Timeout = Timeout;
    Param.FlagCancel = false;

    return TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerCmd_Connect, &Param);
};


bool TcpSessionCancelConnect(HOBJ hTcpSession)
{
    TcpLayerCmdParamConnect_t Param = {};
    Param.FlagCancel = true;

    return TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerCmd_Connect, &Param);
};


bool TcpSessionListen(HOBJ hTcpSession, uint32 Ip, uint16 Port)
{
    TcpLayerCmdParamListen_t Param = {};
    Param.Ip = Ip;
    Param.Port = Port;
    Param.FlagStart = true;
    Param.FlagEnable = true;

	if (TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerCmd_Listen, &Param))
	{
		((TcpSession_t*)hTcpSession)->FlagListen = true;
		return true;
	};

	return false;
};


bool TcpSessionListenEnable(HOBJ hTcpSession, bool Flag)
{
    TcpLayerCmdParamListen_t Param = {};
    Param.FlagStart = false;
    Param.FlagEnable = Flag;

    return TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerCmd_Listen, &Param);
};


bool TcpSessionWrite(HOBJ hTcpSession, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete)
{
    TcpLayerCmdParamWrite_t Param = {};
    Param.Data = Data;
    Param.DataSize = DataSize;
    Param.FlagDisconnectOnComplete = FlagDisconnectOnComplete;

    return TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerCmd_Write, &Param);
};


bool TcpSessionDisconnect(HOBJ hTcpSession)
{
    TcpLayerCmdParamDisconnect_t Param = {};    

    return TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerCmd_Disconnect, &Param);
};


bool TcpSessionProxySet(HOBJ hTcpSession, NetProxy_t Proxytype, uint64 NetAddr, const void* Parameter, int32 ParameterLen)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;
    bool bResult = false;
    
    if (TcpSessionIsConnected(TcpSession))
        return bResult;

    if (TcpSessionLayerSearch(TcpSession, TcpLayerLabel_PrxHttp)    ||
        TcpSessionLayerSearch(TcpSession, TcpLayerLabel_PrxSocks4)  ||
        TcpSessionLayerSearch(TcpSession, TcpLayerLabel_PrxSocks5))
        return bResult;

    ASSERT(TcpSession->ProxyLabel == TcpLayerLabel_Invalid);
    if (!TcpSessionLayerRegist(TcpSession, TcpSessionProxyToLabel(Proxytype), (void*)Proxytype))
        return bResult;

    TcpSession->ProxyLabel = TcpSessionProxyToLabel(Proxytype);

    TcpProxySetParam_t Param = {};
    Param.NetAddr = NetAddr;
    Param.Parameter = Parameter;
    Param.ParemeterLen = ParameterLen;    
    bResult = TcpSessionSendCmd(TcpSession, nullptr, TcpSession->ProxyLabel, TcpProxyCmd_SetParam, &Param);
    if (!bResult)
    {
        TcpSessionLayerRemove(TcpSession, TcpSession->ProxyLabel);
        TcpSession->ProxyLabel = TcpLayerLabel_Invalid;
        bResult = false;
    };

    return bResult;
};


bool TcpSessionProxyClear(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;
    bool bResult = false;
    
    if (TcpSessionIsConnected(TcpSession))
        return bResult;

	TcpSession->ProxyLabel = TcpLayerLabel_Invalid;

    if (TcpSessionLayerSearch(TcpSession, TcpLayerLabel_PrxHttp))
    {
        TcpSessionLayerRemove(TcpSession, TcpLayerLabel_PrxHttp);
        bResult = true;
    };
    
    if (TcpSessionLayerSearch(TcpSession, TcpLayerLabel_PrxSocks4))
    {
        TcpSessionLayerRemove(TcpSession, TcpLayerLabel_PrxSocks4);
        bResult = true;
    };

    if (TcpSessionLayerSearch(TcpSession, TcpLayerLabel_PrxSocks5))
    {
        TcpSessionLayerRemove(TcpSession, TcpLayerLabel_PrxSocks5);
        bResult = true;
    };

    return bResult;
};


bool TcpSessionSecEnable(HOBJ hTcpSession, bool Flag)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    if (TcpSessionIsConnected(TcpSession))
        return false;

    if (Flag)
    {
        if (!TcpSessionLayerSearch(TcpSession, TcpLayerLabel_Ssl))
        {
            TcpSessionLayerRegist(TcpSession, TcpLayerLabel_Ssl);
            return true;
        };
    }
    else
    {
        if (TcpSessionLayerSearch(TcpSession, TcpLayerLabel_Ssl))
        {
            TcpSessionLayerRemove(TcpSession, TcpLayerLabel_Ssl);
            return true;
        };
    };

    return false;
};


bool TcpSessionSecSetHost(HOBJ hTcpSession, const char* Hostname)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    if (TcpSessionIsConnected(TcpSession))
        return false;

    return TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerLabel_Ssl, TcpSslCmd_SetHostname, (void*)Hostname);
};


bool TcpSessionSetTimeoutRead(HOBJ hTcpSession, uint32 Timeout)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    return TcpSessionSendCmd(TcpSession, nullptr, TcpLayerLabel_Transport, TcpTransportCmd_SetTimeoutRead, (void*)Timeout);
};


void TcpSessionSetForceClose(HOBJ hTcpSession, bool Flag)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    if (TcpSessionIsConnected(TcpSession))
        return;

    TcpSessionSendCmd(TcpSession, nullptr, TcpLayerLabel_Transport, TcpTransportCmd_SetForceClose, (void*)Flag);
};


void TcpSessionSetNotifySent(HOBJ hTcpSession, bool Flag)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    if (TcpSessionIsConnected(TcpSession))
        return;

    TcpSession->FlagNotifySent = Flag;
};


bool TcpSessionIsRemote(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    return TcpSession->FlagRemote;
};


bool TcpSessionIsConnected(HOBJ hTcpSession)
{
    bool bIsConnected = false;
    if (TcpSessionSendCmd(hTcpSession, nullptr, TcpLayerLabel_Transport, TcpTransportCmd_CheckOpen, &bIsConnected))
        return bIsConnected;
    else
        return false;
};


void TcpSessionSetUserParam(HOBJ hTcpSession, void* UserParam)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    TcpSession->UserParam = UserParam;
};


void* TcpSessionGetUserParam(HOBJ hTcpSession)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    return TcpSession->UserParam;
};


bool TcpSessionSendEvent(HOBJ hTcpSession, struct TcpLayer_t* Sender, int32 Id, const void* Param)
{
    //
    //  Send event to an next layer before Sender
    //  Each layer should manual resend event to an following via TcpLayerSendEvent
    //  If sender is last layer in chain - routine will dispatch event to an user defined proc
    //  If sender is "nullptr" - routine will dispatch event to an first "bottom" layer in chain
    //

    ASSERT(Sender);
    
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;
    bool bResult = false;
    bool bDispatchToUser = true;
    
    TcpSessionDispatchBegin(TcpSession, TcpSessionDispatch_Event);

    CList<TcpLayer_t>::iterator it(&TcpSession->ListLayer, Sender ? Sender : (TcpLayer_t*)&TcpSession->ListLayer);
    --it;

    while (it != TcpSession->ListLayer.end())
    {
        TcpLayer_t* TcpLayer = &(*it);

        if (TcpLayer->FnEventProc)
        {           
            bResult = TcpLayer->FnEventProc(TcpLayer, Id, Param);
            bDispatchToUser = false;
			break;
        };

        --it;
    };

    if (!bDispatchToUser)
    {
        TcpSessionDispatchEnd(TcpSession, TcpSessionDispatch_Event);
        return bResult;
    };

    NetEvent_t Event = NetEvent_ConnectFail;
    const char* Data = nullptr;
    int32 DataSize = 0;
    uint64 NetAddr = 0;
    uint32 ErrorCode = 0;

    switch (Id)
    {
    case TcpLayerEvent_Connect:
        {
            TcpLayerEventParamConnect_t* EventParam = (TcpLayerEventParamConnect_t*)Param;

            Event = NetEvent_Connect;
            NetAddrInit(&NetAddr, EventParam->Ip, EventParam->Port);

            TcpSession->NetAddr = NetAddr;
        }
        break;

    case TcpLayerEvent_ConnectFail:
        {
            TcpLayerEventParamConnectFail_t* EventParam = (TcpLayerEventParamConnectFail_t*)Param;

            NetAddrInit(&NetAddr, EventParam->Ip, EventParam->Port);
            ErrorCode = EventParam->ErrorCode;
            
            TcpSession->NetAddr = NetAddr;

            switch (EventParam->Label)
            {
            case TcpLayerLabel_Transport:
                Event = NetEvent_ConnectFail;
                break;

            case TcpLayerLabel_Ssl:
                Event = NetEvent_ConnectFailSsl;
                break;

            case TcpLayerLabel_PrxHttp:
            case TcpLayerLabel_PrxSocks4:
			case TcpLayerLabel_PrxSocks4a:
            case TcpLayerLabel_PrxSocks5:
                Event = NetEvent_ConnectFailProxy;
                break;

            default:
                ASSERT(false, "%d", EventParam->Label);
                break;
            };
        }
        break;

    case TcpLayerEvent_Write:
        {
            if (!TcpSession->FlagNotifySent)
            {
                TcpSessionDispatchEnd(TcpSession, TcpSessionDispatch_Event);
                return true;
            };
        }
        break;

    case TcpLayerEvent_Read:
        {
            TcpLayerEventParamRead_t* EventParam = (TcpLayerEventParamRead_t*)Param;

            ErrorCode = EventParam->ErrorCode;
            Data = EventParam->Data;
            DataSize = EventParam->DataSize;
            Event = NetEvent_Recv;
        }
        break;

    case TcpLayerEvent_Disconnect:
        {
            TcpLayerEventParamDisconnect_t* EventParam = (TcpLayerEventParamDisconnect_t*)Param;

            ErrorCode = EventParam->ErrorCode;
            Event = NetEvent_Disconnect;
        }
        break;

    default:
        {
            ASSERT(false);
            TcpSessionDispatchEnd(TcpSession, TcpSessionDispatch_Event);
        }
        return true;
    };

    {
        std::unique_lock<std::recursive_mutex> Lock(TcpSession->EventProcMutex);
        if (TcpSession->EventProc)
            bResult = TcpSession->EventProc(TcpSession, Event, ErrorCode, NetAddr, Data, DataSize, TcpSession->EventProcParam);
        else
            bResult = true;
    }

    TcpSessionDispatchEnd(TcpSession, TcpSessionDispatch_Event);

    return bResult;
};


bool TcpSessionSendCmd(HOBJ hTcpSession, TcpLayer_t* Sender, int32 Id, void* Param)
{
    TcpSessionRefInc(hTcpSession);
    bool bResult = TcpSessionSendCmdEx(hTcpSession, Sender, COMMAND_RECEIVER_ALL, Id, Param);
    TcpSessionRefDec(hTcpSession);
    return bResult;
};


bool TcpSessionSendCmd(HOBJ hTcpSession, TcpLayer_t* Sender, int32 ReceiverLabel, int32 Id, void* Param)
{
    TcpSessionRefInc(hTcpSession);
    bool bResult = TcpSessionSendCmdEx(hTcpSession, Sender, ReceiverLabel, Id, Param);
    TcpSessionRefDec(hTcpSession);
    return bResult;
};


bool TcpSessionSendCmdEx(HOBJ hTcpSession, TcpLayer_t* Sender, int32 ReceiverLabel, int32 Id, void* Param)
{
    //
    //  Send cmd to an next layer following Sender
    //  Each layer should manual resend event to an following via TcpLayerSendCmd
    //  If sender is last layer in chain - routine just return true
    //  If sender is "nullptr" - routine will dispatch event to an first "top" layer in chain
    //
    
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;
    bool bResult = true;

    TcpSessionDispatchBegin(TcpSession, TcpSessionDispatch_Cmd);

	CList<TcpLayer_t>::iterator it(&TcpSession->ListLayer, Sender ? Sender : (TcpLayer_t*)&TcpSession->ListLayer);
    ++it;

    while (it != TcpSession->ListLayer.end())
    {
        TcpLayer_t* TcpLayer = &(*it);
        
        if (!TcpLayer->FnCmdProc)
        {
            ++it;
            continue;
        };

        if (ReceiverLabel == COMMAND_RECEIVER_ALL)
        {
            bResult = TcpLayer->FnCmdProc(TcpLayer, Id, Param);
            break;            
        }
        else
        {
            if (TcpLayer->Label == ReceiverLabel)
            {
                bResult = TcpLayer->FnCmdProc(TcpLayer, Id, Param);                
                break;
            };            
        };

        ++it;
    };

    TcpSessionDispatchEnd(TcpSession, TcpSessionDispatch_Cmd);

    return bResult;
};


void TcpSessionSetEventProc(HOBJ hTcpSession, NetEventProc_t EventProc, void* EventProcParam)
{
    TcpSession_t* TcpSession = (TcpSession_t*)hTcpSession;

    {
        std::unique_lock<std::recursive_mutex> Lock(TcpSession->EventProcMutex);
        TcpSession->EventProc = EventProc;
        TcpSession->EventProcParam = EventProcParam;
    }
};