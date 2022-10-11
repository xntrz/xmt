#pragma once

//
//  Event should go from bottom layer to top.
//  Cmd should go from top layer to bottom.
// 
//  Any of this event or cmd can be modified, splitted(read or write) or 
//  delayed(connect or connect fail) by any layer.
// 

enum TcpLayerEvent_t
{
    TcpLayerEvent_Connect = 0,
    TcpLayerEvent_ConnectFail,
    TcpLayerEvent_Read,
    TcpLayerEvent_Write,
    TcpLayerEvent_Disconnect,
};


struct TcpLayerEventParamConnect_t
{
    uint32 Ip;
    uint16 Port;
    uint32 Elapsed;
    bool FlagRemote;
};


struct TcpLayerEventParamConnectFail_t
{
    uint32 Ip;
    uint16 Port;
    uint32 ErrorCode;
    int32 Label;
};


struct TcpLayerEventParamWrite_t
{
    uint32 ErrorCode;
    const char* Data;
    int32 DataSize;
};


struct TcpLayerEventParamRead_t
{
    uint32 ErrorCode;
    const char* Data;
    int32 DataSize;
    int32 Received;
};


struct TcpLayerEventParamDisconnect_t
{
    uint32 ErrorCode;
    bool FlagRemote;
};


enum TcpLayerCmd_t
{
    //
    //  Cmd range 0-99 is reserved
    //    
    TcpLayerCmd_Connect = 0,
    TcpLayerCmd_Listen,
    TcpLayerCmd_Write,
    TcpLayerCmd_Disconnect,
};


struct TcpLayerCmdParamConnect_t
{
    uint32 Ip;
    uint32 Port;
    uint32 Timeout;
    bool FlagCancel;
};


struct TcpLayerCmdParamListen_t
{
    uint32 Ip;
    uint32 Port;
    bool FlagEnable;
    bool FlagStart;
};


struct TcpLayerCmdParamWrite_t
{
    const char* Data;
    int32 DataSize;
    bool FlagDisconnectOnComplete;
};


struct TcpLayerCmdParamDisconnect_t
{
    ;
};


struct TcpLayer_t : public CListNode<TcpLayer_t>
{
    int32 Label;
    HOBJ hSession;
#ifdef _DEBUG
    uint32 Flags;
#endif
    bool(*FnEventProc)(TcpLayer_t* TcpLayer, int32 Id, const void* Param);
    bool(*FnCmdProc)(TcpLayer_t* TcpLayer, int32 Id, void* Param);
};


HOBJ TcpLayerGetSession(TcpLayer_t* TcpLayer);
int32 TcpLayerGetLabel(TcpLayer_t* TcpLayer);
bool TcpLayerIsRemote(TcpLayer_t* TcpLayer);
bool TcpLayerSendEvent(TcpLayer_t* TcpLayer, int32 Id, const void* Param);
bool TcpLayerSendCmd(TcpLayer_t* TcpLayer, int32 Id, void* Param);
void TcpLayerRefInc(TcpLayer_t* TcpLayer);
void TcpLayerRefDec(TcpLayer_t* TcpLayer);
void TcpLayerSendDisconnect(TcpLayer_t* TcpLayer);