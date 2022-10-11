#pragma once

#include "NetTypedefs.hpp"

extern LPFN_ACCEPTEX NetWsaFnAcceptEx;
extern LPFN_CONNECTEX NetWsaFnConnectEx;
extern LPFN_DISCONNECTEX NetWsaFnDisconnectEx;
extern LPFN_GETACCEPTEXSOCKADDRS NetWsaFnGetAcceptExSockAddrs;


void NetWsaInitialize(void);
void NetWsaTerminate(void);
bool NetWsaStart(void);
void NetWsaStop(void);
SOCKET NetSockNewListen(uint64 NetAddr);
SOCKET NetSockNewAccept(void);
SOCKET NetSockNewConnect(void);
void NetSockClose(SOCKET hSock);
bool NetSockSetNonblock(SOCKET hSock);
bool NetSockSetNoDelay(SOCKET hSock, bool Flag);
bool NetSockSetReuseAddr(SOCKET hSock, bool Flag);
bool NetSockSetPortScale(SOCKET hSock, bool Flag);
bool NetSockSetBuffer(SOCKET hSock, uint32 SizeBuffRecv, uint32 SizeBuffSend);
bool NetSockUpdAcceptCtx(SOCKET hSockListen, SOCKET hSockAccept);
bool NetSockUpdConnectCtx(SOCKET hSock);
bool NetSockSetLinger(SOCKET hSock, bool Flag, uint16 Seconds = 0);
HOBJ NetSockPoolCreate(int32 Capacity);
void NetSockPoolDestroy(HOBJ hSockPool);
SOCKET NetSockPoolAlloc(HOBJ hSockPool);
void NetSockPoolFree(HOBJ hSockPool, SOCKET hSock);
bool NetSockPoolIsFull(HOBJ hSockPool);
bool NetDnsResolve(const char* Hostname, uint32* AddrArray, int32* AddrArrayCount);