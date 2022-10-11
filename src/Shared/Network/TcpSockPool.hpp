#pragma once

#include "NetTypedefs.hpp"


void TcpSockPoolInitialize(void);
void TcpSockPoolTerminate(void);
SOCKET TcpSockPoolAllocBound(void);
SOCKET TcpSockPoolAllocUnbound(void);
void TcpSockPoolFreeBound(SOCKET hSock);
void TcpSockPoolFreeUnbound(SOCKET hSock);