#include "TcpProxy.hpp"
#include "TcpLayer.hpp"
#include "NetProxy.hpp"
#include "NetAddr.hpp"
#include "NetTypes.hpp"

#include "Shared/Common/Time.hpp"


struct TcpProxyDiag_t
{
    uint32 ConnectTime;
    uint32 AuthTime;
};


struct TcpProxy_t
{
    TcpLayer_t Layer;
    TcpProxyDiag_t Diag;
    NetProxy_t Type;
    uint64 NetAddr;
    uint64 NetAddrOrg;
    NetProxyParam_t Param;
    bool(*AuthEventProc)(TcpProxy_t* TcpProxy, int32 Id, const void* Param);
    std::atomic<int32> Step;
    std::atomic<uint32> ErrorCode;
    std::atomic<bool> FlagParamValid;
    std::atomic<bool> FlagAuthComplete;
};


enum TcpProxySocks5Step_t
{
    TcpProxySocks5Step_Hello = 0,
    TcpProxySocks5Step_Auth,
    TcpProxySocks5Step_Connect,
};


static void TcpProxyStartupPerSession(TcpProxy_t* TcpProxy, NetProxy_t Proxytype);
static void TcpProxyCleanupPerSession(TcpProxy_t* TcpProxy);
static void TcpProxyStartupPerConnect(TcpProxy_t* TcpProxy);
static bool TcpProxyDisconnect(TcpProxy_t* TcpProxy);
static bool TcpProxySend(TcpProxy_t* TcpProxy, const char* Data, int32 DataSize);
static bool TcpProxyTrySend(TcpProxy_t* TcpProxy, const char* Data, int32 DataSize);
static bool TcpProxyNotifyComplete(TcpProxy_t* TcpProxy);
static bool TcpProxySendConnectSocks5(TcpProxy_t* TcpProxy);
static bool TcpProxyEventProcHttp(TcpProxy_t* TcpProxy, int32 Id, const void* Param);
static bool TcpProxyEventProcSocks4(TcpProxy_t* TcpProxy, int32 Id, const void* Param);
static bool TcpProxyEventProcSocks5(TcpProxy_t* TcpProxy, int32 Id, const void* Param);
static bool TcpProxyEventProc(TcpLayer_t* TcpLayer, int32 Id, const void* Param);
static bool TcpProxyCmdProc(TcpLayer_t* TcpLayer, int32 Id, void* Param);


static void TcpProxyStartupPerSession(TcpProxy_t* TcpProxy, NetProxy_t Proxytype)
{
    static bool(*const ProxyEventProcArray[])(TcpProxy_t* TcpProxy, int32 Id, const void* Param) =
    {
        TcpProxyEventProcHttp,
        TcpProxyEventProcSocks4,
        TcpProxyEventProcSocks4,
        TcpProxyEventProcSocks5,
    };
    
    static_assert(COUNT_OF(ProxyEventProcArray) == (NetProxy_Socks5 + 1), "update me");
    ASSERT(Proxytype >= 0 && Proxytype < COUNT_OF(ProxyEventProcArray));

    TcpProxy->Layer.data = &TcpProxy->Layer;
    TcpProxy->Layer.FnEventProc = TcpProxyEventProc;
    TcpProxy->Layer.FnCmdProc = TcpProxyCmdProc;
    TcpProxy->Diag = { 0 };
    TcpProxy->Type = Proxytype;
    NetAddrInit(&TcpProxy->NetAddr);
    TcpProxy->Param = { 0 };
    TcpProxy->AuthEventProc = ProxyEventProcArray[Proxytype];
    TcpProxy->Step = -1;
    TcpProxy->FlagParamValid = false;
    TcpProxy->FlagAuthComplete = false;
};


static void TcpProxyCleanupPerSession(TcpProxy_t* TcpProxy)
{
    ;
};


static void TcpProxyStartupPerConnect(TcpProxy_t* TcpProxy)
{
    TcpProxy->FlagAuthComplete = false;
    TcpProxy->Diag.ConnectTime = TimeCurrentTickPrecise();
    TcpProxy->Diag.AuthTime = TimeCurrentTickPrecise();
};


