#pragma once

#include "NetTypes.hpp"
#include "NetProxy.hpp"


void TcpNetInitialize(void);
void TcpNetTerminate(void);
HOBJ TcpNetOpen(NetEventProc_t UserProc, void* UserParam);
void TcpNetClose(HOBJ hConn);
HOBJ TcpNetCopy(HOBJ hConn);
bool TcpNetConnect(HOBJ hConn, uint32 Ip, uint16 Port, uint32 Timeout);
bool TcpNetCancelConnect(HOBJ hConn);
bool TcpNetListen(HOBJ hConn, uint32 Ip, uint16 Port);
bool TcpNetListenEnable(HOBJ hConn, bool Flag);
bool TcpNetWrite(HOBJ hConn, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete);
bool TcpNetDisconnect(HOBJ hConn);
bool TcpNetSetProxy(HOBJ hConn, NetProxy_t Proxytype, uint64 NetAddr, const void* Parameter = nullptr, int32 ParameterLen = 0);
bool TcpNetClearProxy(HOBJ hConn);
bool TcpNetSetSecurity(HOBJ hConn, bool Flag);
bool TcpNetSetSecurityHost(HOBJ hConn, const char* Hostname);
bool TcpNetSetTimeoutRead(HOBJ hConn, uint32 Timeout);
void TcpNetSetForceClose(HOBJ hConn, bool Flag);
void TcpNetSetNotifySent(HOBJ hConn, bool Flag);
void TcpNetSetUserParam(HOBJ hConn, void* UserParam);
void* TcpNetGetUserParam(HOBJ hConn);