#pragma once

#include "TcpResolverCache.hpp"

#include "../NetTypes.hpp"
#include "../NetProxy.hpp"

#include "../../Common/IoService.hpp"


class CTcpSession;


class CTcpNetwork
{
public:
    CTcpNetwork();
    ~CTcpNetwork();
    HCONN Open(NETEVENTPROC EventProc, void* UserParam = nullptr);
    void Close(HCONN hConn);
    HCONN Copy(HCONN hConn);
    HCONN Accept(HCONN hConn);
    bool Connect(HCONN hConn, uint32 Ip, uint16 Port, uint32 Timeout);
    bool CancelConnect(HCONN hConn);
    bool Listen(HCONN hConn, int32 Backlog, uint32 Ip, uint16 Port);
    uint32 Read(HCONN hConn, void* Buff, uint32 BuffSize);
    uint32 Write(HCONN hConn, const void* Data, uint32 DataSize, bool FlagDisconnectOnComplete);
    bool Disconnect(HCONN hConn);
    bool Resolve(HCONN hConn, const char* Hostname, uint32 Timeout);
    bool CancelResolve(HCONN hConn);
    void SetProxy(HCONN hConn, NETPROXY Proxytype, uint64 NetAddr, const NETPROXYPARAM* Parameter);
    void ClearProxy(HCONN hConn);
    void SetSecurity(HCONN hConn, bool Flag);
    void SetSecurityHost(HCONN hConn, const char* Hostname);
    void SetTimeoutRead(HCONN hConn, uint32 Timeout);
    void SetForceClose(HCONN hConn, bool Flag);
    void SetUserParam(HCONN hConn, void* UserParam);
    void* GetUserParam(HCONN hConn);
    bool IsRemote(HCONN hConn);
    bool IsConnected(HCONN hConn);
    uint64 NetAddr(HCONN hConn);
    void SetEventProc(HCONN hConn, NETEVENTPROC EventProc, void* Param);
    void GetEventProc(HCONN hConn, NETEVENTPROC* EventProc, void** Param);
    bool GetAddrinfo(const char* Hostname, uint32* IpAddrArray, int32* IpAddrArrayCount);
    bool GetAddrinfo(const char* Hostname, uint32* Ip);
    bool GetAddrinfoSelf(uint32* IpAddrArray, int32* IpAddrArrayCount);
    
    /*inline*/ CTcpSession& Session(HCONN hConn);
    
private:
    CIoService m_iosvc;
    CTcpResolverCache m_resolverCache;
};

inline CTcpSession& CTcpNetwork::Session(HCONN hConn) {
    return *reinterpret_cast<CTcpSession*>(hConn);
};