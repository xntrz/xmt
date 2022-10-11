#include "TcpNetwork.hpp"
#include "TcpSession.hpp"
#include "TcpIoSvc.hpp"
#include "TcpSockPool.hpp"
#include "TcpTransportTmout.hpp"
#include "TcpTransportAct.hpp"
#include "TcpStats.hpp"


void TcpNetInitialize(void)
{
    TcpStatsInitialize();
    TcpSockPoolInitialize();
    TcpActInitialize();
    TcpTmoutInitialize();
    TcpIoSvcInitialize();
};


void TcpNetTerminate(void)
{
    TcpIoSvcWaitForActEnd();
    TcpIoSvcTerminate();
    TcpTmoutTerminate();
    TcpActTerminate();
    TcpSockPoolTerminate();
    TcpStatsTerminate();
};


HOBJ TcpNetOpen(NetEventProc_t UserProc, void* UserParam)
{
    return TcpSessionOpen(UserProc, UserParam);
};


void TcpNetClose(HOBJ hConn)
{
    TcpSessionClose(hConn);
};


HOBJ TcpNetCopy(HOBJ hConn)
{
    TcpSessionCopy(hConn);
	return hConn;
};


bool TcpNetConnect(HOBJ hConn, uint32 Ip, uint16 Port, uint32 Timeout)
{
    return TcpSessionConnect(hConn, Ip, Port, Timeout);
};


bool TcpNetCancelConnect(HOBJ hConn)
{
    return TcpSessionCancelConnect(hConn);
};


bool TcpNetListen(HOBJ hConn, uint32 Ip, uint16 Port)
{
    return TcpSessionListen(hConn, Ip, Port);
};


bool TcpNetListenEnable(HOBJ hConn, bool Flag)
{
    return TcpSessionListenEnable(hConn, Flag);
};


bool TcpNetWrite(HOBJ hConn, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete)
{
    return TcpSessionWrite(hConn, Data, DataSize, FlagDisconnectOnComplete);
};


bool TcpNetDisconnect(HOBJ hConn)
{
    return TcpSessionDisconnect(hConn);
};


bool TcpNetSetProxy(HOBJ hConn, NetProxy_t Proxytype, uint64 NetAddr, const void* Parameter, int32 ParameterLen)
{
    return TcpSessionProxySet(hConn, Proxytype, NetAddr, Parameter, ParameterLen);
};


bool TcpNetClearProxy(HOBJ hConn)
{
    return TcpSessionProxyClear(hConn);
};


bool TcpNetSetSecurity(HOBJ hConn, bool Flag)
{
    return TcpSessionSecEnable(hConn, Flag);
};


bool TcpNetSetSecurityHost(HOBJ hConn, const char* Hostname)
{
    return TcpSessionSecSetHost(hConn, Hostname);
};


bool TcpNetSetTimeoutRead(HOBJ hConn, uint32 Timeout)
{
    return TcpSessionSetTimeoutRead(hConn, Timeout);
};


void TcpNetSetForceClose(HOBJ hConn, bool Flag)
{
    TcpSessionSetForceClose(hConn, Flag);
};


void TcpNetSetNotifySent(HOBJ hConn, bool Flag)
{
    TcpSessionSetNotifySent(hConn, Flag);
};


void TcpNetSetUserParam(HOBJ hConn, void* UserParam)
{
    TcpSessionSetUserParam(hConn, UserParam);
};


void* TcpNetGetUserParam(HOBJ hConn)
{
    return TcpSessionGetUserParam(hConn);
};