static bool TcpProxyDisconnect(TcpProxy_t* TcpProxy)
{
    TcpLayerCmdParamDisconnect_t Param = {};
    
    return TcpLayerSendCmd(&TcpProxy->Layer, TcpLayerCmd_Disconnect, &Param);
};


static bool TcpProxySend(TcpProxy_t* TcpProxy, const char* Data, int32 DataSize)
{
    TcpLayerCmdParamWrite_t Param = {};
    Param.Data = Data;
    Param.DataSize = DataSize;
    Param.FlagDisconnectOnComplete = false;
    
    return TcpLayerSendCmd(&TcpProxy->Layer, TcpLayerCmd_Write, &Param);
};


static bool TcpProxyTrySend(TcpProxy_t* TcpProxy, const char* Data, int32 DataSize)
{
    bool bResult = TcpProxySend(TcpProxy, Data, DataSize);
    if (!bResult)
        bResult = TcpProxyDisconnect(TcpProxy);

    return bResult;
};


static bool TcpProxyNotifyComplete(TcpProxy_t* TcpProxy)
{
    TcpProxy->Diag.AuthTime = (TimeCurrentTickPrecise() - TcpProxy->Diag.AuthTime);
	TcpProxy->FlagAuthComplete = true;

    TcpLayerEventParamConnect_t Param = {};
    Param.Ip = NetAddrIp(&TcpProxy->NetAddrOrg);
    Param.Port = NetAddrIp(&TcpProxy->NetAddrOrg);
    Param.FlagRemote = TcpLayerIsRemote(&TcpProxy->Layer);

    return TcpLayerSendEvent(&TcpProxy->Layer, TcpLayerEvent_Connect, &Param);
};


static bool TcpProxySendConnectSocks5(TcpProxy_t* TcpProxy)
{
    uint64 NetAddrTarget = TcpProxy->NetAddrOrg;
    NetAddrHton(&NetAddrTarget);

    char Buffer[NETPROXY_BUFF_MAX];
    int32 Written = 0;

    *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_VERSION;
    if (TcpProxy->FlagParamValid)
        *((uint8*)&Buffer[Written++]) = uint8(TcpProxy->Param.Socks5.Command);
    else
        *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_COMMAND_TCP_CONNECTION;
    *((uint8*)&Buffer[Written++]) = 0x00;   // RSV reserved, must be 0x00

    switch (TcpProxy->Param.Socks5.AddrType)
    {
    case NETPROXY_SOCKS5_ADDRTYPE_IPV4:
        {
            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_ADDRTYPE_IPV4;
            *((uint32*)&Buffer[Written]) = NetAddrIp(&NetAddrTarget);
            Written += sizeof(uint32);
        }
        break;
        
    case NETPROXY_SOCKS5_ADDRTYPE_IPV6:
        {
            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_ADDRTYPE_IPV6;
            std::memcpy(
                &Buffer[Written],
                &TcpProxy->Param.Socks5.Addr.IPv6[0],
                sizeof(TcpProxy->Param.Socks5.Addr.IPv6)
            );
            Written += sizeof(TcpProxy->Param.Socks5.Addr.IPv6);
        }
        break;

    case NETPROXY_SOCKS5_ADDRTYPE_DOMAIN:
        {
            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_ADDRTYPE_DOMAIN;
            
            int32 DomainLen = std::strlen(TcpProxy->Param.Socks5.Addr.Domain);
            *((uint8*)&Buffer[Written++]) = uint8(DomainLen);
            std::strcpy(&Buffer[Written], TcpProxy->Param.Socks5.Addr.Domain);
            Written += DomainLen;
        }
        break;

    default:
        {
            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_ADDRTYPE_IPV4;
            *((uint32*)&Buffer[Written]) = NetAddrIp(&NetAddrTarget);
            Written += sizeof(uint32);
        }
        break;
    };
    
    *((uint16*)&Buffer[Written]) = NetAddrPort(&NetAddrTarget);
    Written += sizeof(uint16);

    return TcpProxyTrySend(TcpProxy, Buffer, Written);
};


