#pragma once

enum TcpTransportCmd_t
{
    TcpTransportCmd_CheckOpen = 100,    // Param is a pointer to bool
    TcpTransportCmd_CompleteRemoteConn, // Param is a RemoteSocket
    TcpTransportCmd_UserClose,          // Param is nullptr
    TcpTransportCmd_SetTimeoutRead,     // Param is uint32 timeout value
    TcpTransportCmd_SetForceClose,      // Param is bool value
};

struct TcpLayer_t* TcpTransportCreate(void* Param);
void TcpTransportDestroy(struct TcpLayer_t* TcpLayer);