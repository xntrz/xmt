#pragma once

#include "NetTypes.hpp"
#include "NetProxy.hpp"


HOBJ TcpSessionOpenRemote(HOBJ hTcpSessionHost, uint64 NetAddr, void* Param = nullptr);
HOBJ TcpSessionOpen(NetEventProc_t EventProc, void* EventProcParam);
void TcpSessionClose(HOBJ hTcpSession);
void TcpSessionCopy(HOBJ hTcpSession);
void TcpSessionRefInc(HOBJ hTcpSession);
void TcpSessionRefDec(HOBJ hTcpSession);
bool TcpSessionConnect(HOBJ hTcpSession, uint32 Ip, uint16 Port, uint32 Timeout);
bool TcpSessionCancelConnect(HOBJ hTcpSession);
bool TcpSessionListen(HOBJ hTcpSession, uint32 Ip, uint16 Port);
bool TcpSessionListenEnable(HOBJ hTcpSession, bool Flag);
bool TcpSessionWrite(HOBJ hTcpSession, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete);
bool TcpSessionDisconnect(HOBJ hTcpSession);
bool TcpSessionProxySet(HOBJ hTcpSession, NetProxy_t Proxytype, uint64 NetAddr, const void* Parameter, int32 ParameterLen);
bool TcpSessionProxyClear(HOBJ hTcpSession);
bool TcpSessionSecEnable(HOBJ hTcpSession, bool Flag);
bool TcpSessionSecSetHost(HOBJ hTcpSession, const char* Hostname);
bool TcpSessionSetTimeoutRead(HOBJ hTcpSession, uint32 Timeout);
void TcpSessionSetForceClose(HOBJ hTcpSession, bool Flag);
void TcpSessionSetNotifySent(HOBJ hTcpSession, bool Flag);
bool TcpSessionIsRemote(HOBJ hTcpSession);
bool TcpSessionIsConnected(HOBJ hTcpSession);
void TcpSessionSetUserParam(HOBJ hTcpSession, void* UserParam);
void* TcpSessionGetUserParam(HOBJ hTcpSession);
bool TcpSessionSendEvent(HOBJ hTcpSession, struct TcpLayer_t* Sender, int32 Id, const void* Param = nullptr);
bool TcpSessionSendCmd(HOBJ hTcpSession, struct TcpLayer_t* Sender, int32 Id, void* Param = nullptr);
bool TcpSessionSendCmd(HOBJ hTcpSession, struct TcpLayer_t* Sender, int32 ReceiverLabel, int32 Id, void* Param = nullptr);
bool TcpSessionSendCmdEx(HOBJ hTcpSession, struct TcpLayer_t* Sender, int32 ReceiverLabel, int32 Id, void* Param = nullptr);
void TcpSessionSetEventProc(HOBJ hTcpSession, NetEventProc_t EventProc, void* EventProcParam = nullptr);