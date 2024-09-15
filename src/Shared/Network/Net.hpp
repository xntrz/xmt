#pragma once

#include "NetTypes.hpp"
#include "NetProxy.hpp"
#include "NetAddr.hpp"


bool NetInitialize(void);
void NetTerminate(void);
DLLSHARED HCONN NetTcpOpen(NETEVENTPROC EventProc = nullptr, void* Param = nullptr);
DLLSHARED void NetTcpClose(HCONN hConn);
DLLSHARED HCONN NetTcpCopy(HCONN hConn);
DLLSHARED HCONN NetTcpAccept(HCONN hConn);
DLLSHARED bool NetTcpConnect(HCONN hConn, uint64 NetAddr, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnect(HCONN hConn, uint32 Ip, uint16 Port, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnect(HCONN hConn, const char* Hostname, uint16 Port, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnect(HCONN hConn, const char* Url, uint32 Timeout = 0);
DLLSHARED bool NetTcpConnectSelf(HCONN hConn, uint16 Port, uint32 Timeout = 0);
DLLSHARED bool NetTcpCancelConnect(HCONN hConn);
DLLSHARED bool NetTcpListen(HCONN hConn, int32 Backlog, uint64 NetAddr);
DLLSHARED bool NetTcpListen(HCONN hConn, int32 Backlog, uint32 Ip, uint16 Port);
DLLSHARED bool NetTcpListen(HCONN hConn, int32 Backlog, const char* Ip, uint16 Port);
DLLSHARED bool NetTcpListenSelf(HCONN hConn, int32 Backlog, uint16 Port);
DLLSHARED uint32 NetTcpRecv(HCONN hConn, void* Buff, uint32 BuffSize);
DLLSHARED uint32 NetTcpSend(HCONN hConn, const void* Data, uint32 DataSize, bool FlagDisconnectOnComplete = false);
DLLSHARED bool NetTcpDisconnect(HCONN hConn);
DLLSHARED bool NetTcpResolve(HCONN hConn, const char* Hostname, uint32 Timeout = 0);
DLLSHARED bool NetTcpCancelResolve(HCONN hConn);
DLLSHARED void NetTcpSetForceClose(HCONN hConn, bool Flag);
DLLSHARED void NetTcpSetTimeoutRead(HCONN hConn, uint32 Timeout);
DLLSHARED void NetTcpSetUserParam(HCONN hConn, void* userparam);
DLLSHARED void* NetTcpGetUserParam(HCONN hConn);
DLLSHARED void NetTcpSetProxy(HCONN hConn, NETPROXY ProxyType, uint64 NetAddr, const NETPROXYPARAM* Parameter);
DLLSHARED void NetTcpClearProxy(HCONN hConn);
DLLSHARED void NetTcpSetSecure(HCONN hConn, bool Flag);
DLLSHARED void NetTcpSetSecureHost(HCONN hConn, const char* Hostname);
DLLSHARED bool NetTcpIsRemote(HCONN hConn);
DLLSHARED bool NetTcpIsConnected(HCONN hConn);
DLLSHARED uint64 NetTcpNetAddr(HCONN hConn);
DLLSHARED void NetTcpSetEventProc(HCONN hConn, NETEVENTPROC EventProc, void* Param = nullptr);
DLLSHARED void NetTcpGetEventProc(HCONN hConn, NETEVENTPROC* EventProc, void** Param = nullptr);
DLLSHARED bool NetGetAddrinfo(const char* Hostname, uint32* IpAddrArray, int32* IpAddrArrayCount);
DLLSHARED bool NetGetAddrinfo(const char* Hostname, uint32* Ip);
DLLSHARED bool NetGetAddrinfoSelf(uint32* IpAddrArray, int32* IpAddrArrayCount);