static bool TcpProxyEventProcHttp(TcpProxy_t* TcpProxy, int32 Id, const void* Param)
{
    bool bResult = false;
    
    switch (Id)
    {
    case TcpLayerEvent_Connect:
        {
            int32 Written = 0;
            char Buffer[NETPROXY_BUFF_MAX];
            Buffer[0] = '\0';

            if (TcpProxy->FlagParamValid)
            {
				//
				//	TODO http auth base64 encode "user:password"
				//

                Written += std::sprintf(
                    Buffer,
                    "CONNECT %s:%hu HTTP/1.1\r\n"
                    "Host: %s:%hu\r\n"
                    "Proxy-Authorization: Basic %s:%s\r\n"
                    "\r\n",
                    NetAddrToStdStringIp(&TcpProxy->NetAddrOrg).c_str(), NetAddrPort(&TcpProxy->NetAddrOrg),
                    NetAddrToStdStringIp(&TcpProxy->NetAddrOrg).c_str(), NetAddrPort(&TcpProxy->NetAddrOrg),
                    TcpProxy->Param.Http.UserId, TcpProxy->Param.Http.UserPw
                );
            }
            else
            {
                Written += std::sprintf(
                    Buffer,
                    "CONNECT %s:%hu HTTP/1.1\r\n"
                    "Host: %s:%hu\r\n"
                    "\r\n",
                    NetAddrToStdStringIp(&TcpProxy->NetAddrOrg).c_str(), NetAddrPort(&TcpProxy->NetAddrOrg),
                    NetAddrToStdStringIp(&TcpProxy->NetAddrOrg).c_str(), NetAddrPort(&TcpProxy->NetAddrOrg)
                );
            };

            bResult = TcpProxyTrySend(TcpProxy, Buffer, Written);
        }
        break;

    case TcpLayerEvent_Read:
        {
            TcpLayerEventParamRead_t* EventParam = (TcpLayerEventParamRead_t*)Param;

            static const char* ResponsePatternArray[] =
            {
                "HTTP/1.1 %d",
                "HTTP/1.0 %d",
                "HTTP/2.0 %d",
            };

            int32 ResponseStatus = -1;

            for (int32 i = 0; i < COUNT_OF(ResponsePatternArray); ++i)
            {
                if (std::sscanf(EventParam->Data, ResponsePatternArray[i], &ResponseStatus) > 0)
                    break;
            };

            switch (ResponseStatus)
            {
            case 201:	// CREATED
            case 202:	// ACCEPTED
            case 203:	// NON-AUTHORATIVE INFORMATION
            case 204:	// NO CONTENT
            case 205:	// RESET CONTENT
            case 206:	// PATRIAL CONTENT
            case 207:	// MULTI-STATUS
            case 208:	// ALREADY REPORTED
            case 226:	// IM USED
            case 200:	// OK
                {
                    bResult = TcpProxyNotifyComplete(TcpProxy);                    
                }
                break;

            default:
                {
                    //
                    //  Auth failed, returning false will automatically disconnect transport
                    //
                    TcpProxy->ErrorCode = uint32(ResponseStatus);
                    bResult = false;
                }
                break;
            };
        }
        break;
    };

    return bResult;
};


