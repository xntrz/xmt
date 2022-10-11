#pragma once

#include "NetTypes.hpp"
#include "NetProxy.hpp"
#include "NetAddr.hpp"


DLLSHARED bool NetInitialize(void);
DLLSHARED void NetTerminate(void);
DLLSHARED void NetTcpSetNotifySent(HOBJ hConn, bool Flag);
DLLSHARED void NetTcpSetForceClose(HOBJ hConn, bool Flag);
DLLSHARED bool NetTcpSetTimeoutRead(HOBJ hConn, uint32 Timeout);
DLLSHARED void NetTcpSetUserParam(HOBJ hConn, void* userparam);
DLLSHARED void* NetTcpGetUserParam(HOBJ hConn);
DLLSHARED bool NetTcpSetProxy(HOBJ hConn, NetProxy_t ProxyType, uint64 NetAddr, void* Parameter = nullptr, int32 ParameterLen = 0);
DLLSHARED bool NetTcpClearProxy(HOBJ hConn);
DLLSHARED bool NetTcpSetSecure(HOBJ hConn, bool Flag);
DLLSHARED bool NetTcpSetSecureHost(HOBJ hConn, const char* Hostname);
DLLSHARED HOBJ NetTcpOpen(NetEventProc_t EventProc, void* Param = nullptr);
DLLSHARED void NetTcpClose(HOBJ hConn);
DLLSHARED HOBJ NetTcpCopy(HOBJ hConn);
DLLSHARED bool NetTcpConnect(HOBJ hConn, uint64 NetAddr, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnect(HOBJ hConn, uint32 Ip, uint16 Port, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnect(HOBJ hConn, const char* Hostname, uint16 Port, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnectSelf(HOBJ hConn, uint16 Port, uint32 Timeout = 0);
DLLSHARED bool NetTcpCancelConnect(HOBJ hConn);
DLLSHARED bool NetTcpListen(HOBJ hConn, uint64 NetAddr);
DLLSHARED bool NetTcpListen(HOBJ hConn, uint32 Ip, uint16 Port);
DLLSHARED bool NetTcpListen(HOBJ hConn, const char* Ip, uint16 Port);
DLLSHARED bool NetTcpListenSelf(HOBJ hConn, uint16 Port);
DLLSHARED bool NetTcpListenEnable(HOBJ hConn, bool Flag);
DLLSHARED bool NetTcpSend(HOBJ hConn, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete = false);
DLLSHARED bool NetTcpDisconnect(HOBJ hConn);
DLLSHARED bool NetGetAddrinfo(const char* Hostname, uint32* IpAddrArray, int32* IpAddrArrayCount);
DLLSHARED bool NetGetAddrinfo(const char* Hostname, uint32* Ip);
DLLSHARED bool NetGetAddrinfoSelf(uint32* IpAddrArray, int32* IpAddrArrayCount);