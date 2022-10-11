#pragma once

enum NetEvent_t
{
    NetEvent_Connect = 0,
    NetEvent_ConnectFail,
    NetEvent_ConnectFailProxy,
    NetEvent_ConnectFailSsl,
    NetEvent_Recv,
    NetEvent_Disconnect,
    NetEvent_ResolveAddr,
};

typedef bool(*NetEventProc_t)(
    HOBJ        hConn,
    NetEvent_t  Event,
    uint32      ErrorCode,
    uint64      NetAddr,
    const char* Data,
    uint32      DataSize,
    void*       Param
);