static bool TcpProxyEventProcSocks4(TcpProxy_t* TcpProxy, int32 Id, const void* Param)
{
    bool bResult = false;

    switch (Id)
    {
    case TcpLayerEvent_Connect:
        {
            int32 Written = 0;
            char Buffer[NETPROXY_BUFF_MAX];
            
            uint64 NetAddrTarget = TcpProxy->NetAddrOrg;
            
            if ((TcpProxy->Type == NetProxy_Socks4a) &&
                (TcpProxy->Param.Socks4a.Domain[0] != '\0'))
            {
                //
                //  A server using protocol SOCKS4a must check the DSTIP in the request packet. 
                //  If it represents address 0.0.0.x with nonzero x, the server must read in 
                //  the domain name that the client sends in the packet. 
                //  The server should resolve the domain name and make connection to the destination host if it can.
                //
                NetAddrInit(&NetAddrTarget, "0.0.0.255", NetAddrPort(&NetAddrTarget));
                NetAddrHton(&NetAddrTarget);
            }
			else
			{
				NetAddrHton(&NetAddrTarget);
			};

            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS4_VERSION;
            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS4_CMD_CONNECT;
            *((uint16*)&Buffer[Written]) = NetAddrPort(&NetAddrTarget);
            Written += sizeof(uint16);
            *((uint32*)&Buffer[Written]) = NetAddrIp(&NetAddrTarget);
            Written += sizeof(uint32);

            if (TcpProxy->FlagParamValid)
            {
                if (TcpProxy->Param.Socks4.UserId[0] != '\0')
                {
                    int32 IdLen = std::strlen(TcpProxy->Param.Socks4.UserId);
                    std::strncpy(&Buffer[Written], TcpProxy->Param.Socks4.UserId, IdLen);
                    Written += IdLen;                
                };

                *((uint8*)&Buffer[Written++]) = '\0';

                if (TcpProxy->Type == NetProxy_Socks4a)
                {
                    if (TcpProxy->Param.Socks4a.Domain[0] != '\0')
                    {
                        int32 DomainLen = std::strlen(TcpProxy->Param.Socks4a.Domain);
                        std::strncpy(&Buffer[Written], TcpProxy->Param.Socks4a.Domain, DomainLen);
                        Written += DomainLen;                        
                    };
                    
                    *((uint8*)&Buffer[Written++]) = '\0';
                };
            }
            else
            {
                *((uint8*)&Buffer[Written++]) = '\0';
            };
            
            bResult = TcpProxyTrySend(TcpProxy, Buffer, Written);
        }
        break;

    case TcpLayerEvent_Read:
        {
            TcpLayerEventParamRead_t* EventParam = (TcpLayerEventParamRead_t*)Param;
            if (EventParam->DataSize >= 2)
            {
                uint8 Empty = EventParam->Data[0];
                uint8 Code = EventParam->Data[1];

                if (Empty == 0)
                {
                    switch (Code)
                    {
                    case NETPROXY_SOCKS4_STATUS_GRANTED:
                        {
                            bResult = TcpProxyNotifyComplete(TcpProxy);
                        }
                        break;
                    };
                };
            };
        }
        break;
    };

    return bResult;
};


