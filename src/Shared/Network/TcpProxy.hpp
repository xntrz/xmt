#pragma once

struct TcpProxySetParam_t
{
    uint64 NetAddr;
    const void* Parameter;
    int32 ParemeterLen;
};

struct TcpProxyDiagParam_t
{
    uint32 ConnectTime;
    uint32 AuthTime;
};

enum TcpProxyCmd_t
{
    TcpProxyCmd_SetParam = 100, // Param is a pointer to an TcpProxySetParam_t
    TcpProxyCmd_GetDiag,        // Param is a pointer to an TcpPRoxyDiagParam_t
};

struct TcpLayer_t* TcpProxyCreate(void* Param);
void TcpProxyDestroy(struct TcpLayer_t* TcpLayer);