static bool TcpProxyEventProcSocks5(TcpProxy_t* TcpProxy, int32 Id, const void* Param)
{
    bool bResult = false;

    switch (Id)
    {
    case TcpLayerEvent_Connect:
        {
            int32 Written = 0;
            char Buffer[NETPROXY_BUFF_MAX];

            *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_VERSION;
        
            if (TcpProxy->FlagParamValid)
            {
                *((uint8*)&Buffer[Written++]) = TcpProxy->Param.Socks5.AuthtypeNum;

                for (int32 i = 0; i < TcpProxy->Param.Socks5.AuthtypeNum; ++i)
                    *((uint8*)&Buffer[Written++]) = uint8(TcpProxy->Param.Socks5.Authtype[i]);
            }
            else
            {
                //
                //  If not parameter present, assume there is 1 method with no auth type
                //
                *((uint8*)&Buffer[Written++]) = 1;
                *((uint8*)&Buffer[Written++]) = NETPROXY_SOCKS5_AUTHTYPE_NO_AUTH;
            };

            bResult = TcpProxyTrySend(TcpProxy, Buffer, Written);
            if (bResult)
                TcpProxy->Step = TcpProxySocks5Step_Hello;
        }
        break;

    case TcpLayerEvent_Read:
        {
            TcpLayerEventParamRead_t* EventParam = (TcpLayerEventParamRead_t*)Param;
            if (EventParam->DataSize < 2)
            {
                bResult = false;
                OUTPUTLN("socks5 got read that less than 2 bytes");
                break;
            };

            switch (TcpProxy->Step)
            {
            case TcpProxySocks5Step_Hello:
                {
                    uint8 Ver = *((uint8*)&EventParam->Data[0]);
                    uint8 Cauth = *((uint8*)&EventParam->Data[1]);

                    switch (Cauth)
                    {
                    case NETPROXY_SOCKS5_AUTHTYPE_NO_AUTH:
                        {
                            TcpProxy->Step = TcpProxySocks5Step_Connect;
                            bResult = TcpProxySendConnectSocks5(TcpProxy);
                        }
                        break;

                    case NETPROXY_SOCKS5_AUTHTYPE_USERNAME_PASSWORD:
                        {
                            if (TcpProxy->FlagParamValid)
                            {
                                TcpProxy->Step = TcpProxySocks5Step_Auth;

                                char Buffer[NETPROXY_BUFF_MAX];
                                int32 Written = 0;

                                int32 IdLen = std::strlen(TcpProxy->Param.Socks5.UserId);
                                int32 PwLen = std::strlen(TcpProxy->Param.Socks5.UserPw);

                                *((uint8*)&Buffer[Written++]) = 0x01;

                                *((uint8*)&Buffer[Written++]) = uint8(IdLen);
                                std::strncpy(&Buffer[Written], TcpProxy->Param.Socks5.UserId, IdLen);
                                Written += IdLen;

                                *((uint8*)&Buffer[Written++]) = uint8(PwLen);
                                std::strncpy(&Buffer[Written], TcpProxy->Param.Socks5.UserPw, PwLen);
                                Written += PwLen;

                                bResult = TcpProxyTrySend(TcpProxy, Buffer, Written);
                            };
                        }
                        break;

                    case 0xFF:
                        {
                            //
                            //  No methods
                            //
                            bResult = false;
                        }
                        break;

                    case NETPROXY_SOCKS5_AUTHTYPE_GSSAPI:
                        OUTPUTLN("todo: authtype GSSAPI");
                    default:
						bResult = false;
                        break;
                    };
                }
                break;

            case TcpProxySocks5Step_Auth:
                {
                    ASSERT(TcpProxy->FlagParamValid == true);

                    uint8 Ver = *((uint8*)&EventParam->Data[0]);
                    uint8 Status = *((uint8*)&EventParam->Data[1]);

                    switch (Status)
                    {
                    case 0x00:
                        {
                            TcpProxy->Step = TcpProxySocks5Step_Connect;
                            bResult = TcpProxySendConnectSocks5(TcpProxy);
                        }
                        break;

                    default:
                        {
                            bResult = false;
                        }
                        break;
                    };
                }
                break;

            case TcpProxySocks5Step_Connect:
                {
                    uint8 Ver = *((uint8*)&EventParam->Data[0]);
                    uint8 Status = *((uint8*)&EventParam->Data[1]);

                    switch (Status)
                    {
                    case NETPROXY_SOCKS5_STATUS_GRANTED:
                        {
                            bResult = TcpProxyNotifyComplete(TcpProxy);
                        }
                        break;

                    case NETPROXY_SOCKS5_STATUS_FAILURE:
                    case NETPROXY_SOCKS5_STATUS_NOT_ALLOWED:
                    case NETPROXY_SOCKS5_STATUS_NETWORK_UNREACHABLE:
                    case NETPROXY_SOCKS5_STATUS_HOST_UNREACHABLE:
                    case NETPROXY_SOCKS5_STATUS_REFUSED:
                    case NETPROXY_SOCKS5_STATUS_TTL_EXPIRED:
                    case NETPROXY_SOCKS5_STATUS_CMD_NOT_SUPPORTED:
                    case NETPROXY_SOCKS5_STATUS_ADDR_NOT_SUPPORTED:
                    default:
                        {
                            TcpProxy->ErrorCode = uint32(Status);
                            bResult = false;
                        }
                        break;
                    };
                }
                break;

            default:
                ASSERT(false);
                break;
            };
        }
        break;
    };

    return bResult;
};


static bool TcpProxyEventProc(TcpLayer_t* TcpLayer, int32 Id, const void* Param)
{
    TcpProxy_t* TcpProxy = (TcpProxy_t*)TcpLayer;
    if (TcpProxy->FlagAuthComplete)
    {
        return TcpLayerSendEvent(TcpLayer, Id, Param);
    }
    else
    {
        bool bResult = false;

        switch (Id)
        {
        case TcpLayerEvent_ConnectFail:
            {
                //
                //  Since we modifying connection address we should correct it if fail during transport call
                //
                TcpLayerEventParamConnectFail_t* EventParam = (TcpLayerEventParamConnectFail_t*)Param;
                EventParam->Ip = NetAddrIp(&TcpProxy->NetAddrOrg);
                EventParam->Port = NetAddrPort(&TcpProxy->NetAddrOrg);

                bResult = TcpLayerSendEvent(TcpLayer, TcpLayerEvent_ConnectFail, EventParam);
            }
            break;

        case TcpLayerEvent_Disconnect:
            {
                //
                //  Got dc but auth is not completed
                //  Morph disconnect event to connect fail event
                //
                TcpLayerEventParamConnectFail_t EventParam;
                EventParam.Ip = NetAddrIp(&TcpProxy->NetAddrOrg);
                EventParam.Port = NetAddrPort(&TcpProxy->NetAddrOrg);
                EventParam.Label = TcpLayerGetLabel(TcpLayer);
                EventParam.ErrorCode = TcpProxy->ErrorCode;

                bResult = TcpLayerSendEvent(TcpLayer, TcpLayerEvent_ConnectFail, &EventParam);
            }
            break;

        default:
            {
                if (Id == TcpLayerEvent_Connect)
                    TcpProxy->Diag.ConnectTime = (TimeCurrentTickPrecise() - TcpProxy->Diag.ConnectTime);

                //
                //  Dispatch read/connect event in specific proxy type proc
                //
                bResult = TcpProxy->AuthEventProc(TcpProxy, Id, Param);
            }
            break;
        };

        return bResult;
    };
};


static bool TcpProxyCmdProc(TcpLayer_t* TcpLayer, int32 Id, void* Param)
{
    TcpProxy_t* TcpProxy = (TcpProxy_t*)TcpLayer;
    bool bResult = false;

    switch (Id)
    {
    case TcpLayerCmd_Connect:
        {
            TcpProxyStartupPerConnect(TcpProxy);

            //
            //  Catch up connect cmd save org address and modify to proxy addr
            //            
            TcpLayerCmdParamConnect_t* CmdParam = (TcpLayerCmdParamConnect_t*)Param;
            
            NetAddrInit(&TcpProxy->NetAddrOrg, CmdParam->Ip, CmdParam->Port);
            
            CmdParam->Ip = NetAddrIp(&TcpProxy->NetAddr);
            CmdParam->Port = NetAddrPort(&TcpProxy->NetAddr);

            bResult = TcpLayerSendCmd(TcpLayer, Id, CmdParam);
        }
        break;

    case TcpProxyCmd_SetParam:
        {
            TcpProxySetParam_t* CmdParam = (TcpProxySetParam_t*)Param;

            TcpProxy->NetAddr = CmdParam->NetAddr;
            TcpProxy->FlagParamValid = (CmdParam->Parameter != nullptr);

            if(TcpProxy->FlagParamValid)
                std::memcpy(&TcpProxy->Param, CmdParam->Parameter, sizeof(TcpProxy->Param));

            bResult = true;
        }
        break;

    case TcpProxyCmd_GetDiag:
        {
            TcpProxyDiagParam_t* CmdParam = (TcpProxyDiagParam_t*)Param;

            CmdParam->ConnectTime = TcpProxy->Diag.ConnectTime;
            CmdParam->AuthTime = TcpProxy->Diag.AuthTime;
            
            bResult = true;
        }
        break;

    default:
        {
            bResult = TcpLayerSendCmd(TcpLayer, Id, Param);
        }
        break;
    };

    return bResult;
};


struct TcpLayer_t* TcpProxyCreate(void* Param)
{
    TcpProxy_t* TcpProxy = new TcpProxy_t();
    if (!TcpProxy)
        return 0;

    TcpProxyStartupPerSession(TcpProxy, NetProxy_t(int32(Param)));

    return &TcpProxy->Layer;
};


void TcpProxyDestroy(struct TcpLayer_t* TcpLayer)
{
    TcpProxy_t* TcpProxy = (TcpProxy_t*)TcpLayer;

    TcpProxyCleanupPerSession(TcpProxy);

    delete TcpProxy